//========================================================================
// glfwm - GLFM platform backend for GLFW 3.6: monitor
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

// The GLFM platform has exactly one display: the device screen.
//
void _glfwPollMonitorsGlfm(void)
{
    if (_glfw.glfm.monitor)
        return;

    GLFMDisplay* display = _glfmGlfmDisplay();
    if (!display)
        return;

    int width = 0, height = 0;
    glfmGetDisplaySize(display, &width, &height);
    double scale = glfmGetDisplayScale(display);
    if (scale <= 0.0)
        scale = 1.0;

    // Estimate physical size from logical points (~163 ppi reference device)
    int widthMM = (int) ((width / scale) * 25.4f / 163.f);
    int heightMM = (int) ((height / scale) * 25.4f / 163.f);
    if (widthMM <= 0)
        widthMM = 1;
    if (heightMM <= 0)
        heightMM = 1;

    _GLFWmonitor* monitor = _glfwAllocMonitor("Device Display", widthMM, heightMM);
    if (!monitor)
        return;

    _glfw.glfm.monitor = monitor;
    _glfwInputMonitor(monitor, GLFW_CONNECTED, _GLFW_INSERT_FIRST);
}

void _glfwFreeMonitorGlfm(_GLFWmonitor* monitor)
{
    if (monitor)
        _glfwFreeGammaArrays(&monitor->currentRamp);
}

void _glfwGetMonitorPosGlfm(_GLFWmonitor* monitor, int* xpos, int* ypos)
{
    (void) monitor;
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;
}

void _glfwGetMonitorContentScaleGlfm(_GLFWmonitor* monitor,
                                     float* xscale, float* yscale)
{
    (void) monitor;
    GLFMDisplay* display = _glfmGlfmDisplay();
    float scale = display ? (float) glfmGetDisplayScale(display) : 1.f;
    if (scale <= 0.f)
        scale = 1.f;
    if (xscale)
        *xscale = scale;
    if (yscale)
        *yscale = scale;
}

void _glfwGetMonitorWorkareaGlfm(_GLFWmonitor* monitor,
                                 int* xpos, int* ypos,
                                 int* width, int* height)
{
    (void) monitor;
    GLFMDisplay* display = _glfmGlfmDisplay();
    int w = 0, h = 0;
    if (display)
        glfmGetDisplaySize(display, &w, &h);
    double scale = display ? glfmGetDisplayScale(display) : 1.0;
    if (scale <= 0.0)
        scale = 1.0;

    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;
    if (width)
        *width = (int) (w / scale);
    if (height)
        *height = (int) (h / scale);
}

GLFWvidmode* _glfwGetVideoModesGlfm(_GLFWmonitor* monitor, int* found)
{
    (void) monitor;
    *found = 1;
    return calloc(1, sizeof(GLFWvidmode));
}

GLFWbool _glfwGetVideoModeGlfm(_GLFWmonitor* monitor, GLFWvidmode* mode)
{
    (void) monitor;
    GLFMDisplay* display = _glfmGlfmDisplay();
    int w = 0, h = 0;
    if (display)
        glfmGetDisplaySize(display, &w, &h);
    if (mode)
    {
        mode->width = w;
        mode->height = h;
        mode->redBits = 8;
        mode->greenBits = 8;
        mode->blueBits = 8;
        mode->refreshRate = 60;
    }
    return GLFW_TRUE;
}

GLFWbool _glfwGetGammaRampGlfm(_GLFWmonitor* monitor, GLFWgammaramp* ramp)
{
    (void) monitor;
    if (ramp)
    {
        // Identity ramp
        unsigned short value[256];
        for (int i = 0;  i < 256;  i++)
            value[i] = (unsigned short) (i * 257);
        _glfwAllocGammaArrays(ramp, 256);
        memcpy(ramp->red, value, sizeof(value));
        memcpy(ramp->green, value, sizeof(value));
        memcpy(ramp->blue, value, sizeof(value));
    }
    return GLFW_TRUE;
}

void _glfwSetGammaRampGlfm(_GLFWmonitor* monitor, const GLFWgammaramp* ramp)
{
    (void) monitor;
    (void) ramp;
}
