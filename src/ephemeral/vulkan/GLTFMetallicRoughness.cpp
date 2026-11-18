/**
 * @file GLTFMetallicRoughness.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 18, 2026
 *
 * @brief Implementation of functions declared in GLTFMetallicRoughness.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "GLTFMetallicRoughness.h"

#include "EngineContext.h"
#include "VkInitializer.h"
#include "VkPipelines.h"

namespace ephemeral
{
    void GLTFMetallicRoughness::buildPipelines(EphemeralEngineContext& ctx) noexcept
    {
        // ------------------------ SHADER MODULES ------------------------
        VkShaderModule meshVertShader, meshFragShader;

        if (!vkutils::loadShaderModule("shaders/Mesh.vert.spv", ctx.device, ctx.allocator, &meshVertShader))
        {
            fmt::println("There was an error compiling the mesh vertex shader.");
            return;
        }

        if (!vkutils::loadShaderModule("shaders/Mesh.frag.spv", ctx.device, ctx.allocator, &meshFragShader))
        {
            fmt::println("There was an error compiling the mesh fragment shader.");
            return;
        }


        // ------------------------ PUSH CONSTANTS ------------------------
        VkPushConstantRange pushConstants{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                           .offset     = 0,
                                           .size       = sizeof(GPUDrawPushConstants) };


        // ------------------------ PIPELINE LAYOUT ------------------------
        DescriptorSetLayoutBuilder descriptorBuilder;
        descriptorBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptorBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Color
        descriptorBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Roughness-metallic
        // NOLINTNEXTLINE
        materialLayout = descriptorBuilder.build(ctx.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                 ctx.allocator);
        std::array<VkDescriptorSetLayout, 2> layouts{ ctx.sceneDescriptorSetLayout, materialLayout };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutils::pipelineLayoutCreateInfo();
        pipelineLayoutInfo.setLayoutCount             = layouts.size();
        pipelineLayoutInfo.pSetLayouts                = layouts.data();
        pipelineLayoutInfo.pPushConstantRanges        = &pushConstants;
        pipelineLayoutInfo.pushConstantRangeCount     = 1;

        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(ctx.device, &pipelineLayoutInfo, ctx.allocator, &pipelineLayout));

        opaquePipeline.pipelineLayout      = pipelineLayout;
        transparentPipeline.pipelineLayout = pipelineLayout;


        // ------------------------ PIPELINE ------------------------
        // Common pipeline configuration for both opaque and transparent.
        PipelineBuilder pipelineBuilder;
        pipelineBuilder.setShaderStages(meshVertShader, meshFragShader);
        pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
        pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        pipelineBuilder.disableMSAA();
        pipelineBuilder.disableBlending();
        pipelineBuilder.setDepthTestState(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

        pipelineBuilder.setColorAttachmentFormat(ctx.drawImage.format);
        pipelineBuilder.setDepthAttachmentFormat(ctx.depthImage.format);
        pipelineBuilder.setPipelineLayout(pipelineLayout);

        // Opaque Pipeline
        opaquePipeline.pipeline = pipelineBuilder.build(ctx.device, ctx.allocator);


        // Transparent pipeline config
        pipelineBuilder.enableAdditiveBlending();
        pipelineBuilder.setDepthTestState(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
        transparentPipeline.pipeline = pipelineBuilder.build(ctx.device, ctx.allocator);


        // ------------------------ DESTROY SHADER MODULES ------------------------
        vkDestroyShaderModule(ctx.device, meshVertShader, ctx.allocator);
        vkDestroyShaderModule(ctx.device, meshFragShader, ctx.allocator);

        ctx.mainDeletionQueue.push([&] {
            vkDestroyPipeline(ctx.device, opaquePipeline.pipeline, ctx.allocator);
            vkDestroyPipeline(ctx.device, transparentPipeline.pipeline, ctx.allocator);
            // NOTE: We use only a single pipeline layout so we shouldn't try to destroy
            //       layout from both pipeline.
            vkDestroyPipelineLayout(ctx.device, opaquePipeline.pipelineLayout, ctx.allocator);
            vkDestroyDescriptorSetLayout(ctx.device, materialLayout, ctx.allocator);
        });
    }

    // TODO: Rename to destroy resources?
    void GLTFMetallicRoughness::clearResource(const VkDevice device,
                                              const VkAllocationCallbacks* pAllocator) const noexcept
    {}


    MaterialInstance GLTFMetallicRoughness::writeMaterial(const VkDevice device, const MaterialPass pass,
                                                          const VkAllocationCallbacks* pAllocator,
                                                          const MaterialResources& resources,
                                                          DescriptorAllocatorResizable& descriptorAllocator)
    {
        MaterialInstance matData;
        matData.passType = pass;

        // Select the color pipeline based on the pass type.
        if (pass == MaterialPass::TRANSPARENT)
        {
            matData.pipeline = &transparentPipeline;
        }
        else
        {
            matData.pipeline = &opaquePipeline;
        }

        matData.materialSet = descriptorAllocator.allocate(device, materialLayout, pAllocator);


        descriptorWriter.writeBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), 0,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptorWriter.writeImage(1, resources.colorImage.imageView, resources.colorSampler,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptorWriter.writeImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        descriptorWriter.updateSet(device, matData.materialSet);
        return matData;
    }

} // namespace ephemeral
