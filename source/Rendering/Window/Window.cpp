#include "Window.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "stb_image.h"

#include "Rendering/Manager.h"

#include "Rendering/Context/Context.h"

static void OnError(int InError, const char* InDesc)
{
    LOG("GLFW ERROR ", InError, ": ", InDesc);
}

static void OnFramebufferResize(GLFWwindow* window, int width, int height)
{
    auto& w = Rendering::Manager::Get().GetWindow();
    auto c = w.GetConfig();
    c.Size = { width, height };
    w.QueueConfig(c);
}

static void OnWindowMoved(GLFWwindow* window, int x, int y)
{
    auto& w = Rendering::Manager::Get().GetWindow();
    auto c = w.GetConfig();
    CHECK_RETURN(c.Fullscreen);
    c.Position = { x, y };
    w.QueueConfig(c);
}

void Rendering::Window::Open(const WindowConfig& InConfig)
{
    RN_PROFILE();
    
    glfwSetErrorCallback(OnError);

    CreateWindow(InConfig);
    
    surface = Context::Get().CreateWindowSurface(*this);
    
    TryConfigure(true);
    
#ifndef EMSCRIPTEN
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(window), OnFramebufferResize);
    glfwSetWindowPosCallback(static_cast<GLFWwindow*>(window), OnWindowMoved);
#else
    
#endif
}

void Rendering::Window::Close()
{
    RN_PROFILE();
    target.Deinit();
    wgpuSurfaceRelease(surface);
    surface = {};
    DestroyWindow();
}

void Rendering::Window::CreateWindow(const WindowConfig& InConfig)
{
    CHECK_ASSERT(window, "Window already opened")
    CHECK_ASSERT(surface, "Surface already exists")
    CHECK_ASSERT(InConfig.Size.Get().Min() <= 0, "Invalid resolution");
    config = InConfig;
    pendingConfig = InConfig;
    surfaceConfigured = false;
    
    LOG("Creating glfw window")
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, (config.Borderless || config.Fullscreen) ? GLFW_FALSE : GLFW_TRUE);
    
    if (config.Fullscreen)
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        config.Size = { mode->width, mode->height };
    }
        
    window = glfwCreateWindow(
        config.Size.Get().x, 
        config.Size.Get().y, 
        InConfig.Title.Get().c_str(), 
        nullptr,
        nullptr);
    CHECK_ASSERT(!window, "Failed to create window");
    
    input.Init(window);
}

void Rendering::Window::DestroyWindow()
{
    input.Deinit();
    glfwDestroyWindow(static_cast<GLFWwindow*>(window));
    window = nullptr;
}

Rendering::RenderTarget& Rendering::Window::BeginFrame()
{
    RN_PROFILE();
    
    TryConfigure(false);
    
    if (!surfaceConfigured)
    {
        Context::Get().Poll(true);
        Context::Get().ConfigureWindowSurface(*this);
        surfaceConfigured = true;
    }
    
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
    
    // Release surface texture
    target.Deinit();
    surfaceTexture.texture = {};
}

void Rendering::Window::QueueConfig(const WindowConfig& InConfig)
{
    pendingConfig = InConfig;
}

void Rendering::Window::TryConfigure(bool InOnOpen)
{
    CHECK_RETURN(config == pendingConfig && !InOnOpen);
    
    // Check fullscreen first, then recreate everything
    if ((config.Fullscreen != pendingConfig.Fullscreen || 
        config.Borderless != pendingConfig.Borderless) && !InOnOpen)
    {
        Context::Get().Poll(true);
        DestroyWindow();
        CreateWindow(pendingConfig);
        return;
    }
    
    if (config.Position != pendingConfig.Position || InOnOpen)
    {
        if (pendingConfig.Position == Vec2I::Zero() || config.Fullscreen) 
        {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            int x = (mode->width  - config.Size.Get().x) / 2;
            int y = (mode->height - config.Size.Get().y) / 2;
            glfwSetWindowPos(static_cast<GLFWwindow*>(window), x, y);
        } 
        else 
        {
            glfwSetWindowPos(
                static_cast<GLFWwindow*>(window), 
                config.Position.Get().x, 
                config.Position.Get().y);
        }
    }
    
    if ((config.Icon != pendingConfig.Icon || InOnOpen) && !pendingConfig.Icon.Get().empty())
    {
        int w, h, channels = 0;
        unsigned char* pixels = stbi_load(pendingConfig.Icon.Get().c_str(), &w, &h, &channels, 4);
        if (pixels) {
            GLFWimage icon{ w, h, pixels };
            glfwSetWindowIcon(static_cast<GLFWwindow*>(window), 1, &icon);
            stbi_image_free(pixels);
        }
    }

    if ((config.Title != pendingConfig.Title || InOnOpen) && !pendingConfig.Title.Get().empty())
        glfwSetWindowTitle(static_cast<GLFWwindow*>(window), config.Title.Get().c_str());
    
    
    if (config.Size != pendingConfig.Size && !InOnOpen)
    {
        Vec2I s;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window), &s.x, &s.y);
        if (s != pendingConfig.Size)
            glfwSetWindowSize(
                static_cast<GLFWwindow*>(window), 
                pendingConfig.Size.Get().x, 
                pendingConfig.Size.Get().y);
        
        surfaceConfigured = false;
    }
    
    config = pendingConfig;
}

