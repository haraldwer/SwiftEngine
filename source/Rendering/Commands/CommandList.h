#pragma once

#include "Uniforms/UniformBuffer.h"
#include "webgpu/webgpu.h"

namespace Rendering
{
    struct Command;

    class CommandList
    {
    public:
        void Begin(const String& InName);
        void Add(const Command& InCommand);
        void End();
        void Submit();
        
    private: 
        void RenderFullscreen(const Command& InCommand, const WGPURenderPassEncoder& renderPass);
        void RenderModel(const Command& InCommand, const WGPURenderPassEncoder& renderPass);

        String workingName = "";
        WGPUCommandEncoder encoder = {};
        Vector<WGPUCommandBuffer> commands;
    };
}
