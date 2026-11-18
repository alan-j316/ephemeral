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

    //
    struct DrawContext
    { std::vector<RenderObject> renderables; };


    // Base class for a renderable dynamic object
    class IRenderable
    {
    public:
        virtual ~IRenderable() = default;

        virtual void draw(const flcn::Mat4F& topMatrix, DrawContext& ctx) noexcept;
    };

} // namespace ephemeral
