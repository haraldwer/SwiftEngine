#pragma once

#include <webgpu/webgpu.h>

#include "WindowConfig.h"
#include "Targets/RenderTarget.h"
#include "Input.h"

namespace Rendering
{
    class Window
    {
        friend class Context;
        friend class ImGuiContext;
    public:
        void Open(const WindowConfig& InConfig);
        void Close();
        RenderTarget& BeginFrame();
        void Present(bool& InRun);

        WindowConfig GetConfig() const { return config; }
        void QueueConfig(const WindowConfig& InConfig);
        
    private:
        void CreateWindow(const WindowConfig& InConfig);
        void DestroyWindow();
        void TryConfigure(bool InOnOpen);
        
        WindowConfig pendingConfig;
        WindowConfig config;
        
        WindowHandle window = nullptr;
        Input input = {};
        
        WGPUSurface surface = {};
        bool surfaceConfigured = false;
        WGPUSurfaceTexture surfaceTexture = {};
        RenderTarget target;
    };
}
