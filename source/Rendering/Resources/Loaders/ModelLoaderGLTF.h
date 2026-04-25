#pragma once

#include "ModelLoader.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_INCLUDE_JSON

// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "rapidjson/rapidjson.h"
#include "rapidjson/prettywriter.h"
#include "tiny_gltf.h"

struct GLTFModelLoadResult
{
    ObjectPtr<tinygltf::Model> reader;
    bool success = false;
};

inline GLTFModelLoadResult AsyncLoadModelGLTF(const Resource::ID& InID)
{
    LOG("Loading model")
    GLTFModelLoadResult result;
    result.success = true;
    result.reader = new tinygltf::Model();
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    std::string filename = InID.Str();
    bool ret = loader.LoadASCIIFromFile(result.reader.Get(), &err, &warn, filename);
    if (!warn.empty())
        LOG("Warn: %s\n", warn.c_str());
    if (!err.empty())
        LOG("Err: %s\n", err.c_str());
    CHECK_RETURN_LOG(!ret, "Failed to parse glTF: " + filename, {})
    return result;
}

struct GLTFMeshLoadParams
{
    ObjectPtr<tinygltf::Model> reader;
    Resource::ID id;
    int shapeIndex = 0;
};

inline ObjectPtr<MeshData> AsyncLoadMeshGLTF(const GLTFMeshLoadParams& InParams)
{
    auto model = InParams.reader.Get();
    auto& shapes = model->meshes;
    const auto& primitives = shapes.at(InParams.shapeIndex).primitives;
    if (primitives.empty())
        return {};
    
    ObjectPtr result = new MeshData();
    auto& vertices = result->vertices;
    auto& indices = result->indices;
    
    for (auto& prim : primitives)
    {
        // ── Grab raw attribute pointers ──────────────────────────────────────
        auto GetAttrib = [&](const char* name) -> const uint8_t* {
            const auto it = prim.attributes.find(name);
            if (it == prim.attributes.end()) return nullptr;

            const auto& acc  = model->accessors[it->second];
            const auto& view = model->bufferViews[acc.bufferView];
            const auto& buf  = model->buffers[view.buffer];
            return buf.data.data() + view.byteOffset + acc.byteOffset;
        };

        const auto GetStride = [&](const char* name, size_t tightStride) -> size_t {
            const auto it = prim.attributes.find(name);
            if (it == prim.attributes.end()) return tightStride;
            const auto& view = model->bufferViews[model->accessors[it->second].bufferView];
            return view.byteStride > 0 ? view.byteStride : tightStride;
        };

        const uint8_t* positions  = GetAttrib("POSITION");
        const uint8_t* normals    = GetAttrib("NORMAL");
        const uint8_t* texcoords  = GetAttrib("TEXCOORD_0");

        const size_t posStride = GetStride("POSITION",  sizeof(float) * 3);
        const size_t nrmStride = GetStride("NORMAL",    sizeof(float) * 3);
        const size_t uvStride  = GetStride("TEXCOORD_0", sizeof(float) * 2);

        // ── Index buffer ─────────────────────────────────────────────────────
        const tinygltf::Accessor&   idxAcc  = model->accessors[prim.indices];
        const tinygltf::BufferView& idxView = model->bufferViews[idxAcc.bufferView];
        const uint8_t* rawIdx = model->buffers[idxView.buffer].data.data()
                              + idxView.byteOffset + idxAcc.byteOffset;

        const auto ReadIndex = [&](size_t i) -> uint32_t {
            switch (idxAcc.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    return reinterpret_cast<const uint8_t*> (rawIdx)[i];
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return reinterpret_cast<const uint16_t*>(rawIdx)[i];
                default:
                    return reinterpret_cast<const uint32_t*>(rawIdx)[i];
            }
        };

        // ── Build deduplicated vertex list ───────────────────────────────────
        for (size_t i = 0; i < idxAcc.count; ++i)
        {
            uint32_t srcIdx = ReadIndex(i);

            VertexLayout v{};

            if (positions) {
                const float* p = reinterpret_cast<const float*>(positions + srcIdx * posStride);
                v.px = p[0];
                v.py = p[1];
                v.pz = p[2];
            }
            if (normals) {
                const float* n = reinterpret_cast<const float*>(normals + srcIdx * nrmStride);
                v.nx = n[0];
                v.ny = n[1];
                v.nz = n[2];
            }
            if (texcoords) {
                const float* uv = reinterpret_cast<const float*>(texcoords + srcIdx * uvStride);
                v.u = uv[0];
                v.v = uv[1];
            }
            
            auto newIndex = static_cast<uint32>(vertices.size());
            vertices.push_back(v);
            indices.push_back(newIndex);
        }
    }
    return result;
}
