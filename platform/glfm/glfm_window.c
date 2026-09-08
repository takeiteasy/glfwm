//========================================================================
// glfwm - GLFM platform backend for GLFW 3.6: window + event queue
//------------------------------------------------------------------------
// Copyright (c) 2026 George Watson
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
//
// Event model:
//   GLFM callbacks run on the GLFM/OS thread and only enqueue raw events
//   (and post the frame tick). The app thread drains the queue inside
//   glfwPollEvents/glfwWaitEvents and dispatches through GLFW's regular
//   _glfwInput* paths, so user callbacks always fire on the polling thread.
//
//   glfwPollEvents waits for the next GLFM frame tick: the user's loop is
//   paced to the display refresh rate. This differs from desktop GLFW where
//   pollEvents never blocks, but is the natural semantics when the OS owns
//   the run loop.
//
//========================================================================

#include "internal.h"
#include "glfm_platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

//////////////////////////////////////////////////////////////////////////
//////                  Event queue (thread-safe)                  ///////
//////////////////////////////////////////////////////////////////////////

// The queue outlives glfwInit/glfwTerminate: GLFM callbacks may fire on the
// OS thread before glfwInit and after glfwTerminate, so it uses static
// initialization and is never destroyed.
//
static struct
{
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    GLFWbool        frameTick;      // a display frame is due
    GLFWbool        kick;           // postEmptyEvent
    GLFWbool        terminating;    // glfwTerminate was called
    GLFWbool        surfaceReady;   // GLFM surface exists (view is rendering)
    _GLFMEvent      events[_GLFM_EVENT_QUEUE_CAPACITY];
    int             head;
    int             count;
    double          cursorX;        // last cursor position, logical coords
    double          cursorY;
    GLFWbool        cursorInside;
} glfwm_ev = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
};

void _glfmGlfmPostFrame(void)
{
    pthread_mutex_lock(&glfwm_ev.lock);
    glfwm_ev.frameTick = GLFW_TRUE;
    pthread_cond_broadcast(&glfwm_ev.cond);
    pthread_mutex_unlock(&glfwm_ev.lock);
}

void _glfmGlfmKick(void)
{
    pthread_mutex_lock(&glfwm_ev.lock);
    glfwm_ev.kick = GLFW_TRUE;
    pthread_cond_broadcast(&glfwm_ev.cond);
    pthread_mutex_unlock(&glfwm_ev.lock);
}

void _glfmGlfmSetTerminating(void)
{
    pthread_mutex_lock(&glfwm_ev.lock);
    glfwm_ev.terminating = GLFW_TRUE;
    pthread_cond_broadcast(&glfwm_ev.cond);
    pthread_mutex_unlock(&glfwm_ev.lock);
}

void _glfmGlfmEnqueue(_GLFMEventType type, int i0, int i1, int i2,
                      double d0, double d1, const char* text)
{
    pthread_mutex_lock(&glfwm_ev.lock);
    if (glfwm_ev.count >= _GLFM_EVENT_QUEUE_CAPACITY)
    {
        pthread_mutex_unlock(&glfwm_ev.lock);
        fprintf(stderr, "GLFM: event queue overflow, event dropped\n");
        return;
    }
    _GLFMEvent* ev = &glfwm_ev.events[(glfwm_ev.head + glfwm_ev.count) % _GLFM_EVENT_QUEUE_CAPACITY];
    memset(ev, 0, sizeof(_GLFMEvent));
    ev->type = type;
    ev->i0 = i0;
    ev->i1 = i1;
    ev->i2 = i2;
    ev->d0 = d0;
    ev->d1 = d1;
    if (text)
    {
        strncpy(ev->text, text, sizeof(ev->text) - 1);
        ev->text[sizeof(ev->text) - 1] = '\0';
    }
    glfwm_ev.count++;
    pthread_cond_broadcast(&glfwm_ev.cond);
    pthread_mutex_unlock(&glfwm_ev.lock);
}

static GLFWbool glfwm__popEvent(_GLFMEvent* out)
{
    GLFWbool result = GLFW_FALSE;
    pthread_mutex_lock(&glfwm_ev.lock);
    if (glfwm_ev.count > 0)
    {
        *out = glfwm_ev.events[glfwm_ev.head];
        glfwm_ev.head = (glfwm_ev.head + 1) % _GLFM_EVENT_QUEUE_CAPACITY;
        glfwm_ev.count--;
        result = GLFW_TRUE;
    }
    pthread_mutex_unlock(&glfwm_ev.lock);
    return result;
}

// Dispatch one event. Never called with the queue lock held: user callbacks
// are free to call back into GLFW (e.g. glfwPostEmptyEvent).
//
static void glfwm__dispatch(const _GLFMEvent* ev)
{
    _GLFWwindow* window = _glfw.glfm.window;
    if (!window)
        return;

    switch (ev->type)
    {
        case _GLFM_EVENT_FRAMEBUFFER_SIZE:
            window->glfm.fbWidth = ev->i0;
            window->glfm.fbHeight = ev->i1;
            window->glfm.width = ev->i2 >> 16;
            window->glfm.height = ev->i2 & 0xffff;
            _glfwInputFramebufferSize(window, ev->i0, ev->i1);
            break;
        case _GLFM_EVENT_WINDOW_REFRESH:
            _glfwInputWindowDamage(window);
            break;
        case _GLFM_EVENT_WINDOW_FOCUS:
            _glfwInputWindowFocus(window, ev->i0);
            break;
        case _GLFM_EVENT_WINDOW_ICONIFY:
            _glfwInputWindowIconify(window, ev->i0);
            break;
        case _GLFM_EVENT_WINDOW_CLOSE:
            _glfwInputWindowCloseRequest(window);
            break;
        case _GLFM_EVENT_KEY:
            _glfwInputKey(window, ev->i0, ev->i1, ev->i2 >> 8, ev->i2 & 0xff);
            break;
        case _GLFM_EVENT_CHAR:
        {
            // Decode all UTF-8 codepoints in the string, one input event each
            const unsigned char* p = (const unsigned char*) ev->text;
            while (*p)
            {
                unsigned int cp = 0xfffd; // Unicode replacement character on error
                int len = 0;
                if (p[0] < 0x80)
                    { cp = p[0]; len = 1; }
                else if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80)
                    { cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f); len = 2; }
                else if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80)
                    { cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f); len = 3; }
                else if ((p[0] & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80)
                    { cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f); len = 4; }

                if (cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff))
                    _glfwInputChar(window, cp, 0, GLFW_TRUE);
                p += len ? len : 1;
            }
            break;
        }
        case _GLFM_EVENT_MOUSE_BUTTON:
            _glfwInputMouseClick(window, ev->i0, ev->i1, 0);
            break;
        case _GLFM_EVENT_CURSOR_POS:
            glfwm_ev.cursorX = ev->d0;
            glfwm_ev.cursorY = ev->d1;
            _glfwInputCursorPos(window, ev->d0, ev->d1);
            break;
        case _GLFM_EVENT_CURSOR_ENTER:
            glfwm_ev.cursorInside = ev->i0;
            _glfwInputCursorEnter(window, ev->i0);
            break;
        case _GLFM_EVENT_SCROLL:
            _glfwInputScroll(window, ev->d0, ev->d1);
            break;
        default:
            break;
    }
}

static void glfwm__drain(void)
{
    _GLFMEvent ev;
    while (glfwm__popEvent(&ev))
        glfwm__dispatch(&ev);
}

//////////////////////////////////////////////////////////////////////////
//////                       GLFW platform API                     ///////
//////////////////////////////////////////////////////////////////////////

// Blocks until the next frame tick, then dispatches queued events.
//
void _glfwPollEventsGlfm(void)
{
    pthread_mutex_lock(&glfwm_ev.lock);
    while (!glfwm_ev.frameTick && !glfwm_ev.kick && !glfwm_ev.terminating)
        pthread_cond_wait(&glfwm_ev.cond, &glfwm_ev.lock);
    glfwm_ev.frameTick = GLFW_FALSE;
    glfwm_ev.kick = GLFW_FALSE;
    pthread_mutex_unlock(&glfwm_ev.lock);

    glfwm__drain();
}

// Blocks until an event (or frame tick) arrives, then dispatches.
//
void _glfwWaitEventsGlfm(void)
{
    pthread_mutex_lock(&glfwm_ev.lock);
    while (glfwm_ev.count == 0 && !glfwm_ev.frameTick &&
           !glfwm_ev.kick && !glfwm_ev.terminating)
        pthread_cond_wait(&glfwm_ev.cond, &glfwm_ev.lock);
    glfwm_ev.frameTick = GLFW_FALSE;
    glfwm_ev.kick = GLFW_FALSE;
    pthread_mutex_unlock(&glfwm_ev.lock);

    glfwm__drain();
}

void _glfwWaitEventsTimeoutGlfm(double timeout)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t) timeout;
    ts.tv_nsec += (long) ((timeout - (double) (time_t) timeout) * 1e9);
    if (ts.tv_nsec >= 1000000000L)
    {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&glfwm_ev.lock);
    while (glfwm_ev.count == 0 && !glfwm_ev.frameTick &&
           !glfwm_ev.kick && !glfwm_ev.terminating)
    {
        if (pthread_cond_timedwait(&glfwm_ev.cond, &glfwm_ev.lock, &ts) == ETIMEDOUT)
            break;
    }
    glfwm_ev.frameTick = GLFW_FALSE;
    glfwm_ev.kick = GLFW_FALSE;
    pthread_mutex_unlock(&glfwm_ev.lock);

    glfwm__drain();
}

void _glfwPostEmptyEventGlfm(void)
{
    _glfmGlfmKick();
}

//////////////////////////////////////////////////////////////////////////
//////                        GLFM callbacks                       ///////
//////////////////////////////////////////////////////////////////////////

void _glfmGlfmRenderFunc(GLFMDisplay* display)
{
    (void) display;
    _glfmGlfmPostFrame();
}

void _glfmGlfmSurfaceCreatedFunc(GLFMDisplay* display, int width, int height)
{
    (void) display;
    pthread_mutex_lock(&glfwm_ev.lock);
    glfwm_ev.surfaceReady = GLFW_TRUE;
    pthread_cond_broadcast(&glfwm_ev.cond);
    pthread_mutex_unlock(&glfwm_ev.lock);
    if (_glfw.initialized)
        _glfw.glfm.surfaceCreated = GLFW_TRUE;
    // width/height are in pixels; pack logical size alongside
    double scale = _glfmGlfmDisplay() ? glfmGetDisplayScale(_glfmGlfmDisplay()) : 1.0;
    if (scale <= 0.0)
        scale = 1.0;
    int pw = (int) (width / scale) & 0xffff;
    int ph = (int) (height / scale) & 0xffff;
    _glfmGlfmEnqueue(_GLFM_EVENT_FRAMEBUFFER_SIZE, width, height,
                     (pw << 16) | ph, 0, 0, NULL);
    _glfmGlfmEnqueue(_GLFM_EVENT_WINDOW_REFRESH, 0, 0, 0, 0, 0, NULL);
}

void _glfmGlfmSurfaceResizedFunc(GLFMDisplay* display, int width, int height)
{
    _glfmGlfmSurfaceCreatedFunc(display, width, height);
}

void _glfmGlfmSurfaceDestroyedFunc(GLFMDisplay* display)
{
    (void) display;
    if (_glfw.initialized)
        _glfw.glfm.surfaceCreated = GLFW_FALSE;
    // No graceful "stop the run loop" exists in GLFM: treat surface loss as
    // the app quitting (matches iOS app teardown). Android backgrounding will
    // need a different mapping when that target is supported.
    _glfmGlfmEnqueue(_GLFM_EVENT_WINDOW_CLOSE, 0, 0, 0, 0, 0, NULL);
}

void _glfmGlfmSurfaceRefreshFunc(GLFMDisplay* display)
{
    (void) display;
    _glfmGlfmEnqueue(_GLFM_EVENT_WINDOW_REFRESH, 0, 0, 0, 0, 0, NULL);
}

void _glfmGlfmSurfaceErrorFunc(GLFMDisplay* display, const char* message)
{
    (void) display;
    fprintf(stderr, "GLFM: surface error: %s\n", message ? message : "(null)");
}

void _glfmGlfmAppFocusFunc(GLFMDisplay* display, bool focused)
{
    (void) display;
    // Background/foreground maps to the GLFW iconify events
    _glfmGlfmEnqueue(_GLFM_EVENT_WINDOW_ICONIFY, focused ? 0 : 1, 0, 0, 0, 0, NULL);
    _glfmGlfmEnqueue(_GLFM_EVENT_WINDOW_FOCUS, focused ? 1 : 0, 0, 0, 0, 0, NULL);
}

bool _glfmGlfmTouchFunc(GLFMDisplay* display, int touch, GLFMTouchPhase phase,
                        double x, double y)
{
    (void) display;
    // GLFM reports positions in pixels; GLFW cursor positions are logical
    double scale = glfmGetDisplayScale(display);
    if (scale <= 0.0)
        scale = 1.0;
    double px = x / scale;
    double py = y / scale;

    switch (phase)
    {
        case GLFMTouchPhaseBegan:
            _glfmGlfmEnqueue(_GLFM_EVENT_CURSOR_POS, 0, 0, 0, px, py, NULL);
            if (touch == 0)
                _glfmGlfmEnqueue(_GLFM_EVENT_CURSOR_ENTER, 1, 0, 0, 0, 0, NULL);
            if (touch <= GLFW_MOUSE_BUTTON_LAST)
                _glfmGlfmEnqueue(_GLFM_EVENT_MOUSE_BUTTON, touch, GLFW_PRESS, 0, 0, 0, NULL);
            break;
        case GLFMTouchPhaseMoved:
        case GLFMTouchPhaseHover:
            _glfmGlfmEnqueue(_GLFM_EVENT_CURSOR_POS, 0, 0, 0, px, py, NULL);
            break;
        case GLFMTouchPhaseEnded:
        case GLFMTouchPhaseCancelled:
            if (touch <= GLFW_MOUSE_BUTTON_LAST)
                _glfmGlfmEnqueue(_GLFM_EVENT_MOUSE_BUTTON, touch, GLFW_RELEASE, 0, 0, 0, NULL);
            if (touch == 0)
                _glfmGlfmEnqueue(_GLFM_EVENT_CURSOR_ENTER, 0, 0, 0, 0, 0, NULL);
            break;
        default:
            break;
    }
    return true;
}

bool _glfmGlfmKeyFunc(GLFMDisplay* display, GLFMKeyCode keyCode,
                      GLFMKeyAction action, int modifiers)
{
    (void) display;
    // Let the system handle the navigation/back button so the app can exit
    if (keyCode == GLFMKeyCodeNavigationBack)
        return false;

    int key = _glfmGlfmKeyCodeToGlfw(keyCode);
    if (key != GLFW_KEY_UNKNOWN)
    {
        int actionGlfw;
        switch (action)
        {
            case GLFMKeyActionReleased: actionGlfw = GLFW_RELEASE; break;
            case GLFMKeyActionRepeated: actionGlfw = GLFW_REPEAT;  break;
            default:                    actionGlfw = GLFW_PRESS;   break;
        }
        // GLFM modifier bits match GLFW for shift/control/alt/super; the
        // function-key bit has no GLFW equivalent and is dropped.
        int mods = modifiers & (GLFW_MOD_SHIFT | GLFW_MOD_CONTROL |
                                GLFW_MOD_ALT | GLFW_MOD_SUPER);
        _glfmGlfmEnqueue(_GLFM_EVENT_KEY, key, key, (actionGlfw << 8) | mods,
                         0, 0, NULL);
    }
    return true;
}

void _glfmGlfmCharFunc(GLFMDisplay* display, const char* string, int modifiers)
{
    (void) display;
    (void) modifiers;
    if (string)
        _glfmGlfmEnqueue(_GLFM_EVENT_CHAR, 0, 0, 0, 0, 0, string);
}

bool _glfmGlfmMouseWheelFunc(GLFMDisplay* display, double x, double y,
                             GLFMMouseWheelDeltaType deltaType,
                             double deltaX, double deltaY, double deltaZ)
{
    (void) display;
    (void) x;
    (void) y;
    (void) deltaType;
    (void) deltaZ;
    _glfmGlfmEnqueue(_GLFM_EVENT_SCROLL, 0, 0, 0, deltaX, deltaY, NULL);
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////                        Window functions                     ///////
//////////////////////////////////////////////////////////////////////////

GLFWbool _glfwCreateWindowGlfm(_GLFWwindow* window,
                               const _GLFWwndconfig* wndconfig,
                               const _GLFWctxconfig* ctxconfig,
                               const _GLFWfbconfig* fbconfig)
{
    (void) wndconfig;
    (void) fbconfig;

    if (!_glfmGlfmDisplay())
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "GLFM: Failed to create window: glfmMain has not run");
        return GLFW_FALSE;
    }

    if (_glfw.glfm.window)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "GLFM: Failed to create window: only one window is supported");
        return GLFW_FALSE;
    }

    if (ctxconfig->client != GLFW_NO_API)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "GLFM: Only GLFW_NO_API is supported (render with WebGPU/Vulkan via a surface extension)");
        return GLFW_FALSE;
    }

    _glfwGetWindowSizeGlfm(window, &window->glfm.width, &window->glfm.height);
    _glfwGetFramebufferSizeGlfm(window, &window->glfm.fbWidth, &window->glfm.fbHeight);
    window->glfm.contentScale = (float) glfmGetDisplayScale(_glfmGlfmDisplay());

    _glfw.glfm.window = window;

    // GLFM creates its view after glfmMain returns (view loading), so the
    // surface may not exist yet when the app thread gets here. Wait until
    // GLFM reports the surface created: this restores the GLFW contract that
    // the window (and its native view) exists when createWindow returns, and
    // makes surface accessors (e.g. the Metal view for WebGPU) immediately
    // usable. The condvar is shared with the event queue; the GLFM thread
    // only needs the lock briefly to signal.
    pthread_mutex_lock(&glfwm_ev.lock);
    while (!glfwm_ev.surfaceReady)
        pthread_cond_wait(&glfwm_ev.cond, &glfwm_ev.lock);
    pthread_mutex_unlock(&glfwm_ev.lock);

    return GLFW_TRUE;
}

void _glfwDestroyWindowGlfm(_GLFWwindow* window)
{
    if (_glfw.glfm.window == window)
        _glfw.glfm.window = NULL;
}

void _glfwSetWindowTitleGlfm(_GLFWwindow* window, const char* title)
{
    (void) window;
    (void) title;
}

void _glfwSetWindowIconGlfm(_GLFWwindow* window, int count, const GLFWimage* images)
{
    (void) window;
    (void) count;
    (void) images;
}

void _glfwSetWindowMonitorGlfm(_GLFWwindow* window, _GLFWmonitor* monitor,
                               int xpos, int ypos, int width, int height,
                               int refreshRate)
{
    (void) window;
    (void) monitor;
    (void) xpos;
    (void) ypos;
    (void) width;
    (void) height;
    (void) refreshRate;
}

void _glfwGetWindowPosGlfm(_GLFWwindow* window, int* xpos, int* ypos)
{
    (void) window;
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;
}

void _glfwSetWindowPosGlfm(_GLFWwindow* window, int xpos, int ypos)
{
    (void) window;
    (void) xpos;
    (void) ypos;
}

void _glfwGetWindowSizeGlfm(_GLFWwindow* window, int* width, int* height)
{
    (void) window;
    int w = 0, h = 0;
    if (_glfmGlfmDisplay())
        glfmGetDisplaySize(_glfmGlfmDisplay(), &w, &h);
    double scale = _glfmGlfmDisplay() ? glfmGetDisplayScale(_glfmGlfmDisplay()) : 1.0;
    if (scale <= 0.0)
        scale = 1.0;
    if (width)
        *width = (int) (w / scale);
    if (height)
        *height = (int) (h / scale);
}

void _glfwSetWindowSizeGlfm(_GLFWwindow* window, int width, int height)
{
    // The display size is controlled by the OS/orientation
    (void) window;
    (void) width;
    (void) height;
}

void _glfwSetWindowSizeLimitsGlfm(_GLFWwindow* window,
                                  int minwidth, int minheight,
                                  int maxwidth, int maxheight)
{
    (void) window;
    (void) minwidth;
    (void) minheight;
    (void) maxwidth;
    (void) maxheight;
}

void _glfwSetWindowAspectRatioGlfm(_GLFWwindow* window, int n, int d)
{
    (void) window;
    (void) n;
    (void) d;
}

void _glfwGetFramebufferSizeGlfm(_GLFWwindow* window, int* width, int* height)
{
    (void) window;
    int w = 0, h = 0;
    if (_glfmGlfmDisplay())
        glfmGetDisplaySize(_glfmGlfmDisplay(), &w, &h);
    if (width)
        *width = w;
    if (height)
        *height = h;
}

void _glfwGetWindowFrameSizeGlfm(_GLFWwindow* window,
                                 int* left, int* top,
                                 int* right, int* bottom)
{
    (void) window;
    if (left)
        *left = 0;
    if (top)
        *top = 0;
    if (right)
        *right = 0;
    if (bottom)
        *bottom = 0;
}

void _glfwGetWindowContentScaleGlfm(_GLFWwindow* window,
                                    float* xscale, float* yscale)
{
    (void) window;
    float scale = _glfmGlfmDisplay() ? (float) glfmGetDisplayScale(_glfmGlfmDisplay()) : 1.f;
    if (scale <= 0.f)
        scale = 1.f;
    if (xscale)
        *xscale = scale;
    if (yscale)
        *yscale = scale;
}

void _glfwIconifyWindowGlfm(_GLFWwindow* window)
{
    (void) window;
}

void _glfwRestoreWindowGlfm(_GLFWwindow* window)
{
    (void) window;
}

void _glfwMaximizeWindowGlfm(_GLFWwindow* window)
{
    (void) window;
}

void _glfwShowWindowGlfm(_GLFWwindow* window)
{
    (void) window;
}

void _glfwHideWindowGlfm(_GLFWwindow* window)
{
    (void) window;
}

void _glfwRequestWindowAttentionGlfm(_GLFWwindow* window)
{
    (void) window;
}

void _glfwFocusWindowGlfm(_GLFWwindow* window)
{
    (void) window;
}

GLFWbool _glfwWindowFocusedGlfm(_GLFWwindow* window)
{
    (void) window;
    return GLFW_TRUE;
}

GLFWbool _glfwWindowIconifiedGlfm(_GLFWwindow* window)
{
    (void) window;
    return GLFW_FALSE;
}

GLFWbool _glfwWindowVisibleGlfm(_GLFWwindow* window)
{
    (void) window;
    return GLFW_TRUE;
}

GLFWbool _glfwWindowMaximizedGlfm(_GLFWwindow* window)
{
    (void) window;
    return GLFW_TRUE;
}

GLFWbool _glfwWindowHoveredGlfm(_GLFWwindow* window)
{
    (void) window;
    return glfwm_ev.cursorInside;
}

GLFWbool _glfwFramebufferTransparentGlfm(_GLFWwindow* window)
{
    (void) window;
    return GLFW_FALSE;
}

float _glfwGetWindowOpacityGlfm(_GLFWwindow* window)
{
    (void) window;
    return 1.f;
}

void _glfwSetWindowOpacityGlfm(_GLFWwindow* window, float opacity)
{
    (void) window;
    (void) opacity;
}

void _glfwSetWindowResizableGlfm(_GLFWwindow* window, GLFWbool enabled)
{
    (void) window;
    (void) enabled;
}

void _glfwSetWindowDecoratedGlfm(_GLFWwindow* window, GLFWbool enabled)
{
    (void) window;
    (void) enabled;
}

void _glfwSetWindowFloatingGlfm(_GLFWwindow* window, GLFWbool enabled)
{
    (void) window;
    (void) enabled;
}

void _glfwSetWindowMousePassthroughGlfm(_GLFWwindow* window, GLFWbool enabled)
{
    (void) window;
    (void) enabled;
}

//////////////////////////////////////////////////////////////////////////
//////                         Input functions                     ///////
//////////////////////////////////////////////////////////////////////////

void _glfwGetCursorPosGlfm(_GLFWwindow* window, double* xpos, double* ypos)
{
    (void) window;
    if (xpos)
        *xpos = glfwm_ev.cursorX;
    if (ypos)
        *ypos = glfwm_ev.cursorY;
}

void _glfwSetCursorPosGlfm(_GLFWwindow* window, double x, double y)
{
    (void) window;
    glfwm_ev.cursorX = x;
    glfwm_ev.cursorY = y;
}

void _glfwSetCursorModeGlfm(_GLFWwindow* window, int mode)
{
    (void) window;
    (void) mode;
}

void _glfwSetRawMouseMotionGlfm(_GLFWwindow* window, GLFWbool enabled)
{
    (void) window;
    (void) enabled;
}

GLFWbool _glfwRawMouseMotionSupportedGlfm(void)
{
    return GLFW_FALSE;
}

GLFWbool _glfwCreateCursorGlfm(_GLFWcursor* cursor,
                               const GLFWimage* image,
                               int xhot, int yhot)
{
    (void) cursor;
    (void) image;
    (void) xhot;
    (void) yhot;
    return GLFW_TRUE;
}

GLFWbool _glfwCreateStandardCursorGlfm(_GLFWcursor* cursor, int shape)
{
    (void) cursor;
    (void) shape;
    return GLFW_TRUE;
}

void _glfwDestroyCursorGlfm(_GLFWcursor* cursor)
{
    (void) cursor;
}

void _glfwSetCursorGlfm(_GLFWwindow* window, _GLFWcursor* cursor)
{
    (void) window;
    (void) cursor;
}

void _glfwSetClipboardStringGlfm(const char* string)
{
    if (_glfmGlfmDisplay())
        glfmSetClipboardText(_glfmGlfmDisplay(), string);
    char* copy = _glfw_strdup(string);
    _glfw_free(_glfw.glfm.clipboardString);
    _glfw.glfm.clipboardString = copy;
}

const char* _glfwGetClipboardStringGlfm(void)
{
    // NOTE: GLFM clipboard reads are asynchronous (and may require user
    // confirmation on the web), so this returns the last string set by the
    // app. A synchronous-emulating read may be added later.
    return _glfw.glfm.clipboardString;
}

const char* _glfwGetScancodeNameGlfm(int scancode)
{
    // Scancodes are GLFW key codes (identity) on this platform
    switch (scancode)
    {
        case GLFW_KEY_SPACE:            return " ";
        case GLFW_KEY_APOSTROPHE:       return "'";
        case GLFW_KEY_COMMA:            return ",";
        case GLFW_KEY_MINUS:            return "-";
        case GLFW_KEY_PERIOD:           return ".";
        case GLFW_KEY_SLASH:            return "/";
        case GLFW_KEY_SEMICOLON:        return ";";
        case GLFW_KEY_EQUAL:            return "=";
        case GLFW_KEY_LEFT_BRACKET:     return "[";
        case GLFW_KEY_RIGHT_BRACKET:    return "]";
        case GLFW_KEY_BACKSLASH:        return "\\";
        case GLFW_KEY_GRAVE_ACCENT:     return "`";
        case GLFW_KEY_ESCAPE:           return "Escape";
        case GLFW_KEY_ENTER:            return "Enter";
        case GLFW_KEY_TAB:              return "Tab";
        case GLFW_KEY_BACKSPACE:        return "Backspace";
        case GLFW_KEY_LEFT:             return "Left";
        case GLFW_KEY_RIGHT:            return "Right";
        case GLFW_KEY_UP:               return "Up";
        case GLFW_KEY_DOWN:             return "Down";
        case GLFW_KEY_LEFT_SHIFT:       return "Left Shift";
        case GLFW_KEY_RIGHT_SHIFT:      return "Right Shift";
        case GLFW_KEY_LEFT_CONTROL:     return "Left Control";
        case GLFW_KEY_RIGHT_CONTROL:    return "Right Control";
        case GLFW_KEY_LEFT_ALT:         return "Left Alt";
        case GLFW_KEY_RIGHT_ALT:        return "Right Alt";
        case GLFW_KEY_LEFT_SUPER:       return "Left Super";
        case GLFW_KEY_RIGHT_SUPER:      return "Right Super";
        case GLFW_KEY_MENU:             return "Menu";
        default:
            if (scancode >= GLFW_KEY_0 && scancode <= GLFW_KEY_9)
            {
                static const char digits[10][2] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
                return digits[scancode - GLFW_KEY_0];
            }
            if (scancode >= GLFW_KEY_A && scancode <= GLFW_KEY_Z)
            {
                static const char letters[26][2] =
                {
                    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
                };
                return letters[scancode - GLFW_KEY_A];
            }
            if (scancode >= GLFW_KEY_F1 && scancode <= GLFW_KEY_F12)
            {
                static const char funcs[12][4] =
                {
                    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"
                };
                return funcs[scancode - GLFW_KEY_F1];
            }
            return NULL;
    }
}

int _glfwGetKeyScancodeGlfm(int key)
{
    // Scancodes are GLFW key codes (identity) on this platform
    if (key < GLFW_KEY_SPACE || key > GLFW_KEY_LAST)
        return -1;
    return key;
}

//////////////////////////////////////////////////////////////////////////
//////                     EGL and Vulkan functions                ///////
//////////////////////////////////////////////////////////////////////////

// NOTE: The GLFM platform has no EGL support; rendering is expected to use
// WebGPU/Vulkan through the surface extension. These stubs mirror the Null
// platform so the shared context code links.

EGLenum _glfwGetEGLPlatformGlfm(EGLint** attribs)
{
    (void) attribs;
    return 0;
}

EGLNativeDisplayType _glfwGetEGLNativeDisplayGlfm(void)
{
    return EGL_DEFAULT_DISPLAY;
}

EGLNativeWindowType _glfwGetEGLNativeWindowGlfm(_GLFWwindow* window)
{
    (void) window;
    return 0;
}

// NOTE: Android Vulkan surfaces (VK_KHR_android_surface) will be added with
// the Android target; for now mirror the Null platform's headless surface.
// (VkHeadlessSurfaceCreateInfoEXT / PFN_vkCreateHeadlessSurfaceEXT are
// provided by null_platform.h, which is always included.)

void _glfwGetRequiredInstanceExtensionsGlfm(char** extensions)
{
    if (!_glfw.vk.KHR_surface || !_glfw.vk.EXT_headless_surface)
        return;

    extensions[0] = "VK_KHR_surface";
    extensions[1] = "VK_EXT_headless_surface";
}

GLFWbool _glfwGetPhysicalDevicePresentationSupportGlfm(VkInstance instance,
                                                       VkPhysicalDevice device,
                                                       uint32_t queuefamily)
{
    (void) instance;
    (void) device;
    (void) queuefamily;
    return GLFW_TRUE;
}

VkResult _glfwCreateWindowSurfaceGlfm(VkInstance instance,
                                      _GLFWwindow* window,
                                      const VkAllocationCallbacks* allocator,
                                      VkSurfaceKHR* surface)
{
    PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT =
        (PFN_vkCreateHeadlessSurfaceEXT)
        vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT");
    if (!vkCreateHeadlessSurfaceEXT)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "GLFM: Vulkan instance missing VK_EXT_headless_surface extension");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkHeadlessSurfaceCreateInfoEXT sci;
    memset(&sci, 0, sizeof(sci));
    sci.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;

    const VkResult err = vkCreateHeadlessSurfaceEXT(instance, &sci, allocator, surface);
    if (err)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "GLFM: Failed to create Vulkan surface: %s",
                        _glfwGetVulkanResultString(err));
    }

    return err;
}
