#include "FrameTargets.h"

void Rendering::FrameTargets::Init(const Vec2I &InSize)
{
    RenderTarget::Desc msaaDesc{
        .size = Vec3I(InSize.x, InSize.y, 0),
        .multisample = 4
    };
    msaaFrame.Init(msaaDesc);
    
    // Resolve normals and depth manually!
    msaaDesc.msaaResolve = false; 
    msaaNormals.Init(msaaDesc);
    msaaDepth.Init({ 
        .size = Vec3I(InSize.x, InSize.y, 0),
        .format = WGPUTextureFormat_Depth16Unorm,
        .type = TextureType::DEPTH,
        .multisample = 4
    });
    
    ssao.Init({ .size = Vec3I(InSize.x, InSize.y, 0) });
}

void Rendering::FrameTargets::Deinit()
{
    msaaFrame.Deinit();
    msaaNormals.Deinit();
    msaaDepth.Deinit();
    
    ssao.Deinit();
}
