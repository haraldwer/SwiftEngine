 #pragma once

#include "meshoptimizer.h"

constexpr int numGenLODs = 2;
constexpr int maxLODLevel = 20;
constexpr float targetError = 1e-1f; // allow large geometric error => fewer triangles

 
struct VertexLayout
{
    // TODO: Maybe compact to float16
    float px, py, pz = 0;
    float nx, ny, nz = 0;
    float u, v = 0;
};

struct VertexKey
{
    int v = -1;
    int n = -1;
    int t = -1;

    bool operator==(const VertexKey& o) const noexcept
    {
        return 
            v == o.v && 
            n == o.n && 
            t == o.t;
    }
};

struct VertexKeyHash
{
    size_t operator()(const VertexKey& k) const noexcept
    {
        // Simple hash combine for 3 ints
        size_t h1 = std::hash<int>{}(k.v);
        size_t h2 = std::hash<int>{}(k.n);
        size_t h3 = std::hash<int>{}(k.t);
        size_t h = h1;
        h ^= h2 + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= h3 + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
    
struct MeshData
{
    Vector<VertexLayout> vertices;
    Vector<uint32> indices;
    std::unordered_map<VertexKey, uint32, VertexKeyHash> remap;
};


struct LODLoadParams
{
    Resource::ID id;
    int shapeIndex = 0;
    int lodIndex = 0;
    ObjectPtr<MeshData> data;
};

struct LODData
{
    Vector<VertexLayout> vertices;
    Vector<uint32> indices;
};

inline LODData AsyncLoadLOD(const LODLoadParams& InParams)
{
    LOG("Loading LOD")
    
    // Compute LOD level (higher = more reduction)
    const int lodLevel = (InParams.lodIndex == 0) ? 1 : (maxLODLevel / (numGenLODs - InParams.lodIndex + 1));

    auto& vertices = InParams.data->vertices;
    auto& indices = InParams.data->indices;
    
    const size_t indexCount = indices.size();
    size_t targetIndexCount = indexCount / static_cast<size_t>(lodLevel);
    targetIndexCount = (targetIndexCount / 3) * 3; // must be multiple of 3
    targetIndexCount = Utility::Math::Clamp(
        targetIndexCount,
        static_cast<size_t>(3),
        indexCount);

    std::vector<uint32> lodIndices;
    lodIndices.resize(indexCount);

    if (lodLevel == 1)
    {
        // LOD0: keep original topology; skip simplify entirely
        std::memcpy(lodIndices.data(), indices.data(), indexCount * sizeof(uint32));
        lodIndices.resize(indexCount);
    }
    else
    {
        float resultError = 0;
        size_t lodIndexCount = meshopt_simplifySloppy(
            lodIndices.data(),
            indices.data(),
            indexCount,
            &vertices.front().px,
            vertices.size(),
            sizeof(VertexLayout),
            targetIndexCount,
            targetError,
            &resultError);

        if (lodIndexCount < 3)
            lodIndexCount = 3;

        lodIndices.resize(lodIndexCount);
    }

    // Compact vertices referenced by lodIndices
    std::vector<uint32> remap(vertices.size());
    const size_t lodVertexCount = meshopt_generateVertexRemap(
        remap.data(),
        lodIndices.data(),
        lodIndices.size(),
        vertices.data(),
        vertices.size(),
        sizeof(VertexLayout));

    LODData lod;
    lod.vertices.resize(lodVertexCount);
    lod.indices.resize(lodIndices.size());
    auto& lodVertices = lod.vertices;
    auto& lodIndicesRemapped = lod.indices;
    
    meshopt_remapIndexBuffer(
        lodIndicesRemapped.data(),
        lodIndices.data(),
        lodIndices.size(),
        remap.data());

    
    meshopt_remapVertexBuffer(
        lodVertices.data(),
        vertices.data(),
        vertices.size(),
        sizeof(VertexLayout),
        remap.data());

    // Optimize index order for post-transform cache
    meshopt_optimizeVertexCache(
        lodIndicesRemapped.data(),
        lodIndicesRemapped.data(),
        lodIndicesRemapped.size(),
        lodVertices.size());

    // Optimize index order for overdraw (positions required)
    constexpr float overdrawThreshold = 1.05f; // 1.01 - 3
    if (!lodVertices.empty())
    {
        meshopt_optimizeOverdraw(
            lodIndicesRemapped.data(),
            lodIndicesRemapped.data(),
            lodIndicesRemapped.size(),
            &lodVertices.front().px,
            lodVertices.size(),
            sizeof(VertexLayout),
            overdrawThreshold);
    }

    // Optimize vertex fetch: reorder vertices & remap indices
    std::vector<uint32> fetchRemap(lodVertices.size());
    meshopt_optimizeVertexFetchRemap(
        fetchRemap.data(),
        lodIndicesRemapped.data(),
        lodIndicesRemapped.size(),
        lodVertices.size());

    meshopt_remapIndexBuffer(
        lodIndicesRemapped.data(),
        lodIndicesRemapped.data(),
        lodIndicesRemapped.size(),
        fetchRemap.data());

    std::vector<VertexLayout> lodVerticesReordered(lodVertices.size());
    meshopt_remapVertexBuffer(
        lodVerticesReordered.data(),
        lodVertices.data(),
        lodVertices.size(),
        sizeof(VertexLayout),
        fetchRemap.data());

    lodVertices.swap(lodVerticesReordered);

    return lod;
}
