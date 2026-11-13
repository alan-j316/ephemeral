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

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <cassert>
#include <chrono>
#include <thread>

namespace ephemeral
{

    struct FrameData
    {
        VkCommandPool cmdPool;
        VkCommandBuffer mainCmdBuffer;
    };


    // Main Engine Context(PIMPL used to remove header leakage)
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
                vkDestroyCommandPool(_ctx->device, frame.cmdPool, _ctx->allocator);
            }
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
        // nothing yet
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


    void EphemeralEngine::initSyncStructures() noexcept {}


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
