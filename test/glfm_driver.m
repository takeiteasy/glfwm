//========================================================================
// glfwm - macOS host input test driver (test/glfm_driver.m)
//------------------------------------------------------------------------
// Posts synthetic mouse/keyboard/scroll events at a window owned by the
// given process, used by test/glfm_input.sh to exercise the GLFM backend's
// event mapping (M2) on the macOS host (GLFM runs natively there).
//
// usage: glfm_driver <owner-process-name> <input|key-v|key-c|esc>
//
// Requires Accessibility permission (TCC) for the calling terminal, since
// events are injected into the system event stream via CGEventPost.
//
// Phases:
//   input - moves, left/right clicks, line + pixel scroll, types "hi"
//   key-v - the V key (clipboard read in hello)
//   key-c - the C key (clipboard set in hello)
//   esc   - the Escape key (closes the hello window)
//========================================================================

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <unistd.h>

#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <limits.h>

// Finds the lowest-layer on-screen window owned by `ownerName` and returns
// its bounds/PID. Window names require screen recording permission, so
// matching is done by owner process name only.
static int findWindow(const char* ownerName, CGRect* outBounds, pid_t* outPid)
{
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly,
                                                 kCGNullWindowID);
    if (!list)
        return 0;

    int found = 0;
    int bestLayer = INT_MAX;

    for (CFIndex i = 0; i < CFArrayGetCount(list); i++)
    {
        CFDictionaryRef info = (CFDictionaryRef) CFArrayGetValueAtIndex(list, i);
        CFStringRef ownerRef = (CFStringRef)
            CFDictionaryGetValue(info, kCGWindowOwnerName);
        if (!ownerRef)
            continue;

        char owner[256];
        if (!CFStringGetCString(ownerRef, owner, sizeof(owner), kCFStringEncodingUTF8) ||
            strcmp(owner, ownerName) != 0)
            continue;

        int layer = 0;
        CFNumberRef layerRef = (CFNumberRef) CFDictionaryGetValue(info, kCGWindowLayer);
        if (layerRef)
            CFNumberGetValue(layerRef, kCFNumberIntType, &layer);

        CFDictionaryRef boundsRef = (CFDictionaryRef)
            CFDictionaryGetValue(info, kCGWindowBounds);
        CGRect bounds = CGRectZero;
        if (boundsRef)
            CGRectMakeWithDictionaryRepresentation(boundsRef, &bounds);
        if (CGRectIsEmpty(bounds))
            continue;

        if (found && layer >= bestLayer)
            continue;
        bestLayer = layer;
        *outBounds = bounds;

        CFNumberRef pidRef = (CFNumberRef) CFDictionaryGetValue(info, kCGWindowOwnerPID);
        if (pidRef)
            CFNumberGetValue(pidRef, kCFNumberIntType, outPid);
        found = 1;
    }

    CFRelease(list);
    return found;
}

static void postMouseEvent(CGEventType type, CGPoint point, CGMouseButton button)
{
    CGEventRef event = CGEventCreateMouseEvent(NULL, type, point, button);
    if (event)
    {
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
    usleep(50 * 1000);
}

static void postScroll(CGScrollEventUnit unit, int32_t delta)
{
    CGEventRef event = CGEventCreateScrollWheelEvent(NULL, unit, 1, delta);
    if (event)
    {
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
    usleep(100 * 1000);
}

static void postKey(unsigned short keyCode)
{
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, keyCode, true);
    CGEventRef up = CGEventCreateKeyboardEvent(NULL, keyCode, false);
    if (down)
    {
        CGEventPost(kCGHIDEventTap, down);
        CFRelease(down);
    }
    usleep(40 * 1000);
    if (up)
    {
        CGEventPost(kCGHIDEventTap, up);
        CFRelease(up);
    }
    usleep(40 * 1000);
}

// HIToolbox key codes
enum
{
    GKF_KEY_C      = 0x08,
    GKF_KEY_H      = 0x04,
    GKF_KEY_V      = 0x09,
    GKF_KEY_I      = 0x22,
    GKF_KEY_ESCAPE = 0x35,
};

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        if (argc < 3)
        {
            fprintf(stderr, "usage: glfm_driver <owner-process-name> <input|key-v|key-c|esc>\n");
            return 2;
        }
        const char* owner = argv[1];
        const char* phase = argv[2];

        CGRect bounds = CGRectZero;
        pid_t pid = 0;
        if (!findWindow(owner, &bounds, &pid))
        {
            fprintf(stderr, "glfm_driver: no on-screen window owned by '%s'\n", owner);
            return 1;
        }

        // Bring the owning process to front so clicks and keys land on it
        // (the first posted click also activates the window regardless).
        NSRunningApplication* app =
            [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
        [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        usleep(300 * 1000);

        CGPoint center = CGPointMake(bounds.origin.x + bounds.size.width / 2.0,
                                     bounds.origin.y + bounds.size.height / 2.0);

        if (strcmp(phase, "input") == 0)
        {
            postMouseEvent(kCGEventMouseMoved, center, kCGMouseButtonLeft);
            postMouseEvent(kCGEventMouseMoved,
                           CGPointMake(center.x + 20, center.y + 10), kCGMouseButtonLeft);
            postMouseEvent(kCGEventLeftMouseDown, center, kCGMouseButtonLeft);
            postMouseEvent(kCGEventLeftMouseUp, center, kCGMouseButtonLeft);
            postMouseEvent(kCGEventRightMouseDown, center, kCGMouseButtonRight);
            postMouseEvent(kCGEventRightMouseUp, center, kCGMouseButtonRight);
            postScroll(kCGScrollEventUnitLine, 3);
            postScroll(kCGScrollEventUnitLine, -3);
            postScroll(kCGScrollEventUnitPixel, 100);
            postScroll(kCGScrollEventUnitPixel, -100);
            postKey(GKF_KEY_H);
            postKey(GKF_KEY_I);
        }
        else if (strcmp(phase, "key-v") == 0)
            postKey(GKF_KEY_V);
        else if (strcmp(phase, "key-c") == 0)
            postKey(GKF_KEY_C);
        else if (strcmp(phase, "esc") == 0)
            postKey(GKF_KEY_ESCAPE);
        else
        {
            fprintf(stderr, "glfm_driver: unknown phase '%s'\n", phase);
            return 2;
        }
        return 0;
    }
}
