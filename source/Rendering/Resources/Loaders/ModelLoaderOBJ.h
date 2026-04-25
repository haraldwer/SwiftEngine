#pragma once

#include "ModelLoader.h"
#include "tiny_obj_loader.h"

struct OBJModelLoadResult
{
    ObjectPtr<tinyobj::ObjReader> reader;
    bool success = false;
};

inline OBJModelLoadResult AsyncLoadModelOBJ(const Resource::ID& InID)
{
    LOG("Loading model ", InID.Str())
    OBJModelLoadResult result;
    result.reader = new tinyobj::ObjReader();
    auto ptr = result.reader.Get();
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    result.success = ptr->ParseFromFile(InID.Str(), config);
    LOG("Model Loaded")
    return result;
}

struct OBJMeshLoadParams
{
    ObjectPtr<tinyobj::ObjReader> reader;
    Resource::ID id;
    int shapeIndex = 0;
};

inline ObjectPtr<MeshContent> AsyncLoadMeshOBJ(const OBJMeshLoadParams& InParams)
{
    LOG("Loading mesh ", InParams.shapeIndex)
    auto& shapes = InParams.reader->GetShapes();
    const auto& shapeMesh = shapes.at(InParams.shapeIndex).mesh;
    if (shapeMesh.indices.empty())
        return {};
    
    auto& attrib = InParams.reader->GetAttrib();
    ObjectPtr result = new MeshContent();
    auto& vertices = result->vertices;
    auto& indices = result->indices;
    std::unordered_map<VertexKey, uint32, VertexKeyHash> remap;
    vertices.reserve(shapeMesh.indices.size());
    indices.reserve(shapeMesh.indices.size());
    remap.reserve(shapeMesh.indices.size());
    
    size_t indexOffset = 0;
    for (size_t f = 0; f < shapeMesh.num_face_vertices.size(); ++f)
    {
        const int fv = static_cast<int>(shapeMesh.num_face_vertices[f]);
        for (int vtx = 0; vtx < fv; ++vtx)
        {
            const tinyobj::index_t idx = shapeMesh.indices[indexOffset + static_cast<size_t>(vtx)];

            VertexKey key{ 
                idx.vertex_index, 
                idx.normal_index, 
                idx.texcoord_index 
            };

            auto it = remap.find(key);
            if (it != remap.end())
            {
                indices.push_back(it->second);
                continue;
            }

            VertexLayout v{};

            if (idx.vertex_index >= 0)
            {
                const size_t vi = static_cast<size_t>(idx.vertex_index) * 3u;
                v.px = attrib.vertices[vi + 0];
                v.py = attrib.vertices[vi + 1];
                v.pz = attrib.vertices[vi + 2];
            }

            if (idx.normal_index >= 0 && !attrib.normals.empty())
            {
                const size_t ni = static_cast<size_t>(idx.normal_index) * 3u;
                v.nx = attrib.normals[ni + 0];
                v.ny = attrib.normals[ni + 1];
                v.nz = attrib.normals[ni + 2];
            }

            if (idx.texcoord_index >= 0 && !attrib.texcoords.empty())
            {
                const size_t ti = static_cast<size_t>(idx.texcoord_index) * 2u;
                v.u = attrib.texcoords[ti + 0];
                v.v = attrib.texcoords[ti + 1];
            }

            auto newIndex = static_cast<uint32>(vertices.size());
            vertices.push_back(v);
            indices.push_back(newIndex);
            remap.emplace(key, newIndex);
        }
        indexOffset += static_cast<size_t>(fv);
    }
    LOG("Mesh loaded!")
    return result;
}
