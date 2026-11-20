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

#include "EngineContext.h"
#include "Extra.h"
#include "GLTFMetallicRoughness.h"
#include "Geometry.h"
#include "VkDescriptors.h"
#include "VkImages.h"
#include "VkInitializer.h"
#include "VkLoader.h"
#include "VkPipelines.h"
#include "VkTypes.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <algorithm>
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

    // TODO: REMOVE/REPLACE
    flcn::Mat4F makePersp(const float aspect, const float fov, const float far, const float near)
    {
        const float s1 = 1 / (std::tan(fov * 0.5f) * aspect);
        const float s2 = 1 / std::tan(fov * 0.5f);
        return flcn::Mat4F{ s1, 0, 0,  0, 0, -s2, 0, 0, 0, 0, -far / (far - near), -(far * near) / (far - near),
                            0,  0, -1, 0 };
    }
    // TODO: REMOVE/REPLACE END




    /// +---------------------------------------------------------------------------------------+
    /// |                              STATIC FUNCTIONS                                         |
    /// +---------------------------------------------------------------------------------------+

    /// TODO: HELPERS REMOVE (TEMP) NOT YET IMPL IN FALCON
    inline flcn::Vec4F clamp(const flcn::Vec4F& vec, const float min, const float max) noexcept
    {
        return flcn::Vec4F{
            std::clamp(vec.x(), min, max),
            std::clamp(vec.y(), min, max),
            std::clamp(vec.z(), min, max),
            std::clamp(vec.w(), min, max),
        };
    }

    inline uint32_t packUnorm4x8(const flcn::Vec4F& vec) noexcept
    {
        flcn::Vec4F clamped = clamp(vec, 0.0f, 1.0f);

        const auto x = static_cast<uint32_t>(std::round(clamped.x() * 255.0f));
        const auto y = static_cast<uint32_t>(std::round(clamped.y() * 255.0f));
        const auto z = static_cast<uint32_t>(std::round(clamped.z() * 255.0f));
        const auto w = static_cast<uint32_t>(std::round(clamped.w() * 255.0f));

        return (w << 24u) | (z << 16u) | (y << 8u) | x;
    }


    /// TODO: HELPERS REMOVE END


    inline static void drawBackground(const VkCommandBuffer cmdBuffer, const uint32_t frameNumber,
                                      const EphemeralEngineContext& ctx) noexcept
    {
        // Bind the pipeline and descriptor sets
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.gradientPipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.gradientPipelineLayout, 0, 1,
                                &ctx.drawImageDescriptorSet, 0, nullptr);
        // Add push constants
        const ComputeEffect& effect = ctx.bgEffects[ctx.currentBgEffect];
        vkCmdPushConstants(cmdBuffer, ctx.gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(effect.data),
                           &effect.data);

        // Execute the compute shader
        vkCmdDispatch(cmdBuffer, std::ceil(ctx.drawExtent.width / 16.0), std::ceil(ctx.drawExtent.height / 16.0), 1);
    }


    inline static void drawGeometry(const VkCommandBuffer cmdBuffer, FrameData& currentFrame,
                                    EphemeralEngineContext& ctx) noexcept
    {
        // PROFILING
        ctx.stats.drawCallCount = 0;
        ctx.stats.triangleCount = 0;
        // Begin clock
        auto start = std::chrono::system_clock::now();

        // ------------------------ SORTING ------------------------
        [[maybe_unused]] std::vector<uint32_t> opaqueDraws;
        if constexpr (ENABLE_MESH_SORT)
        {
            opaqueDraws.reserve(ctx.mainDrawContext.opaqueObjects.size());

            for (uint32_t i = 0; i < ctx.mainDrawContext.opaqueObjects.size(); ++i)
            {
                if (isVisible(ctx.mainDrawContext.opaqueObjects[i], ctx.sceneData.viewProj))
                {
                    opaqueDraws.push_back(i);
                }
            }

            // Sort the opaque surfaces by material and mesh
            std::ranges::sort(opaqueDraws.begin(), opaqueDraws.end(), [&](const auto aIdx, const auto bIdx) {
                const RenderObject& a = ctx.mainDrawContext.opaqueObjects[aIdx];
                const RenderObject& b = ctx.mainDrawContext.opaqueObjects[bIdx];
                if (a.material == b.material)
                {
                    return a.indexBuffer < b.indexBuffer;
                }
                else
                {
                    return a.material < b.material;
                }
            });
        }

        // ------------------------ PER FRAME DESCRIPTOR ------------------------
        AllocatedBuffer gpuSceneBuffer = createBuffer(sizeof(GPUSceneData), ctx.vma, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU);
        // Destroy the scene buffer every frame
        currentFrame.deletionQueue.push([gpuSceneBuffer, &ctx] {
            destroyBuffer(ctx.vma, gpuSceneBuffer);
        });
        const auto sceneUniformData = static_cast<GPUSceneData*>(mapBuffer(ctx.vma, gpuSceneBuffer));
        *sceneUniformData           = ctx.sceneData;
        unmapBuffer(ctx.vma, gpuSceneBuffer);

        VkDescriptorSet globalDescriptor =
            currentFrame.frameDescriptors.allocate(ctx.device, ctx.sceneDescriptorSetLayout, ctx.allocator);
        DescriptorWriter writer;
        writer.writeBuffer(0, gpuSceneBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.updateSet(ctx.device, globalDescriptor);


        // ------------------------ COLOR ATTACHMENTS ------------------------
        const VkRenderingAttachmentInfo colorAttachment =
            vkutils::attachmentInfo(ctx.drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // ------------------------ DEPTH ATTACHMENT ------------------------
        const VkRenderingAttachmentInfo depthAttachment =
            vkutils::depthAttachmentInfo(ctx.depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);


        const VkRenderingInfo renderingInfo =
            vkutils::renderingInfo(ctx.drawExtent, &colorAttachment, &depthAttachment);

        vkCmdBeginRendering(cmdBuffer, &renderingInfo);


        // ------------------------ DYNAMIC STATES ------------------------
        const VkViewport viewport{ .x        = 0,
                                   .y        = 0,
                                   .width    = static_cast<float>(ctx.drawExtent.width),
                                   .height   = static_cast<float>(ctx.drawExtent.height),
                                   .minDepth = 0.0f,
                                   .maxDepth = 1.0f };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        const VkRect2D scissor{ .offset = { .x = 0, .y = 0 },
                                .extent = { .width = ctx.drawExtent.width, .height = ctx.drawExtent.height } };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);


        // ------------------------ RENDER ------------------------
        MaterialPipeline* lastPipeline = nullptr;
        MaterialInstance* lastMaterial = nullptr;
        VkBuffer lastIndexBuffer       = VK_NULL_HANDLE;

        const auto draw = [&](const RenderObject& object) {
            if (object.material != lastMaterial)
            {
                lastMaterial = object.material;

                // Rebind pipeline and descriptors if material changed
                if (object.material->pipeline != lastPipeline)
                {
                    lastPipeline = object.material->pipeline;
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->pipeline);
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            object.material->pipeline->pipelineLayout, 0, 1, &globalDescriptor, 0,
                                            nullptr);

                    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
                    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
                }
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        object.material->pipeline->pipelineLayout, 1, 1, &object.material->materialSet,
                                        0, nullptr);
            }

            // Rebind index buffer if it has already changed
            if (lastIndexBuffer != object.indexBuffer)
            {
                vkCmdBindIndexBuffer(cmdBuffer, object.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            }

            // Push constants
            const GPUDrawPushConstants pc{ .worldMatrix  = object.transform,
                                           .vertexBuffer = object.vertexBufferAddress };
            vkCmdPushConstants(cmdBuffer, object.material->pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(GPUDrawPushConstants), &pc);
            vkCmdDrawIndexed(cmdBuffer, object.indexCount, 1, object.firstIndex, 0, 0);

            // PROFILING
            ++ctx.stats.drawCallCount;
            ctx.stats.triangleCount += object.indexCount / 3;
        };

        if constexpr (ENABLE_MESH_SORT)
        {
            for (const auto objIdx : opaqueDraws)
            {
                // Visibility culling is done priority to copying indices
                draw(ctx.mainDrawContext.opaqueObjects[objIdx]);
            }
        }
        else
        {
            for (const auto& object : ctx.mainDrawContext.opaqueObjects)
            {

                if (isVisible(object, ctx.sceneData.viewProj))
                {
                    draw(object);
                }
            }
        }
        for (const RenderObject& object : ctx.mainDrawContext.transparentObjects)
        {
            draw(object);
        }

        vkCmdEndRendering(cmdBuffer);

        // PROFILING
        auto end               = std::chrono::system_clock::now();
        auto elapsed           = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        ctx.stats.meshDrawTime = static_cast<float>(elapsed.count()) / 1000.0f;
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
                                   static_cast<int>(_windowExtent.height), SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        // ------------------------ INITIALIZE VULKAN ------------------------
        initVulkan();
        initVMA();
        initSwapChain();
        initCommands();
        initSyncStructures();
        initDescriptors();
        initPipelines();


        initImgui();
        initDefaultData();

        _ctx->mainCamera.velocity = flcn::Vec3F::zero();
        _ctx->mainCamera.position = flcn::Vec3F(30.0f, 0.0f, -4.5f);
        _ctx->mainCamera.pitch    = 0;
        _ctx->mainCamera.yaw      = 0;

        // Loading structure glb
        const auto structureFile = loadGLTF(*this, "assets/structure.glb");
        assert(structureFile.has_value() && "File loading failed!");
        _ctx->loadedScenes["structure"] = *structureFile;


        // everything went fine
        _isInitialized = true;
    }


    void EphemeralEngine::cleanup()
    {
        if (_isInitialized)
        {
            // ------------------------ VULKAN ------------------------
            vkDeviceWaitIdle(_ctx->device);
            _ctx->loadedScenes.clear();
            for (auto& frame : _ctx->frames)
            {
                frame.deletionQueue.flush();
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
        updateScene();
        _ctx->drawExtent.width = static_cast<uint32_t>(
            static_cast<float>(std::min(_ctx->swapChainExtent.width, _ctx->drawImage.extent.width)) *
            _ctx->renderScale);
        _ctx->drawExtent.height = static_cast<uint32_t>(
            static_cast<float>(std::min(_ctx->swapChainExtent.height, _ctx->drawImage.extent.height)) *
            _ctx->renderScale);

        // ------------------------ WAITING & SWAP CHAIN IMAGE ACQUISITION ------------------------
        const auto curFence           = _ctx->renderFences[_frameNumber % FRAME_OVERLAP];
        const auto swapChainSemaphore = _ctx->swapChainSemaphores[_frameNumber % FRAME_OVERLAP];

        // Wait for previous frame to finish
        VK_CHECK(vkWaitForFences(_ctx->device, 1, &curFence, true, TIMEOUT));
        // Flush the deletion queue for the current frame
        currentFrame().deletionQueue.flush();
        currentFrame().frameDescriptors.clear(_ctx->device);
        VK_CHECK(vkResetFences(_ctx->device, 1, &curFence));

        // Request an image from the swapchain
        uint32_t swapChainImageIdx;
        const auto imageAcquireResult = vkAcquireNextImageKHR(_ctx->device, _ctx->swapChain, TIMEOUT,
                                                              swapChainSemaphore, nullptr, &swapChainImageIdx);
        if (imageAcquireResult != VK_SUCCESS)
        {
            if (imageAcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
            {
                _ctx->resizeRequested = true;
                return;
            }
            else
            {
                fmt::println("Swapchain error: {}", string_VkResult(imageAcquireResult));
            }
        }

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

        // Transition image to color attachment optimal for geometry
        vkutils::transitionImage(cmdBuffer, _ctx->drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // Transition depth image to depth attachment optimal
        vkutils::transitionImage(cmdBuffer, _ctx->depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        drawGeometry(cmdBuffer, currentFrame(), *_ctx);

        // Transition image to transfer src for blitting to swapchain image.
        vkutils::transitionImage(cmdBuffer, _ctx->drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
        const auto presentResult = vkQueuePresentKHR(_ctx->graphicsQueue, &presentInfo);
        if (presentResult != VK_SUCCESS)
        {
            if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
            {
                _ctx->resizeRequested = true;
            }
            else
            {
                fmt::println("Swapchain error: {}", string_VkResult(presentResult));
            }
        }
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
            // Begin clock
            auto start = std::chrono::system_clock::now();
            // Handle events on queue
            while (SDL_PollEvent(&e) != 0)
            {
                // close the window when user alt-f4s or clicks the X button
                if (e.type == SDL_EVENT_QUIT)
                {
                    bQuit = true;
                }

                ImGui_ImplSDL3_ProcessEvent(&e);
                _ctx->mainCamera.processEvent(e);

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

            // ------------------------ SWAPCHAIN RESIZE ------------------------
            if (_ctx->resizeRequested)
            {
                recreateSwapchain();
            }

            // ------------------------ IMGUI ------------------------
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            if (ImGui::Begin("background"))
            {
                // Resolution scale slider
                ImGui::SliderFloat("Render scale", &_ctx->renderScale, 0.3f, 1.0f);

                ComputeEffect& effect = _ctx->bgEffects[_ctx->currentBgEffect];
                ImGui::Text("Selected effect: %s", effect.name.c_str());
                ImGui::SliderInt("Effect Index", &_ctx->currentBgEffect, 0,
                                 static_cast<int>(_ctx->bgEffects.size()) - 1);
                ImGui::InputFloat4("Data 1", reinterpret_cast<float*>(&effect.data.data1));
                ImGui::InputFloat4("Data 2", reinterpret_cast<float*>(&effect.data.data2));
                ImGui::InputFloat4("Data 3", reinterpret_cast<float*>(&effect.data.data3));
                ImGui::InputFloat4("Data 4", reinterpret_cast<float*>(&effect.data.data4));
                ImGui::End();
            }

            if (ImGui::Begin("Stats"))
            {
                ImGui::Text("Frame time: %0.03f ms", _ctx->stats.frameTime);
                ImGui::Text("Draw time: %0.03f ms", _ctx->stats.meshDrawTime);
                ImGui::Text("Update time: %0.03f ms", _ctx->stats.sceneUpdateTime);
                ImGui::Text("Triangles: %i", _ctx->stats.triangleCount);
                ImGui::Text("Draw Calls: %i", _ctx->stats.drawCallCount);
                ImGui::End();
            }

            ImGui::Render();

            draw();

            // Calculate runtime
            auto end              = std::chrono::system_clock::now();
            auto elapsed          = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            _ctx->stats.frameTime = static_cast<float>(elapsed.count()) / 1000.0f;
        }
    }


    /// +---------------------------------------------------------------------------------------+
    /// |                             PRIVATE METHODS                                           |
    /// +---------------------------------------------------------------------------------------+

    void EphemeralEngine::initVulkan() noexcept
    {
        // ------------------------ INSTANCE CREATION ------------------------
        vkb::InstanceBuilder builder;

        auto instanceResult       = builder.set_app_name(_appName)
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


        // ------------------------ DEPTH IMAGE -----------------------
        auto& depthImage                   = _ctx->depthImage;
        depthImage.extent                  = drawImage.extent;
        depthImage.format                  = VK_FORMAT_D32_SFLOAT;
        VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageCreateInfo depthImageInfo =
            vkutils::imageCreateInfo(depthImage.format, depthImageUsages, depthImage.extent);
        vmaCreateImage(_ctx->vma, &depthImageInfo, &drawImgAllocInfo, &depthImage.image, &depthImage.allocation,
                       nullptr);

        // ImageView
        const VkImageViewCreateInfo depthImgViewInfo =
            vkutils::imageviewCreateInfo(depthImage.format, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(_ctx->device, &depthImgViewInfo, _ctx->allocator, &depthImage.imageView));

        // Add to the deletion queue
        _ctx->mainDeletionQueue.push([&] {
            vkDestroyImageView(_ctx->device, drawImage.imageView, _ctx->allocator);
            vmaDestroyImage(_ctx->vma, drawImage.image, drawImage.allocation);

            vkDestroyImageView(_ctx->device, depthImage.imageView, _ctx->allocator);
            vmaDestroyImage(_ctx->vma, depthImage.image, depthImage.allocation);
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
        std::array sizes{
            DescriptorAllocatorResizable::PoolSizeRatio{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 3 },
            DescriptorAllocatorResizable::PoolSizeRatio{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 3 },
            DescriptorAllocatorResizable::PoolSizeRatio{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3 },
            DescriptorAllocatorResizable::PoolSizeRatio{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 4 }
        };
        _ctx->gDescriptorAllocator.init(_ctx->device, _ctx->allocator, 1000, sizes);

        // Make descriptor set layout for our compute shader
        DescriptorSetLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _ctx->drawImageDescriptorSetLayout = builder.build(_ctx->device, VK_SHADER_STAGE_COMPUTE_BIT, _ctx->allocator);
        // Allocate the descriptor the image
        _ctx->drawImageDescriptorSet =
            _ctx->gDescriptorAllocator.allocate(_ctx->device, _ctx->drawImageDescriptorSetLayout, _ctx->allocator);


        // ------------------------ ATTACH OUR DRAW IMAGE TO THE DESCRIPTOR ------------------------
        DescriptorWriter writer;
        writer.writeImage(0, _ctx->drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.updateSet(_ctx->device, _ctx->drawImageDescriptorSet);

        for (size_t i = 0; i < FRAME_OVERLAP; ++i)
        {
            std::vector<DescriptorAllocatorResizable::PoolSizeRatio> frameSizes{
                { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 3 },
                { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 3 },
                { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3 },
                { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 4 }
            };
            _ctx->frames[i].frameDescriptors = DescriptorAllocatorResizable();
            _ctx->frames[i].frameDescriptors.init(_ctx->device, _ctx->allocator, 1000, frameSizes);

            _ctx->mainDeletionQueue.push([&, i] {
                _ctx->frames[i].frameDescriptors.destroy(_ctx->device, _ctx->allocator);
            });
        }

        DescriptorSetLayoutBuilder sceneLayoutBuilder;
        sceneLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // TODO: Updated to both vertex and fragment shader
        // NOLINTBEGIN
        _ctx->sceneDescriptorSetLayout = sceneLayoutBuilder.build(
            _ctx->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, _ctx->allocator);
        // NOLINTEND

        // TEXTURE SAMPLER DESCRIPTORS
        DescriptorSetLayoutBuilder imageDescriptorLayoutBuilder;
        imageDescriptorLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _ctx->singleImageDescriptorLayout =
            imageDescriptorLayoutBuilder.build(_ctx->device, VK_SHADER_STAGE_FRAGMENT_BIT, _ctx->allocator);

        _ctx->mainDeletionQueue.push([&] {
            vkDestroyDescriptorSetLayout(_ctx->device, _ctx->singleImageDescriptorLayout, _ctx->allocator);
            vkDestroyDescriptorSetLayout(_ctx->device, _ctx->sceneDescriptorSetLayout, _ctx->allocator);
            _ctx->gDescriptorAllocator.destroy(_ctx->device, _ctx->allocator);
            vkDestroyDescriptorSetLayout(_ctx->device, _ctx->drawImageDescriptorSetLayout, _ctx->allocator);
        });
    }


    void EphemeralEngine::initPipelines() noexcept
    {
        initBackgroundPipelines();
        initMeshPipelines();

        _ctx->metalRoughMaterial.buildPipelines(*_ctx);
    }


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
        VkShaderModule gradientShader;
        if (!vkutils::loadShaderModule("shaders/GradientColor.comp.spv", _ctx->device, _ctx->allocator,
                                       &gradientShader))
        {
            fmt::print("There was an error loading the color gradient shader module.");
        }

        VkShaderModule skyShader;
        if (!vkutils::loadShaderModule("shaders/Sky.comp.spv", _ctx->device, _ctx->allocator, &skyShader))
        {
            fmt::print("There was an error loading the sky shader module.");
        }

        const VkPipelineShaderStageCreateInfo shaderStageInfo{ .sType =
                                                                   VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                               .pNext  = nullptr,
                                                               .flags  = 0,
                                                               .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                                                               .module = gradientShader,
                                                               .pName  = "main" };


        // ------------------------ PIPELINE ------------------------
        VkComputePipelineCreateInfo computePipelineInfo{ .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                         .pNext  = nullptr,
                                                         .flags  = 0,
                                                         .stage  = shaderStageInfo,
                                                         .layout = _ctx->gradientPipelineLayout };

        ComputeEffect gradient;
        gradient.pipelineLayout = _ctx->gradientPipelineLayout;
        gradient.name           = "gradient";
        gradient.data           = {};
        gradient.data.data1     = { 1, 0, 0, 1 };
        gradient.data.data2     = { 0, 0, 1, 1 };

        VK_CHECK(vkCreateComputePipelines(_ctx->device, VK_NULL_HANDLE, 1, &computePipelineInfo, _ctx->allocator,
                                          &_ctx->gradientPipeline));

        computePipelineInfo.stage.module = skyShader;

        ComputeEffect sky;
        sky.pipelineLayout = _ctx->gradientPipelineLayout;
        sky.name           = "sky";
        sky.data           = {};
        sky.data.data1     = { 0.1f, 0.2f, 0.4f, 0.97f };

        _ctx->bgEffects.push_back(gradient);
        _ctx->bgEffects.push_back(sky);


        // We can create the shader module after its bound to the pipeline
        vkDestroyShaderModule(_ctx->device, skyShader, _ctx->allocator);
        vkDestroyShaderModule(_ctx->device, gradientShader, _ctx->allocator);

        // Add the pipeline layout and pipeline to the deletion queue
        _ctx->mainDeletionQueue.push([&] {
            vkDestroyPipelineLayout(_ctx->device, _ctx->gradientPipelineLayout, _ctx->allocator);
            vkDestroyPipeline(_ctx->device, _ctx->gradientPipeline, _ctx->allocator);
        });
    }


    void EphemeralEngine::initMeshPipelines() noexcept
    {
        // ------------------------ SHADER MODULES ------------------------
        VkShaderModule vertexShader, fragmentShader;
        if (!vkutils::loadShaderModule("shaders/ColoredTriangleMesh.vert.spv", _ctx->device, _ctx->allocator,
                                       &vertexShader))
        {
            fmt::println("There was an error loading the mesh vertex shader");
            return;
        }
        if (!vkutils::loadShaderModule("shaders/TextureImage.frag.spv", _ctx->device, _ctx->allocator, &fragmentShader))
        {
            fmt::println("There was an error loading the mesh fragment shader");
            return;
        }


        // ------------------------ PIPELINE LAYOUT ------------------------
        VkPushConstantRange pushConstants{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                           .offset     = 0,
                                           .size       = sizeof(GPUDrawPushConstants) };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutils::pipelineLayoutCreateInfo();
        pipelineLayoutInfo.pushConstantRangeCount     = 1;
        pipelineLayoutInfo.pPushConstantRanges        = &pushConstants;
        pipelineLayoutInfo.setLayoutCount             = 1;
        pipelineLayoutInfo.pSetLayouts                = &_ctx->singleImageDescriptorLayout;

        VK_CHECK(vkCreatePipelineLayout(_ctx->device, &pipelineLayoutInfo, _ctx->allocator, &_ctx->meshPipelineLayout));


        // ------------------------ PIPELINE BUILD ------------------------
        PipelineBuilder pipelineBuilder;
        pipelineBuilder.setPipelineLayout(_ctx->meshPipelineLayout);
        pipelineBuilder.setShaderStages(vertexShader, fragmentShader);
        pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
        pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        pipelineBuilder.disableMSAA();
        pipelineBuilder.enableAdditiveBlending();
        pipelineBuilder.setDepthTestState(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

        pipelineBuilder.setColorAttachmentFormat(_ctx->drawImage.format);
        pipelineBuilder.setDepthAttachmentFormat(_ctx->depthImage.format);
        _ctx->meshPipeline = pipelineBuilder.build(_ctx->device, _ctx->allocator);


        // ------------------------ CLEANUP ------------------------
        vkDestroyShaderModule(_ctx->device, vertexShader, _ctx->allocator);
        vkDestroyShaderModule(_ctx->device, fragmentShader, _ctx->allocator);

        _ctx->mainDeletionQueue.push([&] {
            vkDestroyPipelineLayout(_ctx->device, _ctx->meshPipelineLayout, _ctx->allocator);
            vkDestroyPipeline(_ctx->device, _ctx->meshPipeline, _ctx->allocator);
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


    void EphemeralEngine::initDefaultData() noexcept
    {
        // TEST MESH
        _ctx->testMeshes = loadGltfMeshes(*this, "assets/basicmesh.glb").value();

        _ctx->mainDeletionQueue.push([&] {
            for (auto&& mesh : _ctx->testMeshes)
            {
                destroyBuffer(_ctx->vma, mesh->meshBuffers.indexBuffer);
                destroyBuffer(_ctx->vma, mesh->meshBuffers.vertexBuffer);
            }
        });

        // TEST TEXTURES
        // ------------------------ CREATE COLORS ------------------------
        const uint32_t white   = packUnorm4x8(flcn::Vec4F::one());
        const uint32_t black   = packUnorm4x8(flcn::Vec4F{ 0.0f, 0.0f, 0.0f, 1.0f });
        const uint32_t gray    = packUnorm4x8(flcn::Vec4F{ 0.66f, 0.66f, 0.66f, 1.0f });
        const uint32_t magenta = packUnorm4x8(flcn::Vec4F{ 1, 0, 1, 1 });

        std::array<uint32_t, 16ull * 16ull> pixels; // 16x16 checkerboard pattern
        for (size_t x = 0; x < 16ull; ++x)
        {
            for (size_t y = 0; y < 16ull; ++y)
            {
                pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : gray;
            }
        }

        // ------------------------ CREATE IMAGES ------------------------
        ImageCreateConfig config{ .size   = VkExtent3D{ .width = 1, .height = 1, .depth = 1 },
                                  .format = VK_FORMAT_R8G8B8A8_UNORM,
                                  // NOLINTNEXTLINE
                                  .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                  .mipmapped  = false };

        _ctx->whiteImage = createImage(_ctx->device, _ctx->allocator, _ctx->vma, config);
        _ctx->blackImage = createImage(_ctx->device, _ctx->allocator, _ctx->vma, config);
        _ctx->grayImage  = createImage(_ctx->device, _ctx->allocator, _ctx->vma, config);
        uploadImage(config, _ctx->whiteImage, &white);
        uploadImage(config, _ctx->blackImage, &black);
        uploadImage(config, _ctx->grayImage, &gray);


        config.size                  = VkExtent3D{ .width = 16, .height = 16, .depth = 1 };
        _ctx->errorCheckerBoardImage = createImage(_ctx->device, _ctx->allocator, _ctx->vma, config);
        uploadImage(config, _ctx->errorCheckerBoardImage, pixels.data());


        // ------------------------ CREATE SAMPLERS ------------------------
        VkSamplerCreateInfo samplerInfo{
            .sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
        };
        vkCreateSampler(_ctx->device, &samplerInfo, _ctx->allocator, &_ctx->defaultSamplerNearest);

        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        vkCreateSampler(_ctx->device, &samplerInfo, _ctx->allocator, &_ctx->defaultSamplerLinear);


        // ------------------------ GLTF Material Resources ------------------------
        GLTFMetallicRoughness::MaterialResources materialResources;
        materialResources.colorImage        = _ctx->whiteImage;
        materialResources.colorSampler      = _ctx->defaultSamplerLinear;
        materialResources.metalRoughImage   = _ctx->whiteImage;
        materialResources.metalRoughSampler = _ctx->defaultSamplerLinear;

        AllocatedBuffer materialConstants =
            createBuffer(sizeof(GLTFMetallicRoughness::MaterialConstants), _ctx->vma,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        const auto sceneUniformData =
            static_cast<GLTFMetallicRoughness::MaterialConstants*>(materialConstants.allocationInfo.pMappedData);
        sceneUniformData->colorFactors          = flcn::Vec4F::one();
        sceneUniformData->metalRoughnessFactors = flcn::Vec4F{ 1.0f, 0.5f, 0.0f, 0.0f };

        materialResources.dataBuffer   = materialConstants.buffer;
        materialResources.bufferOffset = 0;

        _ctx->defaultData = _ctx->metalRoughMaterial.writeMaterial(
            _ctx->device, MaterialPass::MAIN_COLOR, _ctx->allocator, materialResources, _ctx->gDescriptorAllocator);

        _ctx->mainDeletionQueue.push([&, materialConstants] {
            destroyBuffer(_ctx->vma, materialConstants);
            vkDestroySampler(_ctx->device, _ctx->defaultSamplerLinear, _ctx->allocator);
            vkDestroySampler(_ctx->device, _ctx->defaultSamplerNearest, _ctx->allocator);

            destroyImage(_ctx->device, _ctx->allocator, _ctx->errorCheckerBoardImage, _ctx->vma);
            destroyImage(_ctx->device, _ctx->allocator, _ctx->grayImage, _ctx->vma);
            destroyImage(_ctx->device, _ctx->allocator, _ctx->blackImage, _ctx->vma);
            destroyImage(_ctx->device, _ctx->allocator, _ctx->whiteImage, _ctx->vma);
        });

        // ------------------------ TEST DATA ------------------------
        for (const auto& mesh : _ctx->testMeshes)
        {
            auto meshNode  = std::make_shared<MeshNode>();
            meshNode->mesh = mesh;

            meshNode->localTransform = flcn::Mat4F::identity();
            meshNode->worldTransform = flcn::Mat4F::identity();

            for (auto& surface : meshNode->mesh->surfaces)
            {
                surface.material              = std::make_shared<GLTFMaterial>(_ctx->defaultData);
                _ctx->loadedNodes[mesh->name] = std::move(meshNode);
            }
        }
    }


    void EphemeralEngine::recreateSwapchain() noexcept
    {
        // Destroy the swapchain
        vkDeviceWaitIdle(_ctx->device);
        destroySwapChain();

        // Get the new width and height
        int w, h;
        SDL_GetWindowSize(_window, &w, &h);
        _windowExtent.width  = w;
        _windowExtent.height = h;

        // Create the swap chain with the new size.
        createSwapChain(_windowExtent.width, _windowExtent.height);

        _ctx->resizeRequested = false;
    }


    void EphemeralEngine::uploadImage(const ImageCreateConfig& config, const VulkanImage& outImage,
                                      const void* data) const noexcept
    {
        // Create a staging buffer
        const uint32_t dataSize = config.size.width * config.size.height * config.size.depth * 4; // 4 = RGBA
        const AllocatedBuffer stagingBuffer =
            createBuffer(dataSize, _ctx->vma, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Copy the data into the staging buffer
        memcpy(stagingBuffer.allocationInfo.pMappedData, data, dataSize);

        // Immediately transfer the image to optimal layout and record the command buffer
        immediateSubmit(*_ctx, [&](const VkCommandBuffer cmdBuffer) {
            vkutils::transitionImage(cmdBuffer, outImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const VkBufferImageCopy copyRegion{ .bufferOffset      = 0,
                                                .bufferRowLength   = 0,
                                                .bufferImageHeight = 0,
                                                .imageSubresource  = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                       .mipLevel       = 0,
                                                                       .baseArrayLayer = 0,
                                                                       .layerCount     = 1 },
                                                .imageExtent       = config.size };

            vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer.buffer, outImage.image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
            if (config.mipmapped)
            {
                vkutils::generateMipmaps(cmdBuffer, outImage.image, VkExtent2D{outImage.extent.width, outImage.extent.height});
            }
            else
            {
                vkutils::transitionImage(cmdBuffer, outImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });

        // Destroy the staging buffer.
        destroyBuffer(_ctx->vma, stagingBuffer);
    }


    void EphemeralEngine::updateScene() noexcept
    {
        // PROFILING
        const auto start = std::chrono::system_clock::now();

        auto& cam = _ctx->mainCamera;
        cam.update();

        _ctx->mainDrawContext.opaqueObjects.clear();
        _ctx->mainDrawContext.transparentObjects.clear();

        for (int x = -3; x < 3; ++x)
        {
            flcn::Mat4F scale       = flcn::Mat4F::makeScale(0.4f);
            flcn::Mat4F translation = flcn::Mat4F::identity();
            translation(0, 3)       = static_cast<float>(x);
            translation(1, 3)       = 1;

            _ctx->loadedNodes["Cube"]->draw(translation * scale, _ctx->mainDrawContext);
        }

        _ctx->loadedNodes["Suzanne"]->draw(flcn::Mat4F::identity(), _ctx->mainDrawContext);



        // flcn::Mat4F view = flcn::Mat4F::identity();
        // view(2, 3)       = -3;
        // float fov         = std::numbers::pi / 4; // 45deg
        float fov            = 1.22173f; // 70deg
        _ctx->sceneData.view = cam.getViewMatrix();
        _ctx->sceneData.proj = makePersp(
            static_cast<float>(_windowExtent.width) / static_cast<float>(_windowExtent.height), fov, 0.1f, 10000.0f);
        _ctx->sceneData.viewProj = _ctx->sceneData.proj * _ctx->sceneData.view;

        _ctx->sceneData.ambientColor  = flcn::Vec4F(0.1f, 0.1f, 0.1f, 0.1f);
        _ctx->sceneData.sunlightColor = flcn::Vec4F::one();
        _ctx->sceneData.sunlightDir   = flcn::Vec4F(0.0f, 1.0f, 0.5f, 1.0f);

        _ctx->loadedScenes["structure"]->draw(flcn::Mat4F::identity(), _ctx->mainDrawContext);

        // PROFILING
        const auto end              = std::chrono::system_clock::now();
        const auto elapsed          = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        _ctx->stats.sceneUpdateTime = static_cast<float>(elapsed.count()) / 1000.0f;
    }


    FrameData& EphemeralEngine::currentFrame() const noexcept { return _ctx->frames[_frameNumber % FRAME_OVERLAP]; }


    void EphemeralEngine::uploadMesh(const std::span<uint32_t> indices, const std::span<Vertex> vertices,
                                     GPUMeshBuffers& outBuffer) const noexcept
    {
        const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
        const size_t indexBufferSize  = indices.size() * sizeof(uint32_t);

        // ------------------------ VERTEX BUFFER CREATION ------------------------
        // NOLINTBEGIN
        outBuffer.vertexBuffer =
            createBuffer(vertexBufferSize, _ctx->vma,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VMA_MEMORY_USAGE_GPU_ONLY);
        // NOLINTEND


        // ------------------------ FIND VERTEX ADDRESS ------------------------
        const VkBufferDeviceAddressInfo deviceAddressInfo{ .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                           .buffer = outBuffer.vertexBuffer.buffer };
        outBuffer.vertexBufferAddress = vkGetBufferDeviceAddress(_ctx->device, &deviceAddressInfo);


        // ------------------------ INDEX BUFFER CREATION ------------------------
        // NOLINTBEGIN
        outBuffer.indexBuffer = createBuffer(indexBufferSize, _ctx->vma,
                                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VMA_MEMORY_USAGE_GPU_ONLY);
        // NOLINTEND


        // ------------------------ BUFFER COPY USING STAGING BUFFER ------------------------
        const AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, _ctx->vma,
                                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        void* data                    = staging.allocationInfo.pMappedData;

        // Copy Vertex Buffer
        memcpy(data, vertices.data(), vertexBufferSize);
        // Copy Index Buffer
        memcpy(static_cast<uint8_t*>(data) + vertexBufferSize, indices.data(), indexBufferSize);

        immediateSubmit(*_ctx, [&](VkCommandBuffer cmdBuffer) {
            const VkBufferCopy vertexCopy{ .srcOffset = 0, .dstOffset = 0, .size = vertexBufferSize };
            vkCmdCopyBuffer(cmdBuffer, staging.buffer, outBuffer.vertexBuffer.buffer, 1, &vertexCopy);

            const VkBufferCopy indexCopy{ .srcOffset = vertexBufferSize, .dstOffset = 0, .size = indexBufferSize };
            vkCmdCopyBuffer(cmdBuffer, staging.buffer, outBuffer.indexBuffer.buffer, 1, &indexCopy);
        });

        destroyBuffer(_ctx->vma, staging);
    }

} // namespace ephemeral
