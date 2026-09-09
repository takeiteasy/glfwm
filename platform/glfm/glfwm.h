//========================================================================
// glfwm - GLFW + GLFM (windowing/input for mobile and desktop host)
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
// Mobile entry point bridge. GLFM owns the OS entry point (main() on
// Apple/Emscripten, ANativeActivity on Android), so applications must
// register their main function instead of defining main():
//
//   int app_main(int argc, char** argv) { /* normal GLFW code */ }
//   GLFM_GLFW_APP_MAIN(app_main)
//
// The macro registers the entry point from a static initializer, so no other
// app source changes are needed. On desktop builds the macro is inert and
// the app keeps its regular main().
//
// GLFM_GLFW_PLATFORM is auto-detected for iOS/Android/Emscripten. Builds that
// target the GLFM backend on desktop macOS (host development) must define
// GLFM_GLFW_PLATFORM explicitly when compiling application sources.
//
// One codebase builds two library flavors:
//
//   glfwm  - GLFW + the GLFM platform backend (this header's core section).
//   glfwmw - glfwm plus wgpu-native and a WebGPU surface bridge, built with
//            GLFWM_WGPU defined. Adds glfwmwCreateWindowWGPUSurface() below.
//
// Applications include this single header either way; the WebGPU API appears
// only when GLFWM_WGPU is defined (matching the linked library). The surface
// API uses GLFWwindow*, so <GLFW/glfw3.h> must be included before this
// header when GLFWM_WGPU is defined.
//========================================================================

#ifndef glfwm_h
#define glfwm_h

// Auto-detect GLFM platform builds
#if !defined(GLFM_GLFW_PLATFORM)
#  if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#    define GLFM_GLFW_PLATFORM
#  elif defined(__APPLE__)
#    include <TargetConditionals.h>
#    if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#      define GLFM_GLFW_PLATFORM
#    endif
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Application entry point: same signature and semantics as main().
typedef int (*glfwm_entry_fn)(int argc, char** argv);

// Registers the application entry point for GLFM-based (mobile) builds.
// Call before the app thread starts, e.g. from a static initializer or the
// GLFM_GLFW_APP_MAIN macro.
void glfwmSetEntryPoint(glfwm_entry_fn entry);

// *Apple platforms only*: Returns a pointer to the GLFM view (an MTKView)
// backing the window, or NULL if Metal is unavailable or the view does not
// exist yet. The view's layer is a CAMetalLayer; presenting it is the
// application's responsibility (e.g. via a WebGPU surface created with
// glfwCreateWindowWGPUSurface).
void* glfwmGetMetalView(void);

// *Apple platforms only*: Returns the CAMetalLayer to render into, or NULL
// if Metal is unavailable. This is a dedicated layer managed by the backend
// (GLFM's MTKView draws underneath it, so the WebGPU surface owns the
// layer's drawable pool exclusively). Used internally by the surface bridge.
void* glfwmGetMetalLayer(void);

// ---- glfwmw (WebGPU flavor) ------------------------------------------------
// Present only in the glfwmw library (built with GLFWM_WGPU defined): glfwm
// plus wgpu-native and the WebGPU surface bridge.

#ifdef GLFWM_WGPU

#include <webgpu/webgpu.h>

// Creates a WGPUSurface for rendering into the given window. On GLFM (mobile)
// platforms the surface targets the backend's dedicated CAMetalLayer; on
// desktop platforms it uses the window's native surface (e.g. the CAMetalLayer
// of a Cocoa window). Requires a WGPUInstance created with wgpuCreateInstance().
WGPUSurface glfwmwCreateWindowWGPUSurface(WGPUInstance instance, GLFWwindow* window);

#endif // GLFWM_WGPU

#ifdef GLFM_GLFW_PLATFORM

// Registers fn as the application entry point at load time.
#define GLFM_GLFW_APP_MAIN(fn) \
    __attribute__((constructor)) static void glfwm__registerEntryPoint(void) \
    { \
        glfwmSetEntryPoint(fn); \
    }

#else

// Desktop build: the app keeps its own main(); the macro is inert.
#define GLFM_GLFW_APP_MAIN(fn) extern int fn(int argc, char** argv)

#endif

#ifdef __cplusplus
}
#endif

#endif // glfwm_h
