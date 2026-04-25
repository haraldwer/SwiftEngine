#include "CommandList.h"

#include "Command.h"
#include "../Uniforms/UniformBuffer.h"
#include "Context/Context.h"
#include "Pipeline/PipelineCache.h"
#include "Targets/RenderTarget.h"

void Rendering::CommandList::Begin(const String &InName)
{
    RN_PROFILE();
    workingName = InName;
    
    // Create encoder
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = ToStr("Encoder: " + workingName);
    encoder = Context::Get().CreateEncoder(encoderDesc);
    
    // Add instructions
    wgpuCommandEncoderInsertDebugMarker(encoder, ToStr("Begin: " + workingName));
}

void Rendering::CommandList::Add(const Command& InCommand)
{
    RN_PROFILE();
    CHECK_ASSERT(InCommand.targets.empty(), "No targets for command");
    
    // Create attachments and views
    Vec3I maxSize;
    Vector<WGPURenderPassColorAttachment> attachments;
    attachments.reserve(InCommand.targets.size());
    for (auto& target : InCommand.targets)
    {
        CHECK_ASSERT(!target, "Invalid target");
        WGPURenderPassColorAttachment& attachment = attachments.emplace_back();
        
        if (target->descriptor.multisample > 1 && InCommand.multisample)
        {
            if (target->descriptor.msaaResolve)
            {
                attachment.view = target->msaaView;
                attachment.resolveTarget = target->view;
                attachment.loadOp = WGPULoadOp_Clear;
                attachment.storeOp = WGPUStoreOp_Discard;
            }
            else
            {
                attachment.view = target->msaaView;
                attachment.resolveTarget = nullptr;
                attachment.storeOp = WGPUStoreOp_Store;
            }
        }
        else
        {
            attachment.view = target->view;
            attachment.resolveTarget = nullptr;
            attachment.storeOp = WGPUStoreOp_Store;
        }
        
        attachment.loadOp = InCommand.clear ? WGPULoadOp_Clear : WGPULoadOp_Load;
        attachment.clearValue = { 
            InCommand.clearColor.x, 
            InCommand.clearColor.y, 
            InCommand.clearColor.z, 
            InCommand.clearColor.w 
        };
    #ifndef WEBGPU_BACKEND_WGPU
        attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    #endif
        
        maxSize = Vec3I::Max(maxSize, target->GetSize());
    }
    
    WGPURenderPassDepthStencilAttachment depthAttachment;
    if (InCommand.depthTarget)
    {
        CHECK_ASSERT(!InCommand.depthTarget->view, "Invalid depth view");
    
        if (InCommand.depthTarget->descriptor.multisample > 1)
        {
            depthAttachment.view = InCommand.depthTarget->msaaView;
            depthAttachment.depthStoreOp = WGPUStoreOp_Store;
        }
        else
        {
            depthAttachment.view = InCommand.depthTarget->view;
            depthAttachment.depthStoreOp = InCommand.writeDepth ? WGPUStoreOp_Store : WGPUStoreOp_Discard;
        }
    
        depthAttachment.depthLoadOp     = InCommand.clearDepth ? WGPULoadOp_Clear : WGPULoadOp_Load;
        depthAttachment.depthClearValue = 1.0f;
        depthAttachment.depthReadOnly   = !InCommand.writeDepth;
    
        // Stencil - set to undefined if not using stencil
        depthAttachment.stencilLoadOp  = WGPULoadOp_Undefined;
        depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
        depthAttachment.stencilReadOnly = true;
    }

    WGPURenderPassDescriptor renderPassDescriptor = {};
    renderPassDescriptor.label = ToStr("RenderPass: " + InCommand.name);
    renderPassDescriptor.colorAttachmentCount = attachments.size();
    renderPassDescriptor.colorAttachments = attachments.data();
    renderPassDescriptor.depthStencilAttachment = InCommand.depthTarget ? &depthAttachment : nullptr;
    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);
    
    wgpuRenderPassEncoderSetViewport(renderPass, 0, 0, maxSize.x, maxSize.y, 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(renderPass, 0, 0, maxSize.x, maxSize.y);
    
    InCommand.model.Identifier().IsValid() ? 
        RenderModel(InCommand, renderPass) : 
        RenderFullscreen(InCommand, renderPass);
    
    if (InCommand.customFunc)
        InCommand.customFunc(renderPass);
    
    // Use renderpass
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);
}

void Rendering::CommandList::RenderModel(const Command &InCommand, const WGPURenderPassEncoder& renderPass) 
{
    // Get material and mesh
    ModelResource* model = InCommand.model.Get();
    CHECK_RETURN(!model);
    const MeshState* meshState = model->GetMeshState();
    CHECK_RETURN(!meshState);
    
    // Get pipeline
    PipelineDescriptor pipelineDesc;
    pipelineDesc.label = InCommand.name;
    pipelineDesc.material = InCommand.material.Get();
    pipelineDesc.meshState = model->GetMeshState();
    pipelineDesc.data.multisampling = InCommand.targets.front()->descriptor.multisample;
    for (auto& t : InCommand.targets)
    {
        pipelineDesc.targetFormats.push_back(t->descriptor.format);
        CHECK_ASSERT(pipelineDesc.data.multisampling != t->descriptor.multisample, "Multisample state must match");
    }
    if (InCommand.depthTarget)
        pipelineDesc.data.depth.format = InCommand.depthTarget->descriptor.format;
    pipelineDesc.data.depth.write = InCommand.writeDepth;
    
    pipelineDesc.layout = InCommand.buffers ? 
        InCommand.buffers->GetLayout() : nullptr;
    
    WGPURenderPipeline* pipeline = PipelineCache::Get().GetPipeline(pipelineDesc);
    CHECK_RETURN(!pipeline);
    
    wgpuRenderPassEncoderSetPipeline(renderPass, *pipeline);
        
    // Bind buffers
    if (InCommand.buffers)
        CHECK_RETURN(!InCommand.buffers->Bind(renderPass));
        
    // For now, render all meshes
    // In the future: Combine mesh buffers? 
    int numMeshes = model->GetMeshCount();
    for (int i = 0; i < numMeshes; i++)
    {
        auto lod = model->GetMesh(i, 0);
        CHECK_CONTINUE(!lod);
        auto& vb = lod->vertexBuffer;
        auto& ib = lod->indexBuffer;
        CHECK_RETURN(!vb || !ib);
        
        // TODO: Bind mesh-specific buffers...            
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vb, 0, lod->vertexCount * lod->vertexStride);
        wgpuRenderPassEncoderSetIndexBuffer(renderPass, ib, lod->indexFormat, 0, lod->indexCount * lod->indexStride);
        wgpuRenderPassEncoderDrawIndexed(renderPass, lod->indexCount, 1, 0, 0, 0);
    }
}

void Rendering::CommandList::RenderFullscreen(const Command &InCommand, const WGPURenderPassEncoder& renderPass) 
{
    // Fullscreen pass
    // Get pipeline
    PipelineDescriptor pipelineDesc;
    pipelineDesc.label = InCommand.name;
    pipelineDesc.material = InCommand.material.Get();
    pipelineDesc.meshState = nullptr; 
    for (auto& t : InCommand.targets)
        pipelineDesc.targetFormats.push_back(t->descriptor.format);
    if (InCommand.depthTarget)
        pipelineDesc.data.depth.format = InCommand.depthTarget->descriptor.format;
    pipelineDesc.data.depth.write = InCommand.writeDepth;
    pipelineDesc.data.multisampling = false;
    pipelineDesc.layout = InCommand.buffers ? 
        InCommand.buffers->GetLayout() : nullptr;
    
    WGPURenderPipeline* pipeline = PipelineCache::Get().GetPipeline(pipelineDesc);
    CHECK_RETURN(!pipeline);
    
    if (InCommand.buffers)
        CHECK_RETURN(!InCommand.buffers->Bind(renderPass));
    
    wgpuRenderPassEncoderSetPipeline(renderPass, *pipeline);
    wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
}

void Rendering::CommandList::End()
{
    RN_PROFILE();
    wgpuCommandEncoderInsertDebugMarker(encoder, ToStr("End: " + workingName));
    
    WGPUCommandBufferDescriptor commandBufferDesc;
    commandBufferDesc.label = ToStr("Command: " + workingName);
    commands.emplace_back() = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
    wgpuCommandEncoderRelease(encoder);
    encoder = {};
    workingName = "";
}

void Rendering::CommandList::Submit()
{
    RN_PROFILE();
    Context::Get().Submit(commands);
    for (auto& cmd : commands)
        wgpuCommandBufferRelease(cmd);
    commands.clear();
}

