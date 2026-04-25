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

        Vec2I Size() const;
        void RequestResize(int width, int height);

    private:
        Vec2I pendingSize;
        void TryResize();
        
        WindowConfig config = {};
        WindowHandle window = nullptr;
        Input input = {};
        
        WGPUSurface surface;
        WGPUSurfaceTexture surfaceTexture;
        RenderTarget target;
    };
}
