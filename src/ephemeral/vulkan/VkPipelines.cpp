/**
 * @file VkPipelines.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 14, 2026
 *
 * @brief Implementation of functions declared in VkPipelines.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "VkPipelines.h"

#include "VkInitializer.h"

#include <fstream>


namespace ephemeral
{
    bool vkutils::loadShaderModule(const char* filePath, const VkDevice device, const VkAllocationCallbacks* pAllocator,
                                   VkShaderModule* outShaderModule) noexcept
    {
        // ------------------------ FILE READ ------------------------
        // Open the file with the cursor at the end to read the size.
        std::ifstream file(filePath, std::ios::ate | std::ios::binary); // NOLINT

        if (!file.is_open())
        {
            return false;
        }

        const size_t fileSize = static_cast<size_t>(file.tellg());

        // SPIRV expect a uin32_t* so reserve a vector large enough for the entire file.
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        file.seekg(0); // Move the cursor to the start.
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
        file.close();

        // ------------------------ SHADER MODULE CREATION ------------------------
        const VkShaderModuleCreateInfo shaderCreateInfo{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode    = buffer.data(),
        };

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &shaderCreateInfo, pAllocator, &shaderModule) != VK_SUCCESS)
        {
            return false;
        }
        *outShaderModule = shaderModule;
        return true;
    }


    void PipelineBuilder::setShaderStages(const VkShaderModule vertexShader,
                                          const VkShaderModule fragmentShader) noexcept
    {
        _shaderStages.clear();
        _shaderStages.push_back(vkutils::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
        _shaderStages.push_back(vkutils::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
    }


    void PipelineBuilder::setTopology(const VkPrimitiveTopology topology) noexcept
    {
        _inputAssembly.topology               = topology;
        _inputAssembly.primitiveRestartEnable = VK_FALSE;
    }


    void PipelineBuilder::setPolygonMode(VkPolygonMode mode) noexcept
    {
        _rasterizer.polygonMode = mode;
        _rasterizer.lineWidth   = 1.0f;
    }


    void PipelineBuilder::setCullMode(const VkCullModeFlags cullMode, const VkFrontFace frontFace) noexcept
    {
        _rasterizer.cullMode  = cullMode;
        _rasterizer.frontFace = frontFace;
    }


    void PipelineBuilder::disableMSAA() noexcept
    {
        _multisampling.sampleShadingEnable   = VK_FALSE;
        _multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        _multisampling.minSampleShading      = 1.0f;
        _multisampling.pSampleMask           = nullptr;
        _multisampling.alphaToCoverageEnable = VK_FALSE;
        _multisampling.alphaToOneEnable      = VK_FALSE;
    }


    void PipelineBuilder::disableBlending() noexcept
    {
        // NOLINTBEGIN
        _colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // NOLINTEND
        _colorBlendAttachment.blendEnable = VK_FALSE;
    }


    void PipelineBuilder::setColorAttachmentFormat(const VkFormat format) noexcept
    {
        _colorAttachmentFormat                 = format;
        _renderingInfo.colorAttachmentCount    = 1;
        _renderingInfo.pColorAttachmentFormats = &_colorAttachmentFormat;
    }


    void PipelineBuilder::setDepthAttachmentFormat(const VkFormat format) noexcept
    { _renderingInfo.depthAttachmentFormat = format; }


    void PipelineBuilder::disableDepthTest() noexcept
    {
        _depthStencil.depthTestEnable       = VK_FALSE;
        _depthStencil.depthWriteEnable      = VK_FALSE;
        _depthStencil.depthCompareOp        = VK_COMPARE_OP_NEVER;
        _depthStencil.depthBoundsTestEnable = VK_FALSE;
        _depthStencil.stencilTestEnable     = VK_FALSE;
        _depthStencil.front                 = {};
        _depthStencil.back                  = {};
        _depthStencil.minDepthBounds        = 0.0f;
        _depthStencil.maxDepthBounds        = 1.0f;
    }


    VkPipeline PipelineBuilder::build(const VkDevice device, const VkAllocationCallbacks* pAllocator) noexcept
    {
        // ------------------------ VIEWPORT CONFIGURATION ------------------------
        // Set to default values as we are using dynamic rendering
        VkPipelineViewportStateCreateInfo viewPortState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                         .pNext = nullptr,
                                                         .viewportCount = 1,
                                                         .scissorCount  = 1 };


        // ------------------------ COLOR BLEND STATE ------------------------
        // TODO: Disable for now
        VkPipelineColorBlendStateCreateInfo colorBlending{ .sType =
                                                               VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                           .pNext           = nullptr,
                                                           .logicOpEnable   = VK_FALSE,
                                                           .attachmentCount = 1,
                                                           .pAttachments    = &_colorBlendAttachment };


        // ------------------------ VERTEX INPUT ------------------------
        VkPipelineVertexInputStateCreateInfo vertexStateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };

        // ------------------------ DYNAMIC STATES ------------------------
        std::array dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{ .sType =
                                                               VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                           .pNext = nullptr,
                                                           .flags = 0,
                                                           .dynamicStateCount =
                                                               static_cast<uint32_t>(dynamicStates.size()),
                                                           .pDynamicStates = dynamicStates.data() };


        // ------------------------ PIPELINE ------------------------
        VkGraphicsPipelineCreateInfo graphicsPipelineInfo{ .sType      = VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR,
                                                           .pNext      = &_renderingInfo,
                                                           .stageCount = static_cast<uint32_t>(_shaderStages.size()),
                                                           .pStages    = _shaderStages.data(),
                                                           .pVertexInputState   = &vertexStateInfo,
                                                           .pInputAssemblyState = &_inputAssembly,
                                                           .pTessellationState  = nullptr,
                                                           .pViewportState      = &viewPortState,
                                                           .pRasterizationState = &_rasterizer,
                                                           .pMultisampleState   = &_multisampling,
                                                           .pDepthStencilState  = &_depthStencil,
                                                           .pColorBlendState    = &colorBlending,
                                                           .pDynamicState       = &dynamicStateInfo,
                                                           .layout              = _pipelineLayout };

        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, pAllocator,
                                      &graphicsPipeline) != VK_SUCCESS)
        {
            fmt::println("Failed to create graphics pipeline");
            return VK_NULL_HANDLE;
        }
        else
        {
            return graphicsPipeline;
        }
    }


    void PipelineBuilder::clear() noexcept
    {
        _inputAssembly        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        _rasterizer           = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        _colorBlendAttachment = {};
        _multisampling        = { .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
        _pipelineLayout       = {};
        _depthStencil         = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        _renderingInfo        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        _shaderStages.clear();
    }


} // namespace ephemeral
