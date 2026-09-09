//========================================================================
// glfwmw - WebGPU surface creation (glfwmw flavor only)
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
// Brand-neutral wrapper around the WebGPU surface bridge
// (deps/glfw3webgpu, patched for the GLFM platform). Compiled into every
// glfwmw artifact: the merged static libraries for GLFM builds (mobile +
// macOS host) and the desktop shim dylib. glfwm (non-wgpu) builds never
// compile this file.
//
//========================================================================

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <glfw3webgpu.h>
#include "glfwm.h"

#ifdef GLFWM_WGPU

WGPUSurface glfwmwCreateWindowWGPUSurface(WGPUInstance instance, GLFWwindow* window)
{
    return glfwCreateWindowWGPUSurface(instance, window);
}

#endif // GLFWM_WGPU
