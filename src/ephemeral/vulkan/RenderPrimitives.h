#pragma once
/**
 * @file RenderPrimitives.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 18, 2026
 *
 * @brief Rendering object abstraction.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "VkTypes.h"

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


    /// Flattened abstraction for a single draw call.
    struct RenderObject
    {
        uint32_t indexCount;
        uint32_t firstIndex;
        VkBuffer indexBuffer;

        MaterialInstance* material;

        flcn::Mat4F transform;
        VkDeviceAddress vertexBufferAddress;
    };


    struct DrawContext
    { std::vector<RenderObject> renderables; };


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

} // namespace ephemeral
