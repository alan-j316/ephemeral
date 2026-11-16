#pragma once
/**
 * @file VkPipelines.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Vulkan Pipeline abstraction.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "VkTypes.h"

namespace ephemeral
{

    class PipelineBuilder
    {
    public:
        PipelineBuilder() { clear(); }

        /// Set the fragment and vertex shader stages.
        /// @note Entry point for both the shaders are assumed to be "main".
        void setShaderStages(VkShaderModule vertexShader, VkShaderModule fragmentShader) noexcept;


        /// Set rendering primitive topology.
        void setInputTopology(VkPrimitiveTopology topology) noexcept;

        /// Set the polygon mode(Fill/Line/Point)
        void setPolygonMode(VkPolygonMode mode) noexcept;

        /// Set the cull mode and front face orientation(CW/CCW)
        void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) noexcept;

        /// Disable MSAA
        void disableMSAA() noexcept;

        /// Disable alpha blending
        void disableBlending() noexcept;

        /// Set the color attachment format for the pipeline.
        void setColorAttachmentFormat(VkFormat format) noexcept;

        /// Set the depth attachment format for the pipeline.
        void setDepthAttachmentFormat(VkFormat format) noexcept;

        /// Disable depth testing.
        void setDepthTestState(bool depthWriteEnable, VkCompareOp op) noexcept;

        /// Build the pipeline.
        [[nodiscard]] VkPipeline build(VkDevice device, const VkAllocationCallbacks* pAllocator) noexcept;

        /// Clear all pipeline configurations to default values.
        void clear() noexcept;

        /// Set the pipeline layout for the current pipeline.
        void setPipelineLayout(VkPipelineLayout layout) noexcept;

        /// Enable additive blending.
        void enableAdditiveBlending() noexcept;

        /// Enable alpha blending.
        void enableAlphaBlending() noexcept;

    private:
        std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
        VkPipelineInputAssemblyStateCreateInfo _inputAssembly{};
        VkPipelineRasterizationStateCreateInfo _rasterizer{};
        VkPipelineColorBlendAttachmentState _colorBlendAttachment{};
        VkPipelineMultisampleStateCreateInfo _multisampling{ .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
        VkPipelineLayout _pipelineLayout{};
        VkPipelineDepthStencilStateCreateInfo _depthStencil{};
        VkPipelineRenderingCreateInfo _renderingInfo{};
        VkFormat _colorAttachmentFormat{};
    };

    namespace vkutils
    {
        bool loadShaderModule(const char* filePath, VkDevice device, const VkAllocationCallbacks* pAllocator,
                              VkShaderModule* outShaderModule) noexcept;
    }
} // namespace ephemeral
