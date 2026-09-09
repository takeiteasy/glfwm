// glfwm smoke test: runs the same source on desktop (native GLFW) and on the
// GLFM platform backend (mobile / host development builds).
//
// Creates a NO_API window (WebGPU/Vulkan rendering is exercised by later
// milestones), logs events, and exits after 120 frames (override with the
// GLFM_HELLO_FRAMES env var; 0 or negative = run until the window closes).
//
// Input helpers for the M2 host regression (test/glfm_driver.m):
//   C - sets the clipboard to "glfwm-set"
//   V - prints glfwGetClipboardString (sync-emulating read)
// ESC closes the window.

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include "glfwm.h"

static void glfwm__error(int error, const char* description)
{
    fprintf(stderr, "GLFW error %i: %s\n", error, description);
}

static void glfwm__key(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    printf("key: key=%i scancode=%i action=%i mods=%i\n", key, scancode, action, mods);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (action == GLFW_PRESS && key == GLFW_KEY_C)
    {
        glfwSetClipboardString(window, "glfwm-set");
        printf("clipboard set: glfwm-set\n");
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_V)
    {
        const char* text = glfwGetClipboardString(window);
        printf("clipboard: %s\n", text ? text : "(null)");
    }
}

static void glfwm__framebufferSize(GLFWwindow* window, int width, int height)
{
    (void) window;
    printf("framebuffer size: %ix%i\n", width, height);
}

static void glfwm__cursorPos(GLFWwindow* window, double x, double y)
{
    (void) window;
    printf("cursor pos: %.1f,%.1f\n", x, y);
}

static void glfwm__mouseButton(GLFWwindow* window, int button, int action, int mods)
{
    (void) window;
    (void) mods;
    printf("mouse button: %i %s\n", button, action == GLFW_PRESS ? "press" : "release");
}

static void glfwm__windowFocus(GLFWwindow* window, int focused)
{
    (void) window;
    printf("window focus: %s\n", focused ? "true" : "false");
}

static void glfwm__windowIconify(GLFWwindow* window, int iconified)
{
    (void) window;
    printf("window iconify: %s\n", iconified ? "true" : "false");
}

static void glfwm__windowRefresh(GLFWwindow* window)
{
    (void) window;
    printf("window refresh\n");
}

static void glfwm__scroll(GLFWwindow* window, double x, double y)
{
    (void) window;
    printf("scroll: %+.3f %+.3f\n", x, y);
}

static void glfwm__charEvent(GLFWwindow* window, unsigned int codepoint)
{
    (void) window;
    printf("char: %u\n", codepoint);
}

static void glfwm__windowSize(GLFWwindow* window, int width, int height)
{
    (void) window;
    printf("window size cb: %ix%i\n", width, height);
}

int app_main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    glfwSetErrorCallback(glfwm__error);

#ifdef GLFW_PLATFORM_GLFM
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_GLFM);
#endif

    if (!glfwInit())
    {
        fprintf(stderr, "glfwInit failed\n");
        return EXIT_FAILURE;
    }

    printf("platform: %s (0x%x)\n",
           glfwGetPlatform() == GLFW_PLATFORM_GLFM ? "GLFM" : "other",
           glfwGetPlatform());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "glfwm hello", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSetKeyCallback(window, glfwm__key);
    glfwSetFramebufferSizeCallback(window, glfwm__framebufferSize);
    glfwSetCursorPosCallback(window, glfwm__cursorPos);
    glfwSetMouseButtonCallback(window, glfwm__mouseButton);
    glfwSetWindowFocusCallback(window, glfwm__windowFocus);
    glfwSetWindowIconifyCallback(window, glfwm__windowIconify);
    glfwSetWindowRefreshCallback(window, glfwm__windowRefresh);
    glfwSetScrollCallback(window, glfwm__scroll);
    glfwSetCharCallback(window, glfwm__charEvent);
    glfwSetWindowSizeCallback(window, glfwm__windowSize);

    int width, height, fbWidth, fbHeight;
    glfwGetWindowSize(window, &width, &height);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    printf("window size: %ix%i, framebuffer: %ix%i\n", width, height, fbWidth, fbHeight);

    const char* framesEnv = getenv("GLFM_HELLO_FRAMES");
    const int frameCount = framesEnv ? atoi(framesEnv) : 120;
    double startTime = glfwGetTime();
    int frames = 0;
    while (!glfwWindowShouldClose(window) &&
           (frameCount <= 0 || frames < frameCount))
    {
        glfwPollEvents();
        frames++;
    }
    double elapsed = glfwGetTime() - startTime;
    printf("%i frames in %.2fs (%.1f fps)\n", frames, elapsed,
           elapsed > 0.0 ? frames / elapsed : 0.0);

    glfwDestroyWindow(window);
    glfwTerminate();
    printf("ok\n");
    return EXIT_SUCCESS;
}

GLFM_GLFW_APP_MAIN(app_main)
