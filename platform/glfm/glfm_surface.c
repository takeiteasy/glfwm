//========================================================================
// glfwm - GLFM platform backend for GLFW 3.6: Metal layer access
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
// Returns the CAMetalLayer backing the GLFM view for rendering. The GLFM
// Metal view is an MTKView that the glfwm GLFM patch keeps paused: it never
// acquires or presents drawables itself, so the application (e.g. a WebGPU
// surface via glfw3webgpu) has exclusive use of the layer's drawable pool.
//
// Compiled as Objective-C (see the Makefile); the non-Apple path is a stub.
//
//========================================================================

#include "internal.h"
#include "glfm_platform.h"
#include "glfwm.h"

#if defined(__APPLE__)

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX
#import <AppKit/AppKit.h>
typedef NSView GlfwmView;
#else
#import <UIKit/UIKit.h>
typedef UIView GlfwmView;
#endif

void* glfwmGetMetalLayer(void)
{
    GLFMDisplay* display = _glfmGlfmDisplay();
    if (!display)
        return NULL;

    GlfwmView* view = (GlfwmView*) glfmGetMetalView(display);
    if (!view)
        return NULL;

    return view.layer;
}

#else // __APPLE__

void* glfwmGetMetalLayer(void)
{
    return NULL;
}

#endif // __APPLE__
