#include "Window.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "Rendering/Manager.h"

#include "Rendering/Context/Context.h"

// The callback signature must match exactly
static void OnFramebufferResize(GLFWwindow* window, int width, int height)
{
    Rendering::Manager::Get().GetWindow().RequestResize(width, height);
}

void Rendering::Window::Open(const WindowConfig& InConfig)
{
    RN_PROFILE();
    CHECK_ASSERT(window, "Window already opened")
    auto res = InConfig.Resolution.Get();
    CHECK_ASSERT(res.Min() <= 0, "Invalid resolution");
    config = InConfig;
    pendingSize = res;
    
    LOG("Creating glfw window")
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(
        res.x, 
        res.y, 
        InConfig.Title.Get().c_str(), 
        nullptr,
        nullptr);
    CHECK_ASSERT(!window, "Failed to create window");
    input.Init(window);
    
    surface = Context::Get().CreateWindowSurface(*this);
    
#ifndef EMSCRIPTEN
    // TODO: Handle glfw events!
    // Window resizing etc
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(window), OnFramebufferResize);
#else
    
#endif
}

void Rendering::Window::Close()
{
    RN_PROFILE();
    target.Deinit();
    wgpuSurfaceRelease(surface);
    surface = {};
    input.Deinit();
    glfwDestroyWindow(static_cast<GLFWwindow*>(window));
    window = nullptr;
}

Rendering::RenderTarget& Rendering::Window::BeginFrame()
{
    RN_PROFILE();
    
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    CHECK_ASSERT(surfaceTexture.status > WGPUSurfaceGetCurrentTextureStatus_Timeout, "Failed to get surface texture");
    
    target.Deinit();
    target.Init(surfaceTexture.texture);
    return target;
}

void Rendering::Window::Present(bool& InRun)
{
    RN_PROFILE();
#ifndef EMSCRIPTEN
    const WGPUStatus status = wgpuSurfacePresent(surface);
    if (status != WGPUStatus_Success)
        LOG("Failed to present frame");
#endif
    
    // Also poll events
    input.Frame();
    Context::Get().Poll();
    
#ifndef EMSCRIPTEN
    glfwPollEvents();
    if (glfwWindowShouldClose(static_cast<GLFWwindow*>(window)))
        InRun = false;
#endif
    
    TryResize();
    
    // Release surface texture
    target.Deinit();
    surfaceTexture.texture = {};
}

Vec2I Rendering::Window::Size() const
{
    return config.Resolution.Get();
}

void Rendering::Window::TryResize()
{
    CHECK_RETURN(pendingSize == config.Resolution.Get())
    Context::Get().Poll(true);
    config.Resolution = pendingSize;
    Context::Get().ConfigureWindowSurface(*this);
}

void Rendering::Window::RequestResize(const int width, const int height)
{
    pendingSize = Vec2I::Max({ width, height }, {1, 1});
}
