//========================================================================
// glfwm - GLFM platform backend for GLFW 3.6: init + key codes
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

#include "internal.h"
#include "glfm_platform.h"

#include <stdlib.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
//////                       GLFW platform API                     ///////
//////////////////////////////////////////////////////////////////////////

GLFWbool _glfwConnectGlfm(int platformID, _GLFWplatform* platform)
{
    (void) platformID;

    const _GLFWplatform glfm =
    {
        .platformID = GLFW_PLATFORM_GLFM,
        .init = _glfwInitGlfm,
        .terminate = _glfwTerminateGlfm,
        .getCursorPos = _glfwGetCursorPosGlfm,
        .setCursorPos = _glfwSetCursorPosGlfm,
        .setCursorMode = _glfwSetCursorModeGlfm,
        .setRawMouseMotion = _glfwSetRawMouseMotionGlfm,
        .rawMouseMotionSupported = _glfwRawMouseMotionSupportedGlfm,
        .createCursor = _glfwCreateCursorGlfm,
        .createStandardCursor = _glfwCreateStandardCursorGlfm,
        .destroyCursor = _glfwDestroyCursorGlfm,
        .setCursor = _glfwSetCursorGlfm,
        .getScancodeName = _glfwGetScancodeNameGlfm,
        .getKeyScancode = _glfwGetKeyScancodeGlfm,
        .setClipboardString = _glfwSetClipboardStringGlfm,
        .getClipboardString = _glfwGetClipboardStringGlfm,
        // NOTE: null_joystick.c is platform-independent and always compiled
        .initJoysticks = _glfwInitJoysticksNull,
        .terminateJoysticks = _glfwTerminateJoysticksNull,
        .pollJoystick = _glfwPollJoystickNull,
        .getMappingName = _glfwGetMappingNameNull,
        .updateGamepadGUID = _glfwUpdateGamepadGUIDNull,
        .freeMonitor = _glfwFreeMonitorGlfm,
        .getMonitorPos = _glfwGetMonitorPosGlfm,
        .getMonitorContentScale = _glfwGetMonitorContentScaleGlfm,
        .getMonitorWorkarea = _glfwGetMonitorWorkareaGlfm,
        .getVideoModes = _glfwGetVideoModesGlfm,
        .getVideoMode = _glfwGetVideoModeGlfm,
        .getGammaRamp = _glfwGetGammaRampGlfm,
        .setGammaRamp = _glfwSetGammaRampGlfm,
        .createWindow = _glfwCreateWindowGlfm,
        .destroyWindow = _glfwDestroyWindowGlfm,
        .setWindowTitle = _glfwSetWindowTitleGlfm,
        .setWindowIcon = _glfwSetWindowIconGlfm,
        .getWindowPos = _glfwGetWindowPosGlfm,
        .setWindowPos = _glfwSetWindowPosGlfm,
        .getWindowSize = _glfwGetWindowSizeGlfm,
        .setWindowSize = _glfwSetWindowSizeGlfm,
        .setWindowSizeLimits = _glfwSetWindowSizeLimitsGlfm,
        .setWindowAspectRatio = _glfwSetWindowAspectRatioGlfm,
        .getFramebufferSize = _glfwGetFramebufferSizeGlfm,
        .getWindowFrameSize = _glfwGetWindowFrameSizeGlfm,
        .getWindowContentScale = _glfwGetWindowContentScaleGlfm,
        .iconifyWindow = _glfwIconifyWindowGlfm,
        .restoreWindow = _glfwRestoreWindowGlfm,
        .maximizeWindow = _glfwMaximizeWindowGlfm,
        .showWindow = _glfwShowWindowGlfm,
        .hideWindow = _glfwHideWindowGlfm,
        .requestWindowAttention = _glfwRequestWindowAttentionGlfm,
        .focusWindow = _glfwFocusWindowGlfm,
        .setWindowMonitor = _glfwSetWindowMonitorGlfm,
        .windowFocused = _glfwWindowFocusedGlfm,
        .windowIconified = _glfwWindowIconifiedGlfm,
        .windowVisible = _glfwWindowVisibleGlfm,
        .windowMaximized = _glfwWindowMaximizedGlfm,
        .windowHovered = _glfwWindowHoveredGlfm,
        .framebufferTransparent = _glfwFramebufferTransparentGlfm,
        .getWindowOpacity = _glfwGetWindowOpacityGlfm,
        .setWindowResizable = _glfwSetWindowResizableGlfm,
        .setWindowDecorated = _glfwSetWindowDecoratedGlfm,
        .setWindowFloating = _glfwSetWindowFloatingGlfm,
        .setWindowOpacity = _glfwSetWindowOpacityGlfm,
        .setWindowMousePassthrough = _glfwSetWindowMousePassthroughGlfm,
        .pollEvents = _glfwPollEventsGlfm,
        .waitEvents = _glfwWaitEventsGlfm,
        .waitEventsTimeout = _glfwWaitEventsTimeoutGlfm,
        .postEmptyEvent = _glfwPostEmptyEventGlfm,
        .getEGLPlatform = _glfwGetEGLPlatformGlfm,
        .getEGLNativeDisplay = _glfwGetEGLNativeDisplayGlfm,
        .getEGLNativeWindow = _glfwGetEGLNativeWindowGlfm,
        .getRequiredInstanceExtensions = _glfwGetRequiredInstanceExtensionsGlfm,
        .getPhysicalDevicePresentationSupport = _glfwGetPhysicalDevicePresentationSupportGlfm,
        .createWindowSurface = _glfwCreateWindowSurfaceGlfm
    };

    *platform = glfm;
    return GLFW_TRUE;
}

int _glfwInitGlfm(void)
{
    if (!_glfmGlfmDisplay())
    {
        _glfwInputError(GLFW_PLATFORM_UNAVAILABLE,
                        "GLFM: Platform unavailable: glfmMain has not run (is this a mobile build?)");
        return GLFW_FALSE;
    }

    _glfw.glfm.focused = GLFW_TRUE;
    _glfw.glfm.iconified = GLFW_FALSE;

    if (!_glfw.glfm.monitor)
        _glfwPollMonitorsGlfm();

    return GLFW_TRUE;
}

void _glfwTerminateGlfm(void)
{
    // Unblock any pollEvents/waitEvents stuck on the frame tick
    _glfmGlfmSetTerminating();

    _glfw_free(_glfw.glfm.clipboardString);
    _glfw.glfm.clipboardString = NULL;
    _glfw.glfm.window = NULL;
    _glfw.glfm.monitor = NULL;
    _glfw.glfm.surfaceCreated = GLFW_FALSE;
    _glfw.glfm.focused = GLFW_FALSE;
    _glfw.glfm.iconified = GLFW_FALSE;
    // NOTE: the event queue and GLFMDisplay handle intentionally outlive
    // glfwTerminate (GLFM callbacks keep firing on the OS thread).
    _glfwTerminateEGL();
    _glfwTerminateOSMesa();
}

//////////////////////////////////////////////////////////////////////////
//////                    GLFM -> GLFW key codes                   ///////
//////////////////////////////////////////////////////////////////////////

// The printable-ASCII GLFM key codes (0x20..0x60) have the same values as
// GLFW keys for those characters, so only the non-ASCII range needs a table.
//
int _glfmGlfmKeyCodeToGlfw(GLFMKeyCode keyCode)
{
    switch (keyCode)
    {
        case GLFMKeyCodeBackspace:      return GLFW_KEY_BACKSPACE;
        case GLFMKeyCodeTab:            return GLFW_KEY_TAB;
        case GLFMKeyCodeEnter:          return GLFW_KEY_ENTER;
        case GLFMKeyCodeEscape:         return GLFW_KEY_ESCAPE;
        case GLFMKeyCodeDelete:         return GLFW_KEY_DELETE;
        case GLFMKeyCodeCapsLock:       return GLFW_KEY_CAPS_LOCK;
        case GLFMKeyCodeShiftLeft:      return GLFW_KEY_LEFT_SHIFT;
        case GLFMKeyCodeShiftRight:     return GLFW_KEY_RIGHT_SHIFT;
        case GLFMKeyCodeControlLeft:    return GLFW_KEY_LEFT_CONTROL;
        case GLFMKeyCodeControlRight:   return GLFW_KEY_RIGHT_CONTROL;
        case GLFMKeyCodeAltLeft:        return GLFW_KEY_LEFT_ALT;
        case GLFMKeyCodeAltRight:       return GLFW_KEY_RIGHT_ALT;
        case GLFMKeyCodeMetaLeft:       return GLFW_KEY_LEFT_SUPER;
        case GLFMKeyCodeMetaRight:      return GLFW_KEY_RIGHT_SUPER;
        case GLFMKeyCodeMenu:           return GLFW_KEY_MENU;
        case GLFMKeyCodeInsert:         return GLFW_KEY_INSERT;
        case GLFMKeyCodePageUp:         return GLFW_KEY_PAGE_UP;
        case GLFMKeyCodePageDown:       return GLFW_KEY_PAGE_DOWN;
        case GLFMKeyCodeEnd:            return GLFW_KEY_END;
        case GLFMKeyCodeHome:           return GLFW_KEY_HOME;
        case GLFMKeyCodeArrowLeft:      return GLFW_KEY_LEFT;
        case GLFMKeyCodeArrowUp:        return GLFW_KEY_UP;
        case GLFMKeyCodeArrowRight:     return GLFW_KEY_RIGHT;
        case GLFMKeyCodeArrowDown:      return GLFW_KEY_DOWN;
        case GLFMKeyCodePrintScreen:    return GLFW_KEY_PRINT_SCREEN;
        case GLFMKeyCodeScrollLock:     return GLFW_KEY_SCROLL_LOCK;
        case GLFMKeyCodePause:          return GLFW_KEY_PAUSE;
        case GLFMKeyCodeNumLock:        return GLFW_KEY_NUM_LOCK;
        case GLFMKeyCodeNumpadDecimal:  return GLFW_KEY_KP_DECIMAL;
        case GLFMKeyCodeNumpadMultiply: return GLFW_KEY_KP_MULTIPLY;
        case GLFMKeyCodeNumpadAdd:      return GLFW_KEY_KP_ADD;
        case GLFMKeyCodeNumpadDivide:   return GLFW_KEY_KP_DIVIDE;
        case GLFMKeyCodeNumpadEnter:    return GLFW_KEY_KP_ENTER;
        case GLFMKeyCodeNumpadSubtract: return GLFW_KEY_KP_SUBTRACT;
        case GLFMKeyCodeNumpadEqual:    return GLFW_KEY_KP_EQUAL;
        case GLFMKeyCodeF1:             return GLFW_KEY_F1;
        case GLFMKeyCodeF2:             return GLFW_KEY_F2;
        case GLFMKeyCodeF3:             return GLFW_KEY_F3;
        case GLFMKeyCodeF4:             return GLFW_KEY_F4;
        case GLFMKeyCodeF5:             return GLFW_KEY_F5;
        case GLFMKeyCodeF6:             return GLFW_KEY_F6;
        case GLFMKeyCodeF7:             return GLFW_KEY_F7;
        case GLFMKeyCodeF8:             return GLFW_KEY_F8;
        case GLFMKeyCodeF9:             return GLFW_KEY_F9;
        case GLFMKeyCodeF10:            return GLFW_KEY_F10;
        case GLFMKeyCodeF11:            return GLFW_KEY_F11;
        case GLFMKeyCodeF12:            return GLFW_KEY_F12;
        case GLFMKeyCodeF13:            return GLFW_KEY_F13;
        case GLFMKeyCodeF14:            return GLFW_KEY_F14;
        case GLFMKeyCodeF15:            return GLFW_KEY_F15;
        case GLFMKeyCodeF16:            return GLFW_KEY_F16;
        case GLFMKeyCodeF17:            return GLFW_KEY_F17;
        case GLFMKeyCodeF18:            return GLFW_KEY_F18;
        case GLFMKeyCodeF19:            return GLFW_KEY_F19;
        case GLFMKeyCodeF20:            return GLFW_KEY_F20;
        case GLFMKeyCodeF21:            return GLFW_KEY_F21;
        case GLFMKeyCodeF22:            return GLFW_KEY_F22;
        case GLFMKeyCodeF23:            return GLFW_KEY_F23;
        case GLFMKeyCodeF24:            return GLFW_KEY_F24;
        // GLFMKeyCodeNavigationBack is handled by the key callback (returns
        // false to let the system exit the app); media keys have no GLFW
        // equivalent.
        default:
            // Printable ASCII range: identical values in both APIs
            if (keyCode >= GLFMKeyCodeSpace && keyCode <= GLFMKeyCodeBackquote)
            {
                switch (keyCode)
                {
                    case GLFMKeyCodeSpace:      return GLFW_KEY_SPACE;
                    case GLFMKeyCodeQuote:      return GLFW_KEY_APOSTROPHE;
                    case GLFMKeyCodeComma:      return GLFW_KEY_COMMA;
                    case GLFMKeyCodeMinus:      return GLFW_KEY_MINUS;
                    case GLFMKeyCodePeriod:     return GLFW_KEY_PERIOD;
                    case GLFMKeyCodeSlash:      return GLFW_KEY_SLASH;
                    case GLFMKeyCodeSemicolon:  return GLFW_KEY_SEMICOLON;
                    case GLFMKeyCodeEqual:      return GLFW_KEY_EQUAL;
                    case GLFMKeyCodeBracketLeft:  return GLFW_KEY_LEFT_BRACKET;
                    case GLFMKeyCodeBackslash:    return GLFW_KEY_BACKSLASH;
                    case GLFMKeyCodeBracketRight: return GLFW_KEY_RIGHT_BRACKET;
                    case GLFMKeyCodeBackquote:    return GLFW_KEY_GRAVE_ACCENT;
                    default:
                        // 0-9 and A-Z are identity
                        if ((keyCode >= GLFMKeyCode0 && keyCode <= GLFMKeyCode9) ||
                            (keyCode >= GLFMKeyCodeA && keyCode <= GLFMKeyCodeZ))
                            return (int) keyCode;
                        return GLFW_KEY_UNKNOWN;
                }
            }
            if (keyCode >= GLFMKeyCodeNumpad0 && keyCode <= GLFMKeyCodeNumpad9)
                return GLFW_KEY_KP_0 + (keyCode - GLFMKeyCodeNumpad0);
            return GLFW_KEY_UNKNOWN;
    }
}
