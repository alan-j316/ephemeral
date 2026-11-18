/**
 * @file VkLoader.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 16, 2026
 *
 * @brief Implementation of functions declared in VkLoader.h.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "VkLoader.h"

#include "Geometry.h"
#include "VkEngine.h"


// Specialization for fastgltf::ElementTraits for flcn
template <>
struct fastgltf::ElementTraits<flcn::Vec3F>
    : fastgltf::ElementTraitsBase<flcn::Vec3F, fastgltf::AccessorType::Vec3, float>
{};

template <>
struct fastgltf::ElementTraits<flcn::CVec2F>
    : fastgltf::ElementTraitsBase<flcn::CVec2F, fastgltf::AccessorType::Vec2, float>
{};


template <>
struct fastgltf::ElementTraits<flcn::Vec4F>
    : fastgltf::ElementTraitsBase<flcn::Vec4F, fastgltf::AccessorType::Vec4, float>
{};



namespace ephemeral
{

    std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
        EphemeralEngine& engine, const std::filesystem::path& filePath) noexcept
    {
        fmt::println("Loading GLTF: {}", filePath.string());

        // NOLINTNEXTLINE
        constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

        auto expectedData = fastgltf::GltfDataBuffer::FromPath(filePath);
        if (expectedData.has_error())
        {
            fmt::println("There was an error loading {}.\n{}", filePath.string(),
                         getErrorMessage(expectedData.error()));
            return std::nullopt;
        }
        auto data = std::move(expectedData.get());

        fastgltf::Asset gltf;
        fastgltf::Parser parser{};

        auto load = parser.loadGltfBinary(data, filePath.parent_path(), gltfOptions);

        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            fmt::println("There was an error loading the GLTF: {}", filePath.string());
            return std::nullopt;
        }

        std::vector<std::shared_ptr<MeshAsset>> meshes;
        // Use the same vector for all meshes
        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;

        // Iterate through all meshes and add it to assets.
        for (fastgltf::Mesh& mesh : gltf.meshes)
        {
            MeshAsset asset;
            asset.name = mesh.name;

            // Clear the indices and vertices vector.
            indices.clear();
            vertices.clear();


            for (auto&& primitive : mesh.primitives)
            {
                GeoSurface surface;
                surface.startIndex = static_cast<uint32_t>(indices.size());
                surface.count      = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

                size_t initialVertex = vertices.size();

                // ------------------------ INDICES ------------------------
                fastgltf::Accessor& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                vertices.reserve(indices.size() + indexAccessor.count);
                // TODO: Add support for uint8, and 16 indices depending on the type.
                fastgltf::iterateAccessor<uint32_t>(gltf, indexAccessor, [&](const uint32_t index) {
                    indices.push_back(index + initialVertex);
                });


                // ------------------------ VERTEX POSITIONS ------------------------
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);
                fastgltf::iterateAccessorWithIndex<flcn::Vec3F>(gltf, posAccessor, [&](flcn::Vec3F pos, size_t index) {
                    Vertex vertex;
                    vertex.position                 = pos;
                    vertex.color                    = flcn::Vec4F::one();
                    vertex.normal                   = { 1, 0, 0 };
                    vertex.uvX                      = 0;
                    vertex.uvY                      = 0;
                    vertices[initialVertex + index] = vertex;
                });


                // ------------------------ NORMALS ------------------------
                if (auto normalIt = primitive.findAttribute("NORMAL"); normalIt != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<flcn::Vec3F>(gltf, gltf.accessors[normalIt->accessorIndex],
                                                                    [&](flcn::Vec3F normal, size_t index) {
                                                                        vertices[initialVertex + index].normal = normal;
                                                                    });
                }


                // ------------------------ UV ------------------------
                if (auto uvIt = primitive.findAttribute("TEXCOORD_0"); uvIt != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<flcn::CVec2F>(gltf, gltf.accessors[uvIt->accessorIndex],
                                                                     [&](flcn::CVec2F uv, size_t index) {
                                                                         vertices[initialVertex + index].uvX = uv.x();
                                                                         vertices[initialVertex + index].uvY = uv.y();
                                                                     });
                }


                // ------------------------ VERTEX NORMALS ------------------------
                if (auto vertNormIt = primitive.findAttribute("COLOR_0"); vertNormIt != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<flcn::Vec4F>(gltf, gltf.accessors[vertNormIt->accessorIndex],
                                                                    [&](const flcn::Vec4F vertexNorm, size_t index) {
                                                                        vertices[initialVertex + index].color =
                                                                            vertexNorm;
                                                                    });
                }

                asset.surfaces.push_back(surface);
            }

            // Display the vertex normals
            constexpr bool overrideColors = true;
            if constexpr (overrideColors)
            {
                for (Vertex& vert : vertices)
                {
                    vert.color = flcn::Vec4(vert.normal, 1.0f);
                }
            }
            engine.uploadMesh(indices, vertices, asset.meshBuffers);
            meshes.emplace_back(std::make_shared<MeshAsset>(std::move(asset)));
        }

        fmt::println("Asset({}) was loaded successfully!", filePath.string());
        return meshes;
    }
} // namespace ephemeral
