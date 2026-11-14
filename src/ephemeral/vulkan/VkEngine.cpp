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

#include "VkInitializer.h"
#include "VkTypes.h"
#include "VkImages.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <cassert>
#include <chrono>
#include <thread>
#include <ephemeral/utils/DeletionQueue.h>

namespace ephemeral
{

    struct FrameData
    {
        VkCommandPool cmdPool;
        VkCommandBuffer mainCmdBuffer;

        DeletionQueue deletionQueue;
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
        FrameData frames[EphemeralEngine::FRAME_OVERLAP];
        VkQueue graphicsQueue;
        uint32_t graphicsQueueFamily;

        // ------------------------ SYNCHRONIZATION PRIMITIVES ------------------------
        std::array<VkSemaphore, EphemeralEngine::FRAME_OVERLAP> swapChainSemaphores;
        // NOTE: Number of render semaphores must match the number of swap chain images, not frame in flight
        std::vector<VkSemaphore> renderSemaphores;
        std::array<VkFence, EphemeralEngine::FRAME_OVERLAP> renderFences;

        // ------------------------ ENGINE GLOBAL ------------------------
        DeletionQueue mainDeletionQueue;
    };


    EphemeralEngine* loadedEngine = nullptr;


    EphemeralEngine& EphemeralEngine::Get() { return *loadedEngine; }


    void EphemeralEngine::init(std::string appName)
    {
        // only one engine initialization is allowed with the application.
        assert(loadedEngine == nullptr);
        loadedEngine = this;

        _appName = std::move(appName);

        // Allocate memory for context and initialize
        // Here we are using new but a custom allocator is preferred
        _ctx = new EphemeralEngineContext();

        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);
        _window = SDL_CreateWindow(ENGINE_NAME, _windowExtent.width, _windowExtent.height, SDL_WINDOW_VULKAN);

        // ------------------------ INITIALIZE VULKAN ------------------------
        initVulkan();
        initSwapChain();
        initCommands();
        initSyncStructures();

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
            loadedEngine = nullptr;
            // Free the context
            delete _ctx;
        }
    }


    void EphemeralEngine::draw()
    {
        // ------------------------ WAITING & SWAP CHAIN IMAGE ACQUISITION ------------------------
        constexpr auto TIMEOUT        = std::numeric_limits<uint32_t>::max();
        const auto curFence           = _ctx->renderFences[_frameNumber % FRAME_OVERLAP];
        const auto swapChainSemaphore = _ctx->swapChainSemaphores[_frameNumber % FRAME_OVERLAP];

        // Wait for previous frame to finish
        VK_CHECK(vkWaitForFences(_ctx->device, 1, &curFence, true, TIMEOUT));
        // Flush the deletion queue for the current frame
        currentFrame().deletionQueue.flush();
        VK_CHECK(vkResetFences(_ctx->device, 1, &curFence));

        // Request an image from the swapchain
        uint32_t swapChainImageIdx;
        VK_CHECK(
            vkAcquireNextImageKHR(_ctx->device, _ctx->swapChain, TIMEOUT, swapChainSemaphore, nullptr, &
                swapChainImageIdx));
        // NOTE: Render semaphore must be sync with the image not frame index.
        const auto renderSemaphore = _ctx->renderSemaphores[swapChainImageIdx];


        // ------------------------ COMMAND BUFFER RECORDING ------------------------
        // Reset the command buffer and begin recording the commands
        const VkCommandBuffer cmdBuffer = currentFrame().mainCmdBuffer;
        VK_CHECK(vkResetCommandBuffer(cmdBuffer, 0));

        // We will reset the command buffer every time after submission
        const VkCommandBufferBeginInfo cmdBeginInfo = vkutils::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo));


        // Transition the swap chain image to a writeable layout
        vkutils::transitionImage(cmdBuffer, _ctx->swapChainImages[swapChainImageIdx], VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Flashing Background color
        const float flash = std::abs(std::sin(_frameNumber / 120.f));
        const VkClearColorValue clearValue{ flash, flash, 0.0f, 1.0f };
        const VkImageSubresourceRange clearRange = vkutils::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

        // Clear the color
        vkCmdClearColorImage(cmdBuffer, _ctx->swapChainImages[swapChainImageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &clearValue, 1, &clearRange);
        // Transition layout optimal for presentation
        vkutils::transitionImage(cmdBuffer, _ctx->swapChainImages[swapChainImageIdx],
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        // End recording
        vkEndCommandBuffer(cmdBuffer);


        // ------------------------ QUEUE SUBMISSION ------------------------
        VkCommandBufferSubmitInfo cmdBufferSubmitInfo = vkutils::commandBufferSubmitInfo(cmdBuffer);

        VkSemaphoreSubmitInfo waitSemaphoreInfo = vkutils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, swapChainSemaphore);
        VkSemaphoreSubmitInfo renderSemaphoreInfo = vkutils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, renderSemaphore);

        const VkSubmitInfo2 submitInfo = vkutils::submitInfo(&cmdBufferSubmitInfo, &renderSemaphoreInfo,
                                                             &waitSemaphoreInfo);

        VK_CHECK(vkQueueSubmit2(_ctx->graphicsQueue, 1, &submitInfo, curFence));


        // ------------------------ PRESENTATION ------------------------
        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &_ctx->swapChain,
            .pImageIndices = &swapChainImageIdx
        };
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
                // close the window when user alt-f4s or clicks the X button
                if (e.type == SDL_EVENT_QUIT) { bQuit = true; }

                // Stop rendering when the window is in the background.
                if (e.type == SDL_EVENT_WINDOW_MINIMIZED) { stopRendering = true; }
                if (e.type == SDL_EVENT_WINDOW_RESTORED) { stopRendering = false; }
            }

            // do not draw if we are minimized
            if (stopRendering)
            {
                // throttle the speed to avoid the endless spinning
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

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
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing = true,
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


    void EphemeralEngine::initSwapChain() noexcept { createSwapChain(_windowExtent.width, _windowExtent.height); }


    void EphemeralEngine::initCommands() noexcept
    {
        const VkCommandPoolCreateInfo commandPoolInfo = vkutils::commandPoolCreateInfo(
            _ctx->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        for (size_t i = 0; i < FRAME_OVERLAP; ++i)
        {
            VK_CHECK(vkCreateCommandPool(_ctx->device, &commandPoolInfo, _ctx->allocator, &_ctx->frames[i].cmdPool));
            VkCommandBufferAllocateInfo cmdBufferInfo = vkutils::commandBufferAllocateInfo(_ctx->frames[i].cmdPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(_ctx->device, &cmdBufferInfo, &_ctx->frames[i].mainCmdBuffer));
        }
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
                vkCreateSemaphore(_ctx->device, &semaphoreCreateInfo, _ctx->allocator, &_ctx-> swapChainSemaphores[i]));
        }
        _ctx->renderSemaphores.resize(_ctx->swapChainImages.size());
        for (size_t i = 0; i < _ctx->swapChainImages.size(); ++i)
        {
            VK_CHECK(
                vkCreateSemaphore(_ctx->device, &semaphoreCreateInfo, _ctx->allocator, &_ctx->renderSemaphores[i]
                ));
        }
    }


    void EphemeralEngine::createSwapChain(const uint32_t width, const uint32_t height) noexcept
    {
        vkb::SwapchainBuilder swapChainBuilder{ _ctx->gpu, _ctx->device, _ctx->surface };
        _ctx->swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        vkb::Swapchain vkbSwapChain = swapChainBuilder
            .set_allocation_callbacks(_ctx->allocator)
            .set_desired_format(VkSurfaceFormatKHR{ .format = _ctx->swapChainImageFormat,
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

    FrameData& EphemeralEngine::currentFrame() const noexcept { return _ctx->frames[_frameNumber % FRAME_OVERLAP]; }

} // namespace ephemeral
