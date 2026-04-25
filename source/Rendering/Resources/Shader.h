#pragma once
#include "webgpu/webgpu.h"

struct Shader;

namespace Rendering
{
    class ShaderResource : public Resource::Base
    {
        friend class PipelineCache;
        
        CLASS_INFO(ShaderResource, Resource::Base)
        
    public:
        bool Load() override;
        bool Unload() override;
        static bool Accept(const String& InPath);
        Utility::Timepoint GetEditTime() const override;

    private:
        static String LoadShaderFile(const String &InPath, Set<String> &InIncludes);
        static String ProcessIncludes(const String &InShaderCode, const String &InPath, Set<String> &InIncludes);
        static String ProcessDefines(const String &InShaderCode);

        Set<String> includes;
        WGPUShaderModule shader;
    };
}

typedef Resource::Ref<Rendering::ShaderResource> ResShader;
