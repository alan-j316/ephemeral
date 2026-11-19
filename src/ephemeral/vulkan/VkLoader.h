#pragma once
/**
 * @file VkLoader.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Vulkan Loaders.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "RenderPrimitives.h"
#include "VkDescriptors.h"
#include "VkImages.h"
#include "VkTypes.h"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

namespace ephemeral
{
    struct GLTFMaterial
    { MaterialInstance material; };

    struct GeoSurface
    {
        uint32_t startIndex;
        uint32_t count;
        std::shared_ptr<GLTFMaterial> material;
    };

    struct MeshAsset
    {
        std::string name;
        std::vector<GeoSurface> surfaces;
        GPUMeshBuffers meshBuffers;
    };


    // Forward Declaration
    class EphemeralEngine; // NOLINT

    /// @deprecated Use loadGLTF
    std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
        EphemeralEngine& engine, const std::filesystem::path& filePath) noexcept;



    // A render graph node with a mesh
    class MeshNode: public Node
    {
    public:
        std::shared_ptr<struct MeshAsset> mesh; // NOLINT

        void draw(const flcn::Mat4F& topMatrix, DrawContext& ctx) noexcept override;
    };


    // Single GLTF Scene node
    struct LoadedGLTF: public IRenderable
    {
        explicit LoadedGLTF(EphemeralEngine& engine): creator(engine) {}

        // All the GLTF data
        std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes{};
        std::unordered_map<std::string, std::shared_ptr<Node>> nodes{};
        std::unordered_map<std::string, VulkanImage> images{};
        std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials{};

        std::vector<std::shared_ptr<Node>> topNodes{}; // Parent nodes for iterating through the file tree in order
        std::vector<VkSampler> samplers{};
        DescriptorAllocatorResizable descriptorPool{};
        AllocatedBuffer materialBuffer{};

        // TODO: Check if the full engine needs to be passed or just the context is enough
        EphemeralEngine& creator; // NOLINT

        ~LoadedGLTF() override { clearAll(); }

        void draw(const flcn::Mat4F& topMatrix, DrawContext& ctx) noexcept override;

    private:
        void clearAll() noexcept;
    };


    std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(EphemeralEngine& engine,
                                                        const std::filesystem::path& filePath);


    /// Convert from fastgltf sampling filters to VkFilter.
    /// Defaults to Linear Filtering.
    inline VkFilter toVKFilter(const fastgltf::Filter filter) noexcept
    {
        switch (filter)
        {
            // Linear Filter
            case fastgltf::Filter::Linear:
            case fastgltf::Filter::LinearMipMapLinear:
            case fastgltf::Filter::LinearMipMapNearest:
                return VK_FILTER_LINEAR;
                // Near Filter
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapLinear:
            case fastgltf::Filter::NearestMipMapNearest:
                return VK_FILTER_NEAREST;
        }
        return VK_FILTER_LINEAR;
    }

    /// Convert from fastgltf mip-map mode to Vulkan.
    /// Defaults to linear sampling.
    inline VkSamplerMipmapMode toVKMipmapMode(const fastgltf::Filter filter) noexcept
    {
        switch (filter)
        {
            case fastgltf::Filter::LinearMipMapLinear:
            case fastgltf::Filter::NearestMipMapLinear:
            case fastgltf::Filter::Linear:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            case fastgltf::Filter::LinearMipMapNearest:
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::Nearest:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            default:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

} // namespace ephemeral
