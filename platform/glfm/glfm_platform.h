//========================================================================
// glfwm - GLFM platform backend for GLFW 3.6
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
// Implements a GLFW platform on top of GLFM (https://github.com/brackeen/glfm)
// for mobile targets (iOS/tvOS, Android, Emscripten). GLFM is used unmodified.
//
// The GLFM platform owns the OS entry point and the run loop: GLFM render
// callbacks pace the user's GLFW-style main loop (see glfm_window.c), and all
// OS input events are translated to GLFW events through an internal queue.
// Only GLFW_NO_API is supported; rendering is expected to be WebGPU/Vulkan
// via a surface obtained from the native view/window.
//
//========================================================================

#ifndef _glfw_glfm_platform_h_
#define _glfw_glfm_platform_h_

#include <pthread.h>
#include "glfm.h"

#define GLFW_GLFM_WINDOW_STATE         _GLFWwindowGlfm  glfm;
#define GLFW_GLFM_LIBRARY_WINDOW_STATE _GLFWlibraryGlfm glfm;
#define GLFW_GLFM_MONITOR_STATE        _GLFWmonitorGlfm glfm;

#define GLFW_GLFM_CONTEXT_STATE
#define GLFW_GLFM_CURSOR_STATE
#define GLFW_GLFM_LIBRARY_CONTEXT_STATE

// Per-window GLFM data
//
typedef struct _GLFWwindowGlfm
{
    int             width;      // logical size (points/screen coords)
    int             height;
    int             fbWidth;    // physical size (pixels)
    int             fbHeight;
    float           contentScale;
} _GLFWwindowGlfm;

// Per-monitor GLFM data
//
typedef struct _GLFWmonitorGlfm
{
    int             widthMM;
    int             heightMM;
} _GLFWmonitorGlfm;

// Internal event queue (GLFM thread -> app thread)
//
typedef enum _GLFMEventType
{
    _GLFM_EVENT_NONE = 0,
    _GLFM_EVENT_FRAMEBUFFER_SIZE,
    _GLFM_EVENT_WINDOW_REFRESH,
    _GLFM_EVENT_WINDOW_FOCUS,
    _GLFM_EVENT_WINDOW_ICONIFY,
    _GLFM_EVENT_WINDOW_CLOSE,
    _GLFM_EVENT_KEY,
    _GLFM_EVENT_CHAR,
    _GLFM_EVENT_MOUSE_BUTTON,
    _GLFM_EVENT_CURSOR_POS,
    _GLFM_EVENT_CURSOR_ENTER,
    _GLFM_EVENT_SCROLL
} _GLFMEventType;

typedef struct _GLFMEvent
{
    _GLFMEventType  type;
    int             i0;         // key / button / enter / iconify / focus
    int             i1;         // scancode / action
    int             i2;         // mods
    double          d0;         // x / xoffset
    double          d1;         // y / yoffset
    char            text[16];   // UTF-8 text (char events)
} _GLFMEvent;

#define _GLFM_EVENT_QUEUE_CAPACITY 256

// GLFM-specific global data (embedded in _GLFWlibrary)
//
// NOTE: the GLFMDisplay handle is not stored here: it arrives via glfmMain
// before glfwInit and GLFM callbacks may also run before/after glfwInit, so
// it lives in a file-static owned by glfm_entry.c and is accessed with
// _glfmGlfmDisplay().
//
typedef struct _GLFWlibraryGlfm
{
    _GLFWwindow*    window;         // the single window
    _GLFWmonitor*   monitor;
    GLFWbool        surfaceCreated;
    GLFWbool        focused;        // app focus state (backgrounded = false)
    GLFWbool        iconified;      // background state (mirrors AppFocusFunc)
    char*           clipboardString;
} _GLFWlibraryGlfm;

// ---- Entry point bridge (glfm_entry.c) ----

GLFMDisplay* _glfmGlfmDisplay(void);

// ---- Key code table (glfm_init.c) ----

int _glfmGlfmKeyCodeToGlfw(GLFMKeyCode keyCode);

// ---- Event queue (glfm_window.c) ----
// Thread-safe; callable from the GLFM (OS) thread before/after glfwInit.

void _glfmGlfmPostFrame(void);
void _glfmGlfmKick(void);
void _glfmGlfmSetTerminating(void);
void _glfmGlfmEnqueue(_GLFMEventType type, int i0, int i1, int i2,
                      double d0, double d1, const char* text);

// ---- Event loop (glfm_window.c; vtable entries) ----

void _glfwPollEventsGlfm(void);
void _glfwWaitEventsGlfm(void);
void _glfwWaitEventsTimeoutGlfm(double timeout);
void _glfwPostEmptyEventGlfm(void);

// ---- GLFM callbacks (registered by glfm_entry.c, implemented in glfm_window.c) ----

void _glfmGlfmRenderFunc(GLFMDisplay* display);
void _glfmGlfmSurfaceCreatedFunc(GLFMDisplay* display, int width, int height);
void _glfmGlfmSurfaceResizedFunc(GLFMDisplay* display, int width, int height);
void _glfmGlfmSurfaceDestroyedFunc(GLFMDisplay* display);
void _glfmGlfmSurfaceRefreshFunc(GLFMDisplay* display);
void _glfmGlfmSurfaceErrorFunc(GLFMDisplay* display, const char* message);
void _glfmGlfmAppFocusFunc(GLFMDisplay* display, bool focused);
bool _glfmGlfmTouchFunc(GLFMDisplay* display, int touch, GLFMTouchPhase phase,
                        double x, double y);
bool _glfmGlfmKeyFunc(GLFMDisplay* display, GLFMKeyCode keyCode,
                      GLFMKeyAction action, int modifiers);
void _glfmGlfmCharFunc(GLFMDisplay* display, const char* string, int modifiers);
bool _glfmGlfmMouseWheelFunc(GLFMDisplay* display, double x, double y,
                             GLFMMouseWheelDeltaType deltaType,
                             double deltaX, double deltaY, double deltaZ);
void _glfmGlfmClipboardTextFunc(GLFMDisplay* display, const char* string);

// ---- Connect / init ----

GLFWbool _glfwConnectGlfm(int platformID, _GLFWplatform* platform);
int _glfwInitGlfm(void);
void _glfwTerminateGlfm(void);
void _glfwPollMonitorsGlfm(void);

// ---- Monitor ----

void _glfwFreeMonitorGlfm(_GLFWmonitor* monitor);
void _glfwGetMonitorPosGlfm(_GLFWmonitor* monitor, int* xpos, int* ypos);
void _glfwGetMonitorContentScaleGlfm(_GLFWmonitor* monitor, float* xscale, float* yscale);
void _glfwGetMonitorWorkareaGlfm(_GLFWmonitor* monitor, int* xpos, int* ypos, int* width, int* height);
GLFWvidmode* _glfwGetVideoModesGlfm(_GLFWmonitor* monitor, int* found);
GLFWbool _glfwGetVideoModeGlfm(_GLFWmonitor* monitor, GLFWvidmode* mode);
GLFWbool _glfwGetGammaRampGlfm(_GLFWmonitor* monitor, GLFWgammaramp* ramp);
void _glfwSetGammaRampGlfm(_GLFWmonitor* monitor, const GLFWgammaramp* ramp);

// ---- Window ----

GLFWbool _glfwCreateWindowGlfm(_GLFWwindow* window, const _GLFWwndconfig* wndconfig,
                               const _GLFWctxconfig* ctxconfig, const _GLFWfbconfig* fbconfig);
void _glfwDestroyWindowGlfm(_GLFWwindow* window);
void _glfwSetWindowTitleGlfm(_GLFWwindow* window, const char* title);
void _glfwSetWindowIconGlfm(_GLFWwindow* window, int count, const GLFWimage* images);
void _glfwSetWindowMonitorGlfm(_GLFWwindow* window, _GLFWmonitor* monitor,
                               int xpos, int ypos, int width, int height, int refreshRate);
void _glfwGetWindowPosGlfm(_GLFWwindow* window, int* xpos, int* ypos);
void _glfwSetWindowPosGlfm(_GLFWwindow* window, int xpos, int ypos);
void _glfwGetWindowSizeGlfm(_GLFWwindow* window, int* width, int* height);
void _glfwSetWindowSizeGlfm(_GLFWwindow* window, int width, int height);
void _glfwSetWindowSizeLimitsGlfm(_GLFWwindow* window, int minwidth, int minheight,
                                  int maxwidth, int maxheight);
void _glfwSetWindowAspectRatioGlfm(_GLFWwindow* window, int n, int d);
void _glfwGetFramebufferSizeGlfm(_GLFWwindow* window, int* width, int* height);
void _glfwGetWindowFrameSizeGlfm(_GLFWwindow* window, int* left, int* top,
                                 int* right, int* bottom);
void _glfwGetWindowContentScaleGlfm(_GLFWwindow* window, float* xscale, float* yscale);
void _glfwIconifyWindowGlfm(_GLFWwindow* window);
void _glfwRestoreWindowGlfm(_GLFWwindow* window);
void _glfwMaximizeWindowGlfm(_GLFWwindow* window);
void _glfwShowWindowGlfm(_GLFWwindow* window);
void _glfwHideWindowGlfm(_GLFWwindow* window);
void _glfwRequestWindowAttentionGlfm(_GLFWwindow* window);
void _glfwFocusWindowGlfm(_GLFWwindow* window);
GLFWbool _glfwWindowFocusedGlfm(_GLFWwindow* window);
GLFWbool _glfwWindowIconifiedGlfm(_GLFWwindow* window);
GLFWbool _glfwWindowVisibleGlfm(_GLFWwindow* window);
GLFWbool _glfwWindowMaximizedGlfm(_GLFWwindow* window);
GLFWbool _glfwWindowHoveredGlfm(_GLFWwindow* window);
GLFWbool _glfwFramebufferTransparentGlfm(_GLFWwindow* window);
float _glfwGetWindowOpacityGlfm(_GLFWwindow* window);
void _glfwSetWindowOpacityGlfm(_GLFWwindow* window, float opacity);
void _glfwSetWindowResizableGlfm(_GLFWwindow* window, GLFWbool enabled);
void _glfwSetWindowDecoratedGlfm(_GLFWwindow* window, GLFWbool enabled);
void _glfwSetWindowFloatingGlfm(_GLFWwindow* window, GLFWbool enabled);
void _glfwSetWindowMousePassthroughGlfm(_GLFWwindow* window, GLFWbool enabled);

// ---- Input ----

void _glfwGetCursorPosGlfm(_GLFWwindow* window, double* xpos, double* ypos);
void _glfwSetCursorPosGlfm(_GLFWwindow* window, double x, double y);
void _glfwSetCursorModeGlfm(_GLFWwindow* window, int mode);
void _glfwSetRawMouseMotionGlfm(_GLFWwindow* window, GLFWbool enabled);
GLFWbool _glfwRawMouseMotionSupportedGlfm(void);
GLFWbool _glfwCreateCursorGlfm(_GLFWcursor* cursor, const GLFWimage* image,
                               int xhot, int yhot);
GLFWbool _glfwCreateStandardCursorGlfm(_GLFWcursor* cursor, int shape);
void _glfwDestroyCursorGlfm(_GLFWcursor* cursor);
void _glfwSetCursorGlfm(_GLFWwindow* window, _GLFWcursor* cursor);
void _glfwSetClipboardStringGlfm(const char* string);
const char* _glfwGetClipboardStringGlfm(void);
const char* _glfwGetScancodeNameGlfm(int scancode);
int _glfwGetKeyScancodeGlfm(int key);

// ---- EGL and Vulkan ----

EGLenum _glfwGetEGLPlatformGlfm(EGLint** attribs);
EGLNativeDisplayType _glfwGetEGLNativeDisplayGlfm(void);
EGLNativeWindowType _glfwGetEGLNativeWindowGlfm(_GLFWwindow* window);
void _glfwGetRequiredInstanceExtensionsGlfm(char** extensions);
GLFWbool _glfwGetPhysicalDevicePresentationSupportGlfm(VkInstance instance,
                                                       VkPhysicalDevice device,
                                                       uint32_t queuefamily);
VkResult _glfwCreateWindowSurfaceGlfm(VkInstance instance, _GLFWwindow* window,
                                      const VkAllocationCallbacks* allocator,
                                      VkSurfaceKHR* surface);

#endif // _glfw_glfm_platform_h_
