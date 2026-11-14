/**
 * @file VkEngine.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Implementation of member functions declared in VkEngine.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "VkEngine.h"

#include "VkDescriptors.h"
#include "VkImages.h"
#include "VkInitializer.h"
#include "VkPipelines.h"
#include "VkTypes.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <array>
#include <cassert>
#include <chrono>
#include <ephemeral/utils/DeletionQueue.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <thread>


namespace ephemeral
{
    static constexpr auto TIMEOUT = std::numeric_limits<uint32_t>::max();

    struct FrameData
    {
        VkCommandPool cmdPool;
        VkCommandBuffer mainCmdBuffer;

        DeletionQueue deletionQueue;
    };


    struct ComputePushConstant
    {
        flcn::Vec4F data1;
        flcn::Vec4F data2;
        flcn::Vec4F data3;
        flcn::Vec4F data4;
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
        std::array<FrameData, EphemeralEngine::FRAME_OVERLAP> frames;

        VkQueue graphicsQueue;
        uint32_t graphicsQueueFamily;

        // ------------------------ SYNCHRONIZATION PRIMITIVES ------------------------
        std::array<VkSemaphore, EphemeralEngine::FRAME_OVERLAP> swapChainSemaphores;
        // NOTE: Number of render semaphores must match the number of swap chain images, not frames in flight
        std::vector<VkSemaphore> renderSemaphores;
        std::array<VkFence, EphemeralEngine::FRAME_OVERLAP> renderFences;

        // ------------------------ VMA ------------------------
        VmaAllocator vma;

        // ------------------------ DRAW RESOURCES ------------------------
        VulkanImage drawImage;
        VkExtent2D drawExtent;

        // ------------------------ DESCRIPTORS ------------------------
        DescriptorAllocator gDescriptorAllocator;
        VkDescriptorSet drawImageDescriptorSet;
        VkDescriptorSetLayout drawImageDescriptorSetLayout;

        // ------------------------ PIPELINES ------------------------
        VkPipelineLayout gradientPipelineLayout;
        VkPipeline gradientPipeline;

        // ------------------------ IMMEDIATE SUBMITS ------------------------
        VkFence immFence;
        VkCommandBuffer immCmdBuffer;
        VkCommandPool immCmdPool;

        // ------------------------ IMGUI ------------------------
        VkDescriptorPool imguiPool;

        // ------------------------ ENGINE GLOBAL ------------------------
        DeletionQueue mainDeletionQueue;
    };

    /// +---------------------------------------------------------------------------------------+
    /// |                              STATIC FUNCTIONS                                         |
    /// +---------------------------------------------------------------------------------------+


    inline static void drawBackground(const VkCommandBuffer cmdBuffer, const uint32_t frameNumber,
                                      const EphemeralEngineContext& ctx) noexcept
    {
        // Bind the pipeline and descriptor sets
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.gradientPipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.gradientPipelineLayout, 0, 1,
                                &ctx.drawImageDescriptorSet, 0, nullptr);
        // Add push constants
        ComputePushConstant pc;
        pc.data1 = flcn::Vec4F{ 1, 0, 0, 1 };
        pc.data2 = flcn::Vec4F{ 0, 0, 1, 1 };
        vkCmdPushConstants(cmdBuffer, ctx.gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

        // Execute the compute shader
        vkCmdDispatch(cmdBuffer, std::ceil(ctx.drawExtent.width / 16.0), std::ceil(ctx.drawExtent.height / 16.0), 1);
    }


    inline static void immediateSubmit(const EphemeralEngineContext& ctx,
                                       std::function<void(VkCommandBuffer cmdBuffer)>&& func) noexcept
    {
        // ------------------------ RESET FENCE AND COMMAND BUFFER ------------------------
        VK_CHECK(vkResetFences(ctx.device, 1, &ctx.immFence));
        VK_CHECK(vkResetCommandBuffer(ctx.immCmdBuffer, 0));

        const VkCommandBuffer cmdBuffer = ctx.immCmdBuffer;


        // ------------------------ RECORD ------------------------
        const VkCommandBufferBeginInfo cmdBeginInfo =
            vkutils::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo));
        func(cmdBuffer);
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));


        // ------------------------ SUBMIT ------------------------
        const VkCommandBufferSubmitInfo cmdBufferSubmitInfo = vkutils::commandBufferSubmitInfo(cmdBuffer);
        const VkSubmitInfo2 submitInfo = vkutils::submitInfo(&cmdBufferSubmitInfo, nullptr, nullptr);
        VK_CHECK(vkQueueSubmit2(ctx.graphicsQueue, 1, &submitInfo, ctx.immFence));
        VK_CHECK(vkWaitForFences(ctx.device, 1, &ctx.immFence, true, TIMEOUT));
    }


    inline static void drawImGui(const VkCommandBuffer cmdBuffer, const VkImageView targetImageView,
                                 const EphemeralEngineContext& ctx) noexcept
    {
        const VkRenderingAttachmentInfo colorAttachment =
            vkutils::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const VkRenderingInfo renderInfo = vkutils::renderingInfo(ctx.swapChainExtent, &colorAttachment, nullptr);

        vkCmdBeginRendering(cmdBuffer, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
        vkCmdEndRendering(cmdBuffer);
    }



    /// +---------------------------------------------------------------------------------------+
    /// |                             MEMBER FUNCTIONS                                          |
    /// +---------------------------------------------------------------------------------------+

    EphemeralEngine* g_loadedEngine = nullptr;
    EphemeralEngine& EphemeralEngine::get() { return *g_loadedEngine; }


    void EphemeralEngine::init(std::string appName)
    {
        // only one engine initialization is allowed with the application.
        assert(g_loadedEngine == nullptr);
        g_loadedEngine = this;

        _appName = std::move(appName);

        // Allocate memory for context and initialize
        // Here we are using new but a custom allocator is preferred
        _ctx = new EphemeralEngineContext();

        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);
        _window = SDL_CreateWindow(ENGINE_NAME, static_cast<int>(_windowExtent.width),
                                   static_cast<int>(_windowExtent.height), SDL_WINDOW_VULKAN);

        // ------------------------ INITIALIZE VULKAN ------------------------
        initVulkan();
        initVMA();
        initSwapChain();
        initCommands();
        initSyncStructures();
        initDescriptors();
        initPipelines();


        initImgui();


        // everything went fine
        _isInitialized = true;
    }


    void EphemeralEngine::cleanup()
    {
        if (_isInitialized)
        {
            // ------------------------ VULKAN ------------------------
            vkDeviceWaitIdle(_ctx->device);
            for (const auto& frame : _ctx->frames)
            {
                // Destroy Command Pool
                vkDestroyCommandPool(_ctx->device, frame.cmdPool, _ctx->allocator);
            }

            // Destroy Sync Objects
            for (size_t i = 0; i < FRAME_OVERLAP; ++i)
            {
                vkDestroyFence(_ctx->device, _ctx->renderFences[i], _ctx->allocator);

                vkDestroySemaphore(_ctx->device, _ctx->swapChainSemaphores[i], _ctx->allocator);
            }
            for (const auto renderSemaphore : _ctx->renderSemaphores)
            {
                vkDestroySemaphore(_ctx->device, renderSemaphore, _ctx->allocator);
            }

            // Flush the deletion queue
            _ctx->mainDeletionQueue.flush();

            destroySwapChain();
            vkDestroySurfaceKHR(_ctx->instance, _ctx->surface, _ctx->allocator);
            vkDestroyDevice(_ctx->device, _ctx->allocator);
            vkb::destroy_debug_utils_messenger(_ctx->instance, _ctx->debugMessenger);
            vkDestroyInstance(_ctx->instance, _ctx->allocator);


            // ------------------------ SDL ------------------------
            SDL_DestroyWindow(_window);
            SDL_Quit();

            // ------------------------ ENGINE INTERNALS ------------------------
            // clear engine pointer
            g_loadedEngine = nullptr;
            // Free the context
            delete _ctx;
        }
    }


    void EphemeralEngine::draw()
    {
        // ------------------------ WAITING & SWAP CHAIN IMAGE ACQUISITION ------------------------
        const auto curFence           = _ctx->renderFences[_frameNumber % FRAME_OVERLAP];
        const auto swapChainSemaphore = _ctx->swapChainSemaphores[_frameNumber % FRAME_OVERLAP];

        // Wait for previous frame to finish
        VK_CHECK(vkWaitForFences(_ctx->device, 1, &curFence, true, TIMEOUT));
        // Flush the deletion queue for the current frame
        currentFrame().deletionQueue.flush();
        VK_CHECK(vkResetFences(_ctx->device, 1, &curFence));

        // Request an image from the swapchain
        uint32_t swapChainImageIdx;
        VK_CHECK(vkAcquireNextImageKHR(_ctx->device, _ctx->swapChain, TIMEOUT, swapChainSemaphore, nullptr,
                                       &swapChainImageIdx));
        // NOTE: Render semaphore must be sync with the image not frame index.
        const auto renderSemaphore = _ctx->renderSemaphores[swapChainImageIdx];


        _ctx->drawExtent.width  = _ctx->drawImage.extent.width;
        _ctx->drawExtent.height = _ctx->drawImage.extent.height;

        // ------------------------ COMMAND BUFFER RECORDING ------------------------
        // Reset the command buffer and begin recording the commands
        const VkCommandBuffer cmdBuffer = currentFrame().mainCmdBuffer;
        VK_CHECK(vkResetCommandBuffer(cmdBuffer, 0));

        // We will reset the command buffer every time after submission
        const VkCommandBufferBeginInfo cmdBeginInfo =
            vkutils::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo));


        // Transition the draw image to a writeable layout
        vkutils::transitionImage(cmdBuffer, _ctx->drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        // Draw to the 'draw' image.
        drawBackground(cmdBuffer, _frameNumber, *_ctx);

        vkutils::transitionImage(cmdBuffer, _ctx->drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        // Transition the swap chain image
        vkutils::transitionImage(cmdBuffer, _ctx->swapChainImages[swapChainImageIdx], VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Blit the draw image to the swapchain image
        vkutils::copyImageToImage(cmdBuffer, _ctx->drawImage.image, _ctx->swapChainImages[swapChainImageIdx],
                                  _ctx->drawExtent, _ctx->swapChainExtent);

        // Transition layout optimal for rendering
        vkutils::transitionImage(cmdBuffer, _ctx->swapChainImages[swapChainImageIdx],
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        drawImGui(cmdBuffer, _ctx->swapChainImageViews[swapChainImageIdx], *_ctx);

        // Transition layout optimal for presentation
        vkutils::transitionImage(cmdBuffer, _ctx->swapChainImages[swapChainImageIdx],
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        // End recording
        vkEndCommandBuffer(cmdBuffer);


        // ------------------------ QUEUE SUBMISSION ------------------------
        const VkCommandBufferSubmitInfo cmdBufferSubmitInfo = vkutils::commandBufferSubmitInfo(cmdBuffer);

        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
            vkutils::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, swapChainSemaphore);
        const VkSemaphoreSubmitInfo renderSemaphoreInfo =
            vkutils::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, renderSemaphore);

        const VkSubmitInfo2 submitInfo =
            vkutils::submitInfo(&cmdBufferSubmitInfo, &renderSemaphoreInfo, &waitSemaphoreInfo);

        VK_CHECK(vkQueueSubmit2(_ctx->graphicsQueue, 1, &submitInfo, curFence));


        // ------------------------ PRESENTATION ------------------------
        const VkPresentInfoKHR presentInfo{ .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                            .pNext              = nullptr,
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores    = &renderSemaphore,
                                            .swapchainCount     = 1,
                                            .pSwapchains        = &_ctx->swapChain,
                                            .pImageIndices      = &swapChainImageIdx };
        VK_CHECK(vkQueuePresentKHR(_ctx->graphicsQueue, &presentInfo));

        // Increment the frame number
        ++_frameNumber;
    }


    void EphemeralEngine::run()
    {
        SDL_Event e;
        bool bQuit = false;

        // main loop
        while (!bQuit)
        {
            // Handle events on queue
            while (SDL_PollEvent(&e) != 0)
            {
                ImGui_ImplSDL3_ProcessEvent(&e);
                // close the window when user alt-f4s or clicks the X button
                if (e.type == SDL_EVENT_QUIT)
                {
                    bQuit = true;
                }

                // Stop rendering when the window is in the background.
                if (e.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    _stopRendering = true;
                }
                if (e.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    _stopRendering = false;
                }
            }

            // do not draw if we are minimized
            if (_stopRendering)
            {
                // throttle the speed to avoid the endless spinning
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // ------------------------ IMGUI ------------------------
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();
            ImGui::Render();

            draw();
        }
    }


    /// +---------------------------------------------------------------------------------------+
    /// |                             PRIVATE METHODS                                           |
    /// +---------------------------------------------------------------------------------------+

    void EphemeralEngine::initVulkan() noexcept
    {
        // ------------------------ INSTANCE CREATION ------------------------
        vkb::InstanceBuilder builder;

        auto instanceResult = builder.set_app_name(_appName)
                                  .set_engine_name(ENGINE_NAME)
                                  .request_validation_layers(VALIDATION_LAYERS_ENABLED)
                                  .require_api_version(1, 3, 0)
                                  .set_allocation_callbacks(_ctx->allocator)
                                  .build();
        vkb::Instance vkbInstance = instanceResult.value();

        _ctx->instance       = vkbInstance.instance;
        _ctx->debugMessenger = vkbInstance.debug_messenger;


        // ------------------------ SURFACE CREATION ------------------------
        SDL_Vulkan_CreateSurface(_window, vkbInstance.instance, _ctx->allocator, &_ctx->surface);


        // ------------------------ VULKAN FEATURES ------------------------
        VkPhysicalDeviceVulkan13Features vk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                                                       .synchronization2 = true,
                                                       .dynamicRendering = true };

        VkPhysicalDeviceVulkan12Features vk12Features{
            .sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing  = true,
            .bufferDeviceAddress = true,
        };


        // ------------------------ DEVICE SELECTION ------------------------
        vkb::PhysicalDeviceSelector devSelector{ vkbInstance };
        vkb::PhysicalDevice physicalDevice = devSelector
                                                 .set_minimum_version(1, 3) // Vulkan 1_3
                                                 .set_required_features_13(vk13Features)
                                                 .set_required_features_12(vk12Features)
                                                 .set_surface(_ctx->surface)
                                                 .select()
                                                 .value();

        vkb::DeviceBuilder deviceBuilder{ physicalDevice };
        vkb::Device vkbDevice = deviceBuilder.build().value();

        _ctx->gpu    = physicalDevice.physical_device;
        _ctx->device = vkbDevice.device;


        // ------------------------ GRAPHICS QUEUE ------------------------
        _ctx->graphicsQueue       = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        _ctx->graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    }


    void EphemeralEngine::initVMA() noexcept
    {
        const VmaAllocatorCreateInfo vmaInfo{ .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                              .physicalDevice = _ctx->gpu,
                                              .device         = _ctx->device,
                                              .instance       = _ctx->instance };
        vmaCreateAllocator(&vmaInfo, &_ctx->vma);

        _ctx->mainDeletionQueue.push([&] {
            vmaDestroyAllocator(_ctx->vma);
        });
    }


    void EphemeralEngine::initSwapChain() noexcept
    {
        createSwapChain(_windowExtent.width, _windowExtent.height);

        // ------------------------ DRAW IMAGE ------------------------
        auto& drawImage = _ctx->drawImage;

        // Hardcode the format to draw 16-bit floats
        drawImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        drawImage.extent = { .width = _windowExtent.width, .height = _windowExtent.height, .depth = 1 };

        // NOLINTNEXTLINE
        VkImageUsageFlags drawImgUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const VkImageCreateInfo drawImgInfo =
            vkutils::imageCreateInfo(drawImage.format, drawImgUsage, drawImage.extent);

        // Allocate memory
        constexpr VmaAllocationCreateInfo drawImgAllocInfo{ .usage         = VMA_MEMORY_USAGE_GPU_ONLY,
                                                            .requiredFlags = static_cast<VkMemoryPropertyFlags>(
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };

        // Allocate the image
        vmaCreateImage(_ctx->vma, &drawImgInfo, &drawImgAllocInfo, &drawImage.image, &drawImage.allocation, nullptr);

        // ImageView
        const VkImageViewCreateInfo drawImgViewInfo =
            vkutils::imageviewCreateInfo(drawImage.format, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(_ctx->device, &drawImgViewInfo, _ctx->allocator, &drawImage.imageView));

        // Add to the deletion queue
        _ctx->mainDeletionQueue.push([&] {
            vkDestroyImageView(_ctx->device, drawImage.imageView, _ctx->allocator);
            vmaDestroyImage(_ctx->vma, drawImage.image, drawImage.allocation);
        });
    }


    void EphemeralEngine::initCommands() noexcept
    {
        // ------------------------ DRAW COMMANDS ------------------------
        const VkCommandPoolCreateInfo commandPoolInfo =
            vkutils::commandPoolCreateInfo(_ctx->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        for (size_t i = 0; i < FRAME_OVERLAP; ++i)
        {
            VK_CHECK(vkCreateCommandPool(_ctx->device, &commandPoolInfo, _ctx->allocator, &_ctx->frames[i].cmdPool));
            VkCommandBufferAllocateInfo cmdBufferInfo = vkutils::commandBufferAllocateInfo(_ctx->frames[i].cmdPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(_ctx->device, &cmdBufferInfo, &_ctx->frames[i].mainCmdBuffer));
        }


        // ------------------------ IMMEDIATE MODE COMMANDS ------------------------
        VK_CHECK(vkCreateCommandPool(_ctx->device, &commandPoolInfo, _ctx->allocator, &_ctx->immCmdPool));

        const VkCommandBufferAllocateInfo immCmdAllocInfo = vkutils::commandBufferAllocateInfo(_ctx->immCmdPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_ctx->device, &immCmdAllocInfo, &_ctx->immCmdBuffer));

        _ctx->mainDeletionQueue.push([&] {
            vkDestroyCommandPool(_ctx->device, _ctx->immCmdPool, _ctx->allocator);
        });
    }


    void EphemeralEngine::initSyncStructures() noexcept
    {
        // We need to create the fence in signaled state.
        const VkFenceCreateInfo fenceCreateInfo         = vkutils::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
        const VkSemaphoreCreateInfo semaphoreCreateInfo = vkutils::semaphoreCreateInfo();

        for (size_t i = 0; i < FRAME_OVERLAP; ++i)
        {
            VK_CHECK(vkCreateFence(_ctx->device, &fenceCreateInfo, _ctx->allocator, &_ctx->renderFences[i]));
            VK_CHECK(
                vkCreateSemaphore(_ctx->device, &semaphoreCreateInfo, _ctx->allocator, &_ctx->swapChainSemaphores[i]));
        }
        _ctx->renderSemaphores.resize(_ctx->swapChainImages.size());
        for (size_t i = 0; i < _ctx->swapChainImages.size(); ++i)
        {
            VK_CHECK(
                vkCreateSemaphore(_ctx->device, &semaphoreCreateInfo, _ctx->allocator, &_ctx->renderSemaphores[i]));
        }

        // ------------------------ IMMEDIATE MODE ------------------------
        VK_CHECK(vkCreateFence(_ctx->device, &fenceCreateInfo, _ctx->allocator, &_ctx->immFence));
        _ctx->mainDeletionQueue.push([&] {
            vkDestroyFence(_ctx->device, _ctx->immFence, _ctx->allocator);
        });
    }


    void EphemeralEngine::initDescriptors() noexcept
    {
        // ------------------------ CREATE POOLS & DESCRIPTORS ------------------------
        // Create a descriptor pool that will hold 10 sets with 1 image each.
        // STORAGE_IMAGE - Compute Shader Write Target
        std::array sizes{ DescriptorAllocator::PoolSizeRatio{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1 } };
        _ctx->gDescriptorAllocator.init(_ctx->device, _ctx->allocator, 10, sizes);

        // Make descriptor set layout for our compute shader
        DescriptorSetLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _ctx->drawImageDescriptorSetLayout = builder.build(_ctx->device, VK_SHADER_STAGE_COMPUTE_BIT, _ctx->allocator);
        // Allocate the descriptor the image
        _ctx->drawImageDescriptorSet =
            _ctx->gDescriptorAllocator.allocate(_ctx->device, _ctx->drawImageDescriptorSetLayout);


        // ------------------------ ATTACH OUR DRAW IMAGE TO THE DESCRIPTOR ------------------------
        VkDescriptorImageInfo imageInfo{ .imageView   = _ctx->drawImage.imageView,
                                         .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

        const VkWriteDescriptorSet drawImageWrite{ .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                   .pNext           = nullptr,
                                                   .dstSet          = _ctx->drawImageDescriptorSet,
                                                   .dstBinding      = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                   .pImageInfo      = &imageInfo };


        vkUpdateDescriptorSets(_ctx->device, 1, &drawImageWrite, 0, nullptr);
        // Add the descriptor to the deletion queue
        _ctx->mainDeletionQueue.push([&] {
            _ctx->gDescriptorAllocator.destroyPool(_ctx->device, _ctx->allocator);
            vkDestroyDescriptorSetLayout(_ctx->device, _ctx->drawImageDescriptorSetLayout, _ctx->allocator);
        });
    }


    void EphemeralEngine::initPipelines() noexcept { initBackgroundPipelines(); }


    void EphemeralEngine::initBackgroundPipelines() noexcept
    {
        // ------------------------ PIPELINE LAYOUT ------------------------
        VkPushConstantRange pushConstantRange{ .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                               .offset     = 0,
                                               .size       = sizeof(ComputePushConstant) };
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                             .pNext = nullptr,
                                                             .flags = 0,
                                                             .setLayoutCount = 1,
                                                             .pSetLayouts    = &_ctx->drawImageDescriptorSetLayout,
                                                             .pushConstantRangeCount = 1,
                                                             .pPushConstantRanges    = &pushConstantRange };

        VK_CHECK(
            vkCreatePipelineLayout(_ctx->device, &pipelineLayoutInfo, _ctx->allocator, &_ctx->gradientPipelineLayout));


        // ------------------------ SHADER STAGE ------------------------
        VkShaderModule drawComputeShaderModule;
        if (!vkutils::loadShaderModule("shaders/gradient_color.comp.spv", _ctx->device, _ctx->allocator,
                                       &drawComputeShaderModule))
        {
            fmt::print("There was an error loading the shader module.");
        }

        const VkPipelineShaderStageCreateInfo shaderStageInfo{ .sType =
                                                                   VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                               .pNext  = nullptr,
                                                               .flags  = 0,
                                                               .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                                                               .module = drawComputeShaderModule,
                                                               .pName  = "main" };


        // ------------------------ PIPELINE ------------------------
        const VkComputePipelineCreateInfo computePipelineInfo{ .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                               .pNext  = nullptr,
                                                               .flags  = 0,
                                                               .stage  = shaderStageInfo,
                                                               .layout = _ctx->gradientPipelineLayout };

        VK_CHECK(vkCreateComputePipelines(_ctx->device, VK_NULL_HANDLE, 1, &computePipelineInfo, _ctx->allocator,
                                          &_ctx->gradientPipeline));

        // We can create the shader module after its bound to the pipeline
        vkDestroyShaderModule(_ctx->device, drawComputeShaderModule, _ctx->allocator);

        // Add the pipeline layout and pipeline to the deletion queue
        _ctx->mainDeletionQueue.push([&] {
            vkDestroyPipelineLayout(_ctx->device, _ctx->gradientPipelineLayout, _ctx->allocator);
            vkDestroyPipeline(_ctx->device, _ctx->gradientPipeline, _ctx->allocator);
        });
    }


    void EphemeralEngine::createSwapChain(const uint32_t width, const uint32_t height) noexcept
    {
        vkb::SwapchainBuilder swapChainBuilder{ _ctx->gpu, _ctx->device, _ctx->surface };
        _ctx->swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        vkb::Swapchain vkbSwapChain =
            swapChainBuilder.set_allocation_callbacks(_ctx->allocator)
                .set_desired_format(VkSurfaceFormatKHR{ .format     = _ctx->swapChainImageFormat,
                                                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
                .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // TODO: Update to mailbox if possible
                .set_desired_extent(width, height)
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .build()
                .value();

        _ctx->swapChainExtent     = vkbSwapChain.extent;
        _ctx->swapChain           = vkbSwapChain.swapchain;
        _ctx->swapChainImages     = vkbSwapChain.get_images().value();
        _ctx->swapChainImageViews = vkbSwapChain.get_image_views().value();
    }


    void EphemeralEngine::destroySwapChain() const
    {
        vkDestroySwapchainKHR(_ctx->device, _ctx->swapChain, _ctx->allocator);

        for (const auto& imgView : _ctx->swapChainImageViews)
        {
            vkDestroyImageView(_ctx->device, imgView, _ctx->allocator);
        }
    }


    void EphemeralEngine::initImgui() noexcept
    {
        // ------------------------ SETUP DESCRIPTOR POOLS ------------------------
        constexpr std::array poolSizes{
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = 1000 },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, .descriptorCount = 1000 },
        };

        const VkDescriptorPoolCreateInfo poolInfo{ .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                   .pNext         = nullptr,
                                                   .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                                   .maxSets       = 1000,
                                                   .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                                   .pPoolSizes    = poolSizes.data() };

        VK_CHECK(vkCreateDescriptorPool(_ctx->device, &poolInfo, _ctx->allocator, &_ctx->imguiPool));


        // ------------------------ SETUP IMGUI LIBRARY ------------------------
        ImGui::CreateContext();
        ImGui_ImplSDL3_InitForVulkan(_window);

        const VkPipelineRenderingCreateInfo imguiPipelineInfo{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &_ctx->swapChainImageFormat,
        };

        ImGui_ImplVulkan_InitInfo vkImguiInfo{ .ApiVersion          = VK_API_VERSION_1_3,
                                               .Instance            = _ctx->instance,
                                               .PhysicalDevice      = _ctx->gpu,
                                               .Device              = _ctx->device,
                                               .QueueFamily         = _ctx->graphicsQueueFamily,
                                               .Queue               = _ctx->graphicsQueue,
                                               .DescriptorPool      = _ctx->imguiPool,
                                               .MinImageCount       = 3,
                                               .ImageCount          = 3,
                                               .UseDynamicRendering = true,
                                               .Allocator           = _ctx->allocator };
        vkImguiInfo.PipelineInfoMain.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;
        vkImguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo = imguiPipelineInfo;

        ImGui_ImplVulkan_Init(&vkImguiInfo);

        // Deletion Queue
        _ctx->mainDeletionQueue.push([&] {
            ImGui_ImplVulkan_Shutdown();
            vkDestroyDescriptorPool(_ctx->device, _ctx->imguiPool, _ctx->allocator);
        });
    }


    FrameData& EphemeralEngine::currentFrame() const noexcept { return _ctx->frames[_frameNumber % FRAME_OVERLAP]; }

} // namespace ephemeral
