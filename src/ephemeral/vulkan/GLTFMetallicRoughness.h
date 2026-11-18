#pragma once
/**
 * @file GLTFMetallicRoughness.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 18, 2026
 *
 * @brief Main Render Pipelines for the engine.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "RenderPrimitives.h"
#include "VkDescriptors.h"
#include "VkImages.h"

namespace ephemeral
{

    struct GLTFMetallicRoughness
    {
        MaterialPipeline transparentPipeline, opaquePipeline;
        VkDescriptorSetLayout materialLayout;

        struct MaterialConstants
        {
            flcn::Vec4F colorFactors;
            flcn::Vec4F metalRoughnessFactors;
            /// Padding
            std::array<flcn::Vec4F, 14> _reserved; // NOLINT
        };

        struct MaterialResources
        {
            VulkanImage colorImage;
            VkSampler colorSampler;
            VulkanImage metalRoughImage;
            VkSampler metalRoughSampler;
            VkBuffer dataBuffer;
            uint32_t bufferOffset;
        };

        DescriptorWriter descriptorWriter;

        void buildPipelines(struct EphemeralEngineContext& ctx) noexcept; // NOLINT
        void clearResource(VkDevice device, const VkAllocationCallbacks* pAllocator) const noexcept;

        MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const VkAllocationCallbacks* pAllocator,
                                       const MaterialResources& resources,
                                       DescriptorAllocatorResizable& descriptorAllocator);
    };
}; // namespace ephemeral
