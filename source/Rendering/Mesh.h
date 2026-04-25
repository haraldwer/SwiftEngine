#pragma once
#include <webgpu/webgpu.h>

namespace Rendering
{
    struct MeshLOD
    {
        WGPUBuffer vertexBuffer = {};
        WGPUBuffer indexBuffer = {};
        uint32 vertexCount = 0;
        uint32 indexCount = 0;
        uint32 vertexStride = 0;
        uint32 indexStride = 0;
        WGPUIndexFormat indexFormat = WGPUIndexFormat_Uint32;
    };
    
    struct MeshTransformBuffer
    {
        WGPUBuffer buffer = nullptr;
        uint32_t count = 1;
    };
    
    struct MeshData
    {
        MeshLOD const* lod = nullptr;
        MeshTransformBuffer const* transforms = nullptr;
        
    };
    
    struct Mesh
    {
        Vector<MeshLOD> lods;
        MeshTransformBuffer transforms;
    };
    
    struct MeshState
    {
        WGPUPrimitiveState primitiveState;
        Vector<WGPUVertexBufferLayout> vertexLayouts;
        uint32 hash = 0;
    };
    
    struct PrimitiveData
    {
        MeshLOD mesh;
        MeshState state;
    };
}
