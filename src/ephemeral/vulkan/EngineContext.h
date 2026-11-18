#pragma once
/**
 * @file EngineContext.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 18, 2026
 *
 * @brief Decoupled Engine context.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "DeletionQueue.h"
#include "GLTFMetallicRoughness.h"
#include "Geometry.h"
#include "VkDescriptors.h"
#include "VkImages.h"
#include "VkLoader.h"
#include "VkTypes.h"

#include <ephemeral/utils/Camera.h>

namespace ephemeral
{


    // Config variables
    static constexpr auto ENGINE_NAME               = "Ephemeral";
    static constexpr bool VALIDATION_LAYERS_ENABLED = true;
    static constexpr size_t FRAME_OVERLAP           = 2; // Triple buffering

    struct FrameData
    {
        VkCommandPool cmdPool;
        VkCommandBuffer mainCmdBuffer;

        DeletionQueue deletionQueue;
        DescriptorAllocatorResizable frameDescriptors;
    };


    struct ComputePushConstant
    {
        flcn::Vec4F data1;
        flcn::Vec4F data2;
        flcn::Vec4F data3;
        flcn::Vec4F data4;
    };

    struct ComputeEffect
    {
        std::string name;

        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;

        ComputePushConstant data;
    };


    // Main Engine Context(Used to remove header leakage)
    struct EphemeralEngineContext
    {
        // ------------------------ VULKAN INSTANCE ------------------------
        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkPhysicalDevice gpu; ///< Physical Device
        VkDevice device;      /// Logical Device
        VkSurfaceKHR surface;
        VkAllocationCallbacks* allocator{ nullptr };

        // ------------------------ SWAP CHAIN ------------------------
        VkSwapchainKHR swapChain;
        VkFormat swapChainImageFormat;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
        VkExtent2D swapChainExtent;

        // ------------------------ COMMAND POOLS & BUFFERS ------------------------
        std::array<FrameData, FRAME_OVERLAP> frames;

        VkQueue graphicsQueue;
        uint32_t graphicsQueueFamily;

        // ------------------------ SYNCHRONIZATION PRIMITIVES ------------------------
        std::array<VkSemaphore, FRAME_OVERLAP> swapChainSemaphores;
        // NOTE: Number of render semaphores must match the number of swap chain images, not frames in flight
        std::vector<VkSemaphore> renderSemaphores;
        std::array<VkFence, FRAME_OVERLAP> renderFences;

        // ------------------------ VMA ------------------------
        VmaAllocator vma;

        // ------------------------ DRAW RESOURCES ------------------------
        VulkanImage drawImage;
        VulkanImage depthImage;
        VkExtent2D drawExtent;

        // ------------------------ DESCRIPTORS ------------------------
        DescriptorAllocatorResizable gDescriptorAllocator;
        VkDescriptorSet drawImageDescriptorSet;
        VkDescriptorSetLayout drawImageDescriptorSetLayout;
        VkDescriptorSetLayout sceneDescriptorSetLayout;
        VkDescriptorSetLayout singleImageDescriptorLayout;

        // ------------------------ PIPELINES ------------------------
        VkPipelineLayout gradientPipelineLayout;
        VkPipeline gradientPipeline;
        VkPipelineLayout meshPipelineLayout;
        VkPipeline meshPipeline;

        // ------------------------ IMMEDIATE SUBMITS ------------------------
        VkFence immFence;
        VkCommandBuffer immCmdBuffer;
        VkCommandPool immCmdPool;

        // ------------------------ IMGUI ------------------------
        VkDescriptorPool imguiPool;

        // ------------------------ ENGINE GLOBAL ------------------------
        DeletionQueue mainDeletionQueue;
        bool resizeRequested;
        float renderScale;

        // ------------------------ DATA ------------------------
        std::vector<ComputeEffect> bgEffects{};
        int currentBgEffect{ 0 };
        std::vector<std::shared_ptr<MeshAsset>> testMeshes;
        GPUSceneData sceneData;
        VulkanImage whiteImage, blackImage, grayImage, errorCheckerBoardImage;
        VkSampler defaultSamplerLinear, defaultSamplerNearest;

        MaterialInstance defaultData;
        GLTFMetallicRoughness metalRoughMaterial;
        DrawContext mainDrawContext;
        std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

        Camera mainCamera;
    };
} // namespace ephemeral
