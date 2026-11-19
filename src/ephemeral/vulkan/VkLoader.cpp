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

#include "EngineContext.h"
#include "Geometry.h"
#include "VkEngine.h"
#include "VkInitializer.h"


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


    void LoadedGLTF::draw(const flcn::Mat4F& topMatrix, DrawContext& ctx) noexcept
    {
        for (const auto& node : topNodes)
        {
            node->draw(topMatrix, ctx);
        }
    }


    void LoadedGLTF::clearAll() noexcept
    {
        // TODO: Impl
    }


    std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(EphemeralEngine& engine, const std::filesystem::path& filePath)
    {

        if (!std::filesystem::exists(filePath))
        {
            fmt::println("No suitable file ({}) exists in the executable path.", filePath.string());
            return std::nullopt;
        }

        fmt::println("Loading {}...", filePath.string());
        auto& ctx = *engine._ctx;

        // ------------------------ INITIALIZATION ------------------------
        auto scene      = std::make_shared<LoadedGLTF>(engine);
        const auto file = scene.get();
        // Temporary array for storing the loaded assets
        std::vector<std::shared_ptr<MeshAsset>> meshes;
        std::vector<std::shared_ptr<Node>> nodes;
        std::vector<VulkanImage> images;
        std::vector<std::shared_ptr<GLTFMaterial>> materials;

        fastgltf::Parser parser{};
        constexpr auto gltfOptions = fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;
        // fastgltf::Options::LoadExternalImages;



        // ------------------------ GLTF LOADING ------------------------
        auto expectedData = fastgltf::GltfDataBuffer::FromPath(filePath);
        if (expectedData.has_error())
        {
            fmt::println("There was an error loading {}.\n{}", filePath.string(),
                         getErrorMessage(expectedData.error()));
            return std::nullopt;
        }
        auto& data = expectedData.get();

        auto assetExpected = parser.loadGltf(data, filePath.parent_path(), gltfOptions);
        if (assetExpected.has_error())
        {
            fmt::println("There was an error loading {}.\n{}", filePath.string(),
                         getErrorMessage(expectedData.error()));
            return std::nullopt;
        }
        auto& asset = assetExpected.get();


        // ------------------------ DESCRIPTOR POOL CONFIG ------------------------
        using PSR = DescriptorAllocatorResizable::PoolSizeRatio; // NOLINT
        std::vector descriptorPoolSizes{ PSR{
                                             .type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                             .ratio = 3,
                                         },
                                         PSR{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3 },
                                         PSR{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1 } };

        file->descriptorPool.init(ctx.device, ctx.allocator, asset.materials.size(), descriptorPoolSizes);

        // ------------------------ LOAD SAMPLERS ------------------------
        file->samplers.reserve(asset.samplers.size());
        for (auto& gltfSampler : asset.samplers)
        {
            VkSamplerCreateInfo samplerInfo{
                .sType      = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter  = toVKFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest)),
                .minFilter  = toVKFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest)),
                .mipmapMode = toVKMipmapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest)),
                .minLod     = 0,
                .maxLod     = VK_LOD_CLAMP_NONE
            };

            VkSampler vkSampler;
            VK_CHECK(vkCreateSampler(ctx.device, &samplerInfo, ctx.allocator, &vkSampler));
            file->samplers.push_back(vkSampler);
        }


        // ------------------------ LOAD TEXTURES ------------------------
        images.reserve(asset.images.size());
        for (auto& gltfImage : asset.images)
        {
            images.push_back(ctx.errorCheckerBoardImage);
            // TODO: Update later. checker board pattern for now.
        }


        // ------------------------ LOAD MATERIALS ------------------------
        using MatConstants   = GLTFMetallicRoughness::MaterialConstants; // NOLINT
        file->materialBuffer = createBuffer(sizeof(MatConstants) * asset.materials.size(), ctx.vma,
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        size_t dataIndex     = 0;
        auto* pMatConstants  = static_cast<MatConstants*>(file->materialBuffer.allocationInfo.pMappedData);

        materials.reserve(asset.materials.size());
        for (auto& gltfMaterial : asset.materials)
        {
            auto material = std::make_shared<GLTFMaterial>();
            materials.push_back(material);
            file->materials[gltfMaterial.name] = material;

            // Material constants
            MatConstants constants;
            constants.colorFactors.x() = gltfMaterial.pbrData.baseColorFactor.x();
            constants.colorFactors.y() = gltfMaterial.pbrData.baseColorFactor.y();
            constants.colorFactors.z() = gltfMaterial.pbrData.baseColorFactor.z();
            constants.colorFactors.w() = gltfMaterial.pbrData.baseColorFactor.w();

            constants.metalRoughnessFactors.x() = gltfMaterial.pbrData.metallicFactor;
            constants.metalRoughnessFactors.y() = gltfMaterial.pbrData.roughnessFactor;

            pMatConstants[dataIndex] = constants;


            // Material Type
            MaterialPass pass = MaterialPass::MAIN_COLOR;
            if (gltfMaterial.alphaMode == fastgltf::AlphaMode::Blend)
            {
                pass = MaterialPass::TRANSPARENT;
            }

            // Textures and samplers
            GLTFMetallicRoughness::MaterialResources resources;
            // TODO: Using default texture; update
            resources.colorImage        = ctx.whiteImage;
            resources.colorSampler      = ctx.defaultSamplerLinear;
            resources.metalRoughImage   = ctx.whiteImage;
            resources.metalRoughSampler = ctx.defaultSamplerLinear;

            resources.dataBuffer   = file->materialBuffer.buffer;
            resources.bufferOffset = dataIndex * sizeof(MatConstants);

            // Grab the textures and samplers from GLTF
            if (gltfMaterial.pbrData.baseColorTexture.has_value())
            {
                size_t imgIdx =
                    asset.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
                size_t samplerIdx =
                    asset.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

                resources.colorImage   = images[imgIdx];
                resources.colorSampler = file->samplers[samplerIdx];
            }

            // Build material
            material->material =
                ctx.metalRoughMaterial.writeMaterial(ctx.device, pass, ctx.allocator, resources, file->descriptorPool);
            ++dataIndex;
        }

        // ------------------------ LOAD MESHES ------------------------
        // Use the same vertex and index storage for all the meshes to prevent a lot of reallocations
        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;

        meshes.reserve(asset.meshes.size());
        for (auto& gltfMesh : asset.meshes)
        {
            auto mesh = std::make_shared<MeshAsset>();
            meshes.push_back(mesh);
            file->meshes[gltfMesh.name] = mesh;
            mesh->name                  = gltfMesh.name;

            indices.clear();
            vertices.clear();

            for (auto&& primitive : gltfMesh.primitives)
            {
                GeoSurface surface;
                surface.startIndex = indices.size();
                surface.count      = asset.accessors[primitive.indicesAccessor.value()].count;

                const size_t initialVertex = vertices.size();

                // Load indices
                {
                    fastgltf::Accessor& accessor = asset.accessors[primitive.indicesAccessor.value()];
                    indices.reserve(accessor.count);
                    // The data type needs to be updated based on the accessor's component type or
                    // the parsing will fail.
                    if (accessor.componentType == fastgltf::ComponentType::UnsignedByte)
                    {
                        fastgltf::iterateAccessor<uint8_t>(asset, accessor, [&](const uint8_t index) {
                            indices.push_back(static_cast<uint32_t>(index + initialVertex));
                        });
                    }
                    else if (accessor.componentType == fastgltf::ComponentType::UnsignedShort)
                    {
                        fastgltf::iterateAccessor<uint16_t>(asset, accessor, [&](const uint16_t index) {
                            indices.push_back(static_cast<uint32_t>(index + initialVertex));
                        });
                    }
                    else if (accessor.componentType == fastgltf::ComponentType::UnsignedInt)
                    {
                        fastgltf::iterateAccessor<uint32_t>(asset, accessor, [&](const uint32_t index) {
                            indices.push_back(static_cast<uint32_t>(index + initialVertex));
                        });
                    }
                    else
                    {
                        fmt::println("Error parsing GLTF. Unsupported component type for index buffer.");
                    }
                }


                // Load vertex positions
                {
                    fastgltf::Accessor& accessor = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                    vertices.resize(vertices.size() + accessor.count);
                    fastgltf::iterateAccessorWithIndex<flcn::Vec3F>(asset, accessor,
                                                                    [&](const flcn::Vec3F vertex, const size_t index) {
                                                                        Vertex vert;
                                                                        vert.position = vertex;
                                                                        vert.color    = flcn::Vec4F::one();
                                                                        vert.normal   = { 1, 0, 0 };
                                                                        vert.uvX      = 0;
                                                                        vert.uvY      = 0;
                                                                        vertices[initialVertex + index] = vert;
                                                                    });
                }

                // Load vertex normals
                {
                    // Only load the normal if they are present.
                    if (auto normalAttr = primitive.findAttribute("NORMAL"); normalAttr != primitive.attributes.end())
                    {
                        fastgltf::Accessor& accessor = asset.accessors[normalAttr->accessorIndex];
                        fastgltf::iterateAccessorWithIndex<flcn::Vec3F>(
                            asset, accessor, [&](const flcn::Vec3F normal, const size_t index) {
                                vertices[initialVertex + index].normal = normal;
                            });
                    }
                }

                // Load UV
                {
                    if (auto uvAttr = primitive.findAttribute("TEXTCOORD_0"); uvAttr != primitive.attributes.end())
                    {
                        fastgltf::Accessor& accessor = asset.accessors[uvAttr->accessorIndex];
                        fastgltf::iterateAccessorWithIndex<flcn::CVec2F>(
                            asset, accessor, [&](const flcn::CVec2F uv, const size_t index) {
                                vertices[initialVertex + index].uvX = uv.x();
                                vertices[initialVertex + index].uvY = uv.y();
                            });
                    }
                }

                // Load Vertex colors
                {
                    if (auto vertColorsAttr = primitive.findAttribute("COLOR_0");
                        vertColorsAttr != primitive.attributes.end())
                    {
                        fastgltf::Accessor& accessor = asset.accessors[vertColorsAttr->accessorIndex];
                        fastgltf::iterateAccessorWithIndex<flcn::Vec4F>(
                            asset, accessor, [&](const flcn::Vec4F color, const size_t index) {
                                vertices[initialVertex + index].color = color;
                            });
                    }
                }

                // Load materials
                if (primitive.materialIndex.has_value())
                {
                    surface.material = materials[primitive.materialIndex.value()];
                }
                else
                {
                    surface.material = materials[0];
                }
                mesh->surfaces.push_back(surface);
            }

            // ENGINE COUPLING
            engine.uploadMesh(indices, vertices, mesh->meshBuffers);
        }


        // ------------------------ LOAD NODES ------------------------
        nodes.reserve(asset.nodes.size());
        // First pass: Create the nodes and load the transform data.
        for (auto& gltfNode : asset.nodes)
        {
            std::shared_ptr<Node> node;

            // If a node has a mesh, then hook it to the mesh pointer
            if (gltfNode.meshIndex.has_value())
            {
                node = std::make_shared<MeshNode>();
                // Cast node to a mesh node and assign its node value
                // to the current gltf node's mesh
                static_cast<MeshNode*>(node.get())->mesh = meshes[*gltfNode.meshIndex];
            }
            else
            {
                node = std::make_shared<Node>();
            }

            nodes.push_back(node);
            file->nodes[gltfNode.name] = node;

            // Load the nodes transformation matrix.
            std::visit(fastgltf::visitor{ [&](fastgltf::TRS transform) {
                                             flcn::Mat4F translation = flcn::Mat4F::identity();
                                             translation(0, 3)       = transform.translation.x();
                                             translation(1, 3)       = transform.translation.y();
                                             translation(2, 3)       = transform.translation.z();

                                             const flcn::Mat4F scale = flcn::Mat4F::makeScale(
                                                 transform.scale.x(), transform.scale.y(), transform.scale.z());

                                             const auto rotAxis =
                                                 flcn::Vec3F{ transform.rotation.x(), transform.rotation.y(),
                                                              transform.rotation.z() };

                                             const flcn::Mat4F rotation =
                                                 flcn::Mat4F::makeRotation(transform.rotation.w(), rotAxis);

                                             node->localTransform = translation * rotation * scale;
                                         },
                                          [&](fastgltf::math::nmat4x4 matrix) {
                                              memcpy(&node->localTransform, matrix.data(), sizeof(matrix));
                                          } },
                       gltfNode.transform);
        }

        // Second Pass: Setup the transform hierarchy
        for (size_t i = 0; i < asset.nodes.size(); ++i)
        {
            fastgltf::Node& gltfNode    = asset.nodes[i];
            std::shared_ptr<Node>& node = nodes[i];

            for (auto child : gltfNode.children)
            {
                node->children.push_back(nodes[child]);
                nodes[child]->parent = node;
            }
        }

        // Add the top nodes(nodes with no parents) to the scene's list
        for (auto& node : nodes)
        {
            if (node->parent.lock() == nullptr)
            {
                file->topNodes.push_back(node);
                node->refreshTransform(flcn::Mat4F::identity());
            }
        }

        fmt::println("Loaded {}!", filePath.string());
        return scene;
    }

} // namespace ephemeral
