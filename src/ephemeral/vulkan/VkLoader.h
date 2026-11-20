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


#include "VkDescriptors.h"
#include "VkEngine.h"
#include "VkImages.h"
#include "VkTypes.h"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

namespace ephemeral
{
    enum class MaterialPass : uint8_t
    {
        OPAQUE,
        TRANSPARENT,
        MAIN_COLOR,
        OTHER
    };


    /// Pipeline abstraction
    struct MaterialPipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
    };


    /// Pipeline and descriptor abstraction for a material
    struct MaterialInstance
    {
        MaterialPipeline* pipeline;
        VkDescriptorSet materialSet;
        MaterialPass passType;
    };

    // Forward Declaration
    class EphemeralEngine; // NOLINT


    struct GLTFMaterial
    { MaterialInstance material; };

    struct Bounds
    {
        flcn::Vec3F origin;
        float sphereRadius;
        flcn::Vec3F extents;
    };


    struct GeoSurface
    {
        uint32_t startIndex;
        uint32_t count;
        Bounds bounds;
        std::shared_ptr<GLTFMaterial> material;
    };

    struct MeshAsset
    {
        std::string name;
        std::vector<GeoSurface> surfaces;
        GPUMeshBuffers meshBuffers;
    };


    /// @deprecated Use loadGLTF
    std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
        EphemeralEngine& engine, const std::filesystem::path& filePath) noexcept;


    /// Flattened abstraction for a single draw call.
    struct RenderObject
    {
        uint32_t indexCount;
        uint32_t firstIndex;
        VkBuffer indexBuffer;
        Bounds bounds;
        MaterialInstance* material;

        flcn::Mat4F transform;
        VkDeviceAddress vertexBufferAddress;
    };


    struct DrawContext
    {
        std::vector<RenderObject> opaqueObjects;
        std::vector<RenderObject> transparentObjects;
    };


    // Base class for a renderable dynamic object
    class IRenderable
    {
    public:
        virtual ~IRenderable() = default;

        virtual void draw(const flcn::Mat4F& topMatrix, DrawContext& ctx) noexcept = 0;
    };



    // Scene node.
    // Can hold children and parents transformation will propagate to them.
    class Node: public IRenderable
    {
    public:
        // Parent pointer must be a weak pointer to avoid circular dependencies
        std::weak_ptr<Node> parent;
        std::vector<std::shared_ptr<Node>> children;

        flcn::Mat4F localTransform;
        flcn::Mat4F worldTransform;

        // Update the parents world transform and propagate the new world transform down to the children.
        void refreshTransform(const flcn::Mat4F& parentMatrix)
        {
            worldTransform = parentMatrix * localTransform;
            for (const auto& child : children)
            {
                child->refreshTransform(worldTransform);
            }
        }

        // Draw the node.
        void draw(const flcn::Mat4F& topMatrix, DrawContext& ctx) noexcept override
        {
            // Draw child nodes
            for (const auto& child : children)
            {
                child->draw(topMatrix, ctx);
            }
        }
    };


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


    std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(EphemeralEngine& engine, const std::filesystem::path& filePath);

    std::optional<VulkanImage> loadImage(EphemeralEngine& engine, fastgltf::Asset& asset, fastgltf::Image& image);


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
