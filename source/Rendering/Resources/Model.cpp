#include "Model.h"

#define TINYOBJLOADER_IMPLEMENTATION

#include "Loaders/ModelLoaderOBJ.h"
#include "Loaders/ModelLoaderGLTF.h"

#include "Context/Context.h"

#include "Tasks/Task.h"

bool Rendering::ModelResource::Load()
{
    CHECK_ASSERT(!meshes.empty(), "Model already loaded");
    if (asyncLoadStarted)
        return true;
    
    String path = id.Str();
    if (id.Str() == "Cube") path = "Defaults/M_Cube.obj";
    if (id.Str() == "Cylinder") path = "Defaults/M_Cylinder.obj";
    if (id.Str() == "Sphere") path = "Defaults/M_Cylinder.obj";
    
    CHECK_RETURN_LOG(!Utility::File::Exists(path), "Model does not exist: " + path, false)
    
    asyncLoadStarted = true;
    loadCount++;
    LOG("Loading model: ", path);
    if (path.ends_with(".obj"))
        Tasks::Enqueue<Resource::ID, OBJModelLoadResult>(id, &AsyncLoadModelOBJ);
    else if (path.ends_with(".gltf"))
        Tasks::Enqueue<Resource::ID, GLTFModelLoadResult>(id, &AsyncLoadModelGLTF);
    else
    {
        LOG("Unknown model type: " + path);
        return false;
    }
    return true;
}

static Rendering::MeshTransformBuffer LoadTransformBuffer(const Vector<Mat4F>& InTransforms)
{
    if (InTransforms.empty() || (InTransforms.size() == 1 && InTransforms.at(0) == Mat4F()))
    {
        // Use a default buffer!
        return {};
    }
    
    Rendering::MeshTransformBuffer result;
    WGPUBufferDescriptor desc = {};
    desc.size  = InTransforms.size() * sizeof(Mat4F);
    desc.usage = WGPUBufferUsage_Storage;
    desc.mappedAtCreation = true;
    result.buffer = Rendering::Context::Get().CreateBuffer(desc);
    auto gpuMem = wgpuBufferGetMappedRange(result.buffer, 0, desc.size);
    memcpy(gpuMem, InTransforms.data(), desc.size);
    wgpuBufferUnmap(result.buffer);
    result.count = InTransforms.size();
    return result;
}

void Rendering::ModelResource::ContinueLoad()
{
    if (loadCount == 0)
        return;
    if (!asyncLoadStarted)
        return;
    
    Tasks::Consume<Resource::ID, OBJModelLoadResult>([&](const Resource::ID& InID, const OBJModelLoadResult& InResult)
    {
        CHECK_RETURN(InID != id, false);
        loadCount--;
        
        CHECK_RETURN_LOG(!InResult.success, "Failed to load model: " + id.Str(), false)
        CHECK_RETURN_LOG(!InResult.reader, "Reader invalid: " + id.Str(), false)

        LoadState();
        auto& shapes = InResult.reader->GetShapes();
        meshes.resize(shapes.size());
        
        for (int i = 0; i < shapes.size(); i++)
        {
            meshes.at(i).lods.resize(numGenLODs + 1);
            
            // Start new tasks
            loadCount++;
            OBJMeshLoadParams params;
            params.reader = InResult.reader;
            params.id = id;
            params.shapeIndex = i;
            Tasks::Enqueue<OBJMeshLoadParams, ObjectPtr<MeshContent>>(params, &AsyncLoadMeshOBJ);
        }
        return true;
    }, 1);
    
    Tasks::Consume<Resource::ID, GLTFModelLoadResult>([&](const Resource::ID& InID, const GLTFModelLoadResult& InResult)
    {
        CHECK_RETURN(InID != id, false);
        loadCount--;
            
        CHECK_RETURN_LOG(!InResult.success, "Failed to load model: " + id.Str(), false)
        CHECK_RETURN_LOG(!InResult.reader, "Reader invalid: " + id.Str(), false)
        
        LoadState();
        auto& shapes = InResult.reader->meshes;
        meshes.resize(shapes.size());
        for (int i = 0; i < shapes.size(); i++)
        {
            auto& mesh = meshes.at(i);
            mesh.lods.resize(numGenLODs + 1);
            if (InResult.transforms.contains(i))
                mesh.transforms = LoadTransformBuffer(InResult.transforms.at(i));
            
            // Start new tasks
            loadCount++;
            GLTFMeshLoadParams params;
            params.reader = InResult.reader;
            params.id = id;
            params.shapeIndex = i;
            Tasks::Enqueue<GLTFMeshLoadParams, ObjectPtr<MeshContent>>(params, &AsyncLoadMeshGLTF);
        }
        
        // Possibly also load textures?
        return true;
    }, 1);
    
    Tasks::Consume<OBJMeshLoadParams, ObjectPtr<MeshContent>>([&](const OBJMeshLoadParams& InParams, const ObjectPtr<MeshContent>& InData)
    {
        CHECK_RETURN(InParams.id != id.Str(), false);
        loadCount--;
        CHECK_RETURN_LOG(!InData, "Data invalid: " + InParams.id.Str(), false);
        
        if (InData->vertices.empty() || InData->indices.empty())
        {
            LOG("Failed to load shape: " + id.Str() + ":" + Utility::ToStr(InParams.shapeIndex));
            return true;
        }
        
        CHECK_ASSERT(InParams.shapeIndex >= meshes.size(), "Invalid shapeIndex")
        for (int lod = 0; lod <= numGenLODs; lod++)
        {
            loadCount++;
            LODLoadParams params { 
                id, 
                InParams.shapeIndex,
                lod,
                InData
            };
            Tasks::Enqueue<LODLoadParams, LODData>(params, &AsyncLoadLOD);
        }
        return true;
    }, 1);
    

    Tasks::Consume<GLTFMeshLoadParams, ObjectPtr<MeshContent>>([&](const GLTFMeshLoadParams& InParams, const ObjectPtr<MeshContent>& InData)
    {
        CHECK_RETURN(InParams.id != id.Str(), false);
        loadCount--;
        CHECK_RETURN_LOG(!InData, "Data invalid: " + InParams.id.Str(), false);
        
        if (InData->vertices.empty() || InData->indices.empty())
        {
            LOG("Failed to load shape: " + id.Str() + ":" + Utility::ToStr(InParams.shapeIndex));
            return true;
        }
        
        CHECK_ASSERT(InParams.shapeIndex >= meshes.size(), "Invalid shapeIndex")
        for (int lod = 0; lod <= numGenLODs; lod++)
        {
            loadCount++;
            LODLoadParams params { 
                id, 
                InParams.shapeIndex,
                lod,
                InData
            };
            Tasks::Enqueue<LODLoadParams, LODData>(params, &AsyncLoadLOD);
        }
        return true;
    }, 1);
    
    Tasks::Consume<LODLoadParams, LODData>([&](const LODLoadParams& InParams, const LODData& InResult)
    {
        CHECK_RETURN(InParams.id != id.Str(), false);
        loadCount--;
        
        CHECK_ASSERT(InParams.shapeIndex >= static_cast<int>(meshes.size()), "Invalid shapeIndex")
        auto& mesh = meshes.at(InParams.shapeIndex);
        CHECK_ASSERT(InParams.lodIndex >= static_cast<int>(mesh.lods.size()), "Invalid lodIndex")
        
        auto& vertices = InResult.vertices;
        auto& indices = InResult.indices;
        
        MeshLOD& lod = mesh.lods.at(InParams.lodIndex);
        lod.vertexCount = static_cast<uint32>(vertices.size());
        lod.indexCount = static_cast<uint32>(indices.size());
        lod.vertexStride = sizeof(VertexLayout);
        
        WGPUBufferDescriptor vertexBufferDesc = {};
        vertexBufferDesc.label = ToStr("LOD" + Utility::ToStr(InParams.lodIndex + 1) + " VB: " + id.Str());
        vertexBufferDesc.size = vertices.size() * sizeof(VertexLayout);
        vertexBufferDesc.usage = WGPUBufferUsage_Vertex;
        lod.vertexBuffer = Context::Get().CreateBuffer(vertexBufferDesc);
        Context::Get().WriteBuffer(lod.vertexBuffer,vertices.data(),vertices.size() * sizeof(VertexLayout));

        bool canUseUint16 = (lod.vertexCount <= std::numeric_limits<uint16_t>::max());
        std::vector<uint16_t> indices16;
        if (canUseUint16)
        {
            indices16.reserve(indices.size());
            for (uint32 idx : indices)
            {
                if (idx > std::numeric_limits<uint16_t>::max())
                {
                    canUseUint16 = false;
                    break;
                }
                indices16.push_back(static_cast<uint16_t>(idx));
            }
        }
        
        const void* data = canUseUint16 ? static_cast<const void *>(indices16.data()) : static_cast<const void *>(indices.data());
        String ibType = canUseUint16 ? " IB16: " : " IB32: ";
        uint32 stride = canUseUint16 ? sizeof(uint16_t) : sizeof(uint32);
        size_t sizeBytes = (canUseUint16 ? indices16.size() : indices.size()) * stride;
        lod.indexFormat = canUseUint16 ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32;
        lod.indexStride = stride;
        WGPUBufferDescriptor indexBufferDesc = {};
        indexBufferDesc.label = ToStr("LOD" + Utility::ToStr(InParams.lodIndex + 1 + 1) + ibType + id.Str());
        indexBufferDesc.size = sizeBytes;
        indexBufferDesc.usage = WGPUBufferUsage_Index;
        lod.indexBuffer = Context::Get().CreateBuffer(indexBufferDesc);
        Context::Get().WriteBuffer(lod.indexBuffer,data,sizeBytes);
        
        return true;
    }, 1);
}

void Rendering::ModelResource::LoadState() 
{
    state = {};
    state.primitiveState.topology = WGPUPrimitiveTopology_TriangleList;
    state.primitiveState.frontFace = WGPUFrontFace_CCW;
    state.primitiveState.cullMode = WGPUCullMode_Back;
    state.primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
    
    // Create default layout
    auto& layout = state.vertexLayouts.emplace_back();
    static Vector<WGPUVertexAttribute> attributes;
    if (attributes.empty())
    {
        // TODO: Make this cleaner
        attributes.resize(3);
        auto& positionAttr = attributes.at(0);
        positionAttr.format = WGPUVertexFormat_Float32x3;
        positionAttr.offset = 0;
        positionAttr.shaderLocation = 0;
        auto& normalAttr = attributes.at(1);
        normalAttr.format = WGPUVertexFormat_Float32x3;
        normalAttr.offset = 3 * sizeof(float);
        normalAttr.shaderLocation = 1;
        auto& uvAttr = attributes.at(2);
        uvAttr.format = WGPUVertexFormat_Float32x2;
        uvAttr.offset = 2 * 3 * sizeof(float);
        uvAttr.shaderLocation = 2;
    }
    layout.attributes = attributes.data();
    layout.attributeCount = static_cast<uint32>(attributes.size());
    layout.arrayStride = sizeof(VertexLayout);
    layout.stepMode = WGPUVertexStepMode_Vertex;
    
    // TODO: Instanced attributes
    
    struct HashData
    {
        uint32 prim = 0;
        uint32 attr = 0;
    } hashData;
    hashData.prim = Utility::Hash(state.primitiveState);
    hashData.attr = Utility::Hash(attributes);
    state.hash = Utility::Hash(hashData);
}

bool Rendering::ModelResource::Unload()
{
    if (!meshes.empty())
    {
        for (auto& mesh : meshes)
        {
            for (auto& lod : mesh.lods)
            {
                if (lod.vertexBuffer) wgpuBufferRelease(lod.vertexBuffer);
                if (lod.indexBuffer) wgpuBufferRelease(lod.indexBuffer);
            }
            if (mesh.transforms.buffer)
                wgpuBufferDestroy(mesh.transforms.buffer);
            mesh.transforms = {};
        }
        meshes.clear();
    }
    loadCount = 0;
    asyncLoadStarted = false;
    return true;
}

bool Rendering::ModelResource::Edit(const String &InName, uint32 InOffset)
{
    return false;
}

const Rendering::MeshState* Rendering::ModelResource::GetMeshState()
{
    ContinueLoad();
    if (state.hash > 0)
        return &state;
    return nullptr;
}

Rendering::MeshData Rendering::ModelResource::GetMesh(const int InMeshIndex, const int InLOD)
{
    ContinueLoad();
    CHECK_RETURN(InMeshIndex < 0 || InMeshIndex >= static_cast<int>(meshes.size()), {})
    auto& mesh = meshes.at(InMeshIndex); 
    const int lod = Utility::Math::Clamp(InLOD, 0, static_cast<int>(mesh.lods.size()) - 1);
    return {
        &mesh.lods.at(lod),
        &mesh.transforms
    };
}

int Rendering::ModelResource::GetMeshCount()
{
    ContinueLoad();
    return static_cast<int>(meshes.size());
}

void Rendering::ModelDefaults::Init()
{
    WGPUBufferDescriptor desc = {};
    desc.size  = sizeof(Mat4F);
    desc.usage = WGPUBufferUsage_Storage;
    desc.mappedAtCreation = true;
    defaultBuffer.buffer = Context::Get().CreateBuffer(desc);
    auto gpuMem = wgpuBufferGetMappedRange(defaultBuffer.buffer, 0, desc.size);
    Mat4F data = Mat4F();
    memcpy(gpuMem, data.data, desc.size);
    wgpuBufferUnmap(defaultBuffer.buffer);
    defaultBuffer.count = 1;
}

void Rendering::ModelDefaults::Deinit()
{
    CHECK_ASSERT(!defaultBuffer.buffer, "Invalid buffer");
    wgpuBufferDestroy(defaultBuffer.buffer);
    defaultBuffer.buffer = nullptr;
    defaultBuffer.count = 0;
}
