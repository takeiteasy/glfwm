//========================================================================
// glfwmw example - WebGPU triangle
//------------------------------------------------------------------------
// A plain GLFW + WebGPU program: it runs unchanged on desktop GLFW builds
// and on GLFM (mobile) builds. The glfwmw flavor provides the surface
// (CAMetalLayer on Apple, including the GLFM platform) via
// glfwmwCreateWindowWGPUSurface().
//
// The loop is the standard GLFW pattern; on GLFM platforms glfwPollEvents
// is paced to the display refresh by the backend. Compiled with GLFWM_WGPU
// defined (the glfwmw flavor); the surface API lives in the guarded section
// of glfwm.h, which pulls in webgpu.h.
//
// NOTE (portability): adapter/device requests spin on
// wgpuInstanceProcessEvents, which is correct for native builds. An
// Emscripten target (emdawnwebgpu) needs a browser-yielding wait instead.
//========================================================================

#include <GLFW/glfw3.h>
#include "glfwm.h"

#include <stdio.h>
#include <stdlib.h>

static const char* TRIANGLE_WGSL =
    "struct VertexOut {\n"
    "    @builtin(position) pos : vec4f,\n"
    "    @location(0) color : vec3f,\n"
    "}\n"
    "@vertex fn vs(@builtin(vertex_index) i : u32) -> VertexOut {\n"
    "    var p = array<vec2f, 3>(vec2f(-0.5, -0.5), vec2f(0.5, -0.5), vec2f(0.0, 0.5));\n"
    "    var c = array<vec3f, 3>(vec3f(1.0, 0.0, 0.0), vec3f(0.0, 1.0, 0.0), vec3f(0.0, 0.0, 1.0));\n"
    "    var out : VertexOut;\n"
    "    out.pos = vec4f(p[i], 0.0, 1.0);\n"
    "    out.color = c[i];\n"
    "    return out;\n"
    "}\n"
    "@fragment fn fs(in : VertexOut) -> @location(0) vec4f {\n"
    "    return vec4f(in.color, 1.0);\n"
    "}\n";

typedef struct RequestState
{
    int done;
    void* handle;               // WGPUAdapter or WGPUDevice
    WGPUStringView message;
} RequestState;

static void onAdapterRequested(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                               WGPUStringView message, void* userdata1, void* userdata2)
{
    RequestState* state = (RequestState*) userdata1;
    (void) userdata2;
    state->done = 1;
    state->message = message;
    if (status == WGPURequestAdapterStatus_Success)
        state->handle = (void*) adapter;
    else
        fprintf(stderr, "adapter request failed: %.*s\n",
                (int) message.length, message.data ? message.data : "");
}

static void onDeviceRequested(WGPURequestDeviceStatus status, WGPUDevice device,
                              WGPUStringView message, void* userdata1, void* userdata2)
{
    RequestState* state = (RequestState*) userdata1;
    (void) userdata2;
    state->done = 1;
    state->message = message;
    if (status == WGPURequestDeviceStatus_Success)
        state->handle = (void*) device;
    else
        fprintf(stderr, "device request failed: %.*s\n",
                (int) message.length, message.data ? message.data : "");
}

// Spins the instance event loop until the callback fires.
static void waitRequest(WGPUInstance instance, RequestState* state)
{
    while (!state->done)
        wgpuInstanceProcessEvents(instance);
}

// Returns the surface's preferred texture format.
static WGPUTextureFormat configureSurface(WGPUSurface surface, WGPUAdapter adapter,
                                          WGPUDevice device, uint32_t width, uint32_t height)
{
    WGPUSurfaceCapabilities capabilities;
    wgpuSurfaceGetCapabilities(surface, adapter, &capabilities);
    WGPUTextureFormat format = capabilities.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(capabilities);

    WGPUSurfaceConfiguration config = {
        .device = device,
        .format = format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .width = width,
        .height = height,
        .viewFormatCount = 0,
        .viewFormats = NULL,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .presentMode = WGPUPresentMode_Fifo,
    };
    wgpuSurfaceConfigure(surface, &config);
    return format;
}

int app_main(int argc, char** argv)
{
    (void) argc; (void) argv;

    setvbuf(stdout, NULL, _IONBF, 0);

    if (!glfwInit())
    {
        fprintf(stderr, "glfwInit failed\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "glfwm triangle", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    WGPUInstanceDescriptor instanceDescriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
    WGPUInstance instance = wgpuCreateInstance(&instanceDescriptor);
    if (!instance)
    {
        fprintf(stderr, "wgpuCreateInstance failed\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    WGPUSurface surface = glfwmwCreateWindowWGPUSurface(instance, window);
    if (!surface)
    {
        fprintf(stderr, "glfwCreateWindowWGPUSurface failed\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    RequestState adapterState = {0};
    WGPURequestAdapterOptions adapterOptions = {
        .compatibleSurface = surface,
    };
    WGPURequestAdapterCallbackInfo adapterCallbackInfo = {
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = onAdapterRequested,
        .userdata1 = &adapterState,
    };
    wgpuInstanceRequestAdapter(instance, &adapterOptions, adapterCallbackInfo);
    waitRequest(instance, &adapterState);
    WGPUAdapter adapter = (WGPUAdapter) adapterState.handle;
    if (!adapter)
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    RequestState deviceState = {0};
    WGPUDeviceDescriptor deviceDescriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    WGPURequestDeviceCallbackInfo deviceCallbackInfo = {
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = onDeviceRequested,
        .userdata1 = &deviceState,
    };
    wgpuAdapterRequestDevice(adapter, &deviceDescriptor, deviceCallbackInfo);
    waitRequest(instance, &deviceState);
    WGPUDevice device = (WGPUDevice) deviceState.handle;
    if (!device)
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    WGPUShaderSourceWGSL shaderSource = {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = { TRIANGLE_WGSL, WGPU_STRLEN },
    };
    WGPUShaderModuleDescriptor shaderDescriptor = {
        .nextInChain = &shaderSource.chain,
        .label = { "triangle shader", WGPU_STRLEN },
    };
    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(device, &shaderDescriptor);

    WGPUColorTargetState colorTarget = {
        .format = WGPUTextureFormat_Undefined,      // set on first configure
        .blend = NULL,
        .writeMask = WGPUColorWriteMask_All,
    };
    WGPUFragmentState fragment = {
        .module = shader,
        .entryPoint = { "fs", WGPU_STRLEN },
        .targetCount = 1,
        .targets = &colorTarget,
    };
    WGPURenderPipelineDescriptor pipelineDescriptor = {
        .label = { "triangle pipeline", WGPU_STRLEN },
        .layout = NULL,                             // automatic layout
        .vertex = {
            .module = shader,
            .entryPoint = { "vs", WGPU_STRLEN },
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None,
        },
        .multisample = {
            .count = 1,
            .mask = 0xFFFFFFFF,
        },
        .fragment = &fragment,
    };

    WGPURenderPipeline pipeline = NULL;
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    int fbWidth = 0, fbHeight = 0;
    unsigned long frames = 0;
    double lastReport = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0)
            continue;
        if (width != fbWidth || height != fbHeight)
        {
            fbWidth = width;
            fbHeight = height;
            colorTarget.format = configureSurface(surface, adapter, device,
                                                  (uint32_t) fbWidth, (uint32_t) fbHeight);
        }

        // The pipeline depends on the target format; create it once the
        // surface has been configured (and re-create if the format changes).
        if (!pipeline)
            pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDescriptor);

        WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
        wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
        // NOTE: the first few frames may report an implementation-specific
        // status (wgpu-native) with no texture; skip until acquisition
        // reports success.
        if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
            surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
            continue;
        if (!surfaceTexture.texture)
            continue;

        WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, NULL);

        WGPURenderPassColorAttachment colorAttachment = {
            .view = view,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = { 0.1, 0.2, 0.3, 1.0 },
        };
        WGPURenderPassDescriptor renderPass = {
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
        };

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);
        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPass);
        wgpuRenderPassEncoderSetPipeline(pass, pipeline);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);
        WGPUCommandBufferDescriptor commandBufferDescriptor = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &commandBufferDescriptor);
        wgpuQueueSubmit(queue, 1, &commands);
        wgpuCommandBufferRelease(commands);
        wgpuRenderPassEncoderRelease(pass);
        wgpuCommandEncoderRelease(encoder);

        // Present before releasing the texture/view: the surface must keep
        // its reference on the drawable until presentation is scheduled.
        WGPUStatus presentStatus = wgpuSurfacePresent(surface);
        if (presentStatus != WGPUStatus_Success)
            fprintf(stderr, "present status=%d\n", (int) presentStatus);

        wgpuTextureViewRelease(view);
        wgpuTextureRelease(surfaceTexture.texture);

        frames++;
        double now = glfwGetTime();
        if (now - lastReport >= 2.0)
        {
            printf("%lu frames in %.2fs (%.1f fps)\n",
                   frames, now - lastReport, frames / (now - lastReport));
            frames = 0;
            lastReport = now;
        }
    }

    if (pipeline)
        wgpuRenderPipelineRelease(pipeline);
    glfwDestroyWindow(window);
    glfwTerminate();
    printf("ok\n");
    return EXIT_SUCCESS;
}

GLFM_GLFW_APP_MAIN(app_main)
