//========================================================================
// glfwm - GLFM platform backend for GLFW 3.6: entry point bridge
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
// GLFM owns the OS entry point and run loop (glfmMain is called by GLFM's
// platform bootstrap after the surface exists). This file:
//   1. Stores the GLFMDisplay handle before glfwInit runs.
//   2. Registers the GLFM callbacks that drive the GLFW event queue.
//   3. Spawns the application thread, which runs the user-registered entry
//      point (glfwmSetEntryPoint / GLFM_GLFW_APP_MAIN macro). When that
//      entry returns, the process exits.
//
//========================================================================

#include "internal.h"
#include "glfm_platform.h"
#include "glfwm.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

//////////////////////////////////////////////////////////////////////////
//////                         Entry state                         ///////
//////////////////////////////////////////////////////////////////////////

static GLFMDisplay* glfwm__display = NULL;
static glfwm_entry_fn glfwm__entry = NULL;

GLFMDisplay* _glfmGlfmDisplay(void)
{
    return glfwm__display;
}

void glfwmSetEntryPoint(glfwm_entry_fn entry)
{
    // Must be called before the app thread starts: typically from a static
    // initializer (see the GLFM_GLFW_APP_MAIN macro in glfwm.h).
    glfwm__entry = entry;
}

//////////////////////////////////////////////////////////////////////////
//////                        App thread                           ///////
//////////////////////////////////////////////////////////////////////////

static void* glfwm__appThreadMain(void* argument)
{
    (void) argument;

    if (!glfwm__entry)
    {
        fprintf(stderr,
                "GLFM: no entry point registered; call glfwmSetEntryPoint() or use the GLFM_GLFW_APP_MAIN macro\n");
        exit(EXIT_FAILURE);
    }

    char* argv[1] = { (char*) "app" };
    int result = glfwm__entry(1, argv);

    // The user's main returned: on a GLFM platform the process is done.
    // There is no graceful "stop the run loop" API in GLFM.
    exit(result);
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
//////                        glfmMain                             ///////
//////////////////////////////////////////////////////////////////////////

void glfmMain(GLFMDisplay* display)
{
    glfwm__display = display;

    // Configure the display. The preferred rendering API is Metal (iOS):
    // GLFM falls back to the next available API (unused OpenGL ES context on
    // Android/Emscripten - rendering is expected to be WebGPU via a surface
    // created from the native view/window).
    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIMetal,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);
    glfmSetMultitouchEnabled(display, true);

    // Register the callbacks that drive the GLFW event queue
    glfmSetRenderFunc(display, _glfmGlfmRenderFunc);
    glfmSetSurfaceCreatedFunc(display, _glfmGlfmSurfaceCreatedFunc);
    glfmSetSurfaceResizedFunc(display, _glfmGlfmSurfaceResizedFunc);
    glfmSetSurfaceDestroyedFunc(display, _glfmGlfmSurfaceDestroyedFunc);
    glfmSetSurfaceRefreshFunc(display, _glfmGlfmSurfaceRefreshFunc);
    glfmSetSurfaceErrorFunc(display, _glfmGlfmSurfaceErrorFunc);
    glfmSetAppFocusFunc(display, _glfmGlfmAppFocusFunc);
    glfmSetTouchFunc(display, _glfmGlfmTouchFunc);
    glfmSetKeyFunc(display, _glfmGlfmKeyFunc);
    glfmSetCharFunc(display, _glfmGlfmCharFunc);
    glfmSetMouseWheelFunc(display, _glfmGlfmMouseWheelFunc);

    // Run the application entry on a separate thread: glfmMain must return
    // before GLFM starts delivering render ticks and input events, so the
    // user's blocking GLFW-style loop cannot run on this thread.
    pthread_attr_t attribute;
    pthread_attr_init(&attribute);
    pthread_attr_setdetachstate(&attribute, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    if (pthread_create(&thread, &attribute, glfwm__appThreadMain, NULL) != 0)
    {
        fprintf(stderr, "GLFM: failed to start application thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_attr_destroy(&attribute);
}
