// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include "Geometry.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>


/// Forward declarations
struct SDL_Window; // NOLINT(readability-identifier-naming)

namespace ephemeral
{
    struct Extent2D
    { uint32_t width, height; };

    class EphemeralEngine
    {
    public:
        static EphemeralEngine& get();

        // initializes everything in the engine
        void init(std::string appName);

        // shuts down the engine
        void cleanup();

        // draw loop
        void draw();

        // run main loop
        void run();

        // TODO: Move back to private
        void uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices,
                        struct GPUMeshBuffers& outBuffer) const noexcept; // NOLINT(readability-identifier-naming)

    protected:
        bool _isInitialized{ false };
        int _frameNumber{ 0 };
        bool _stopRendering{ false };
        Extent2D _windowExtent{ .width = 1700, .height = 900 };
        std::string _appName{};

        SDL_Window* _window{ nullptr };

        struct EphemeralEngineContext* _ctx{}; // NOLINT(readability-identifier-naming)

    private:
        void initVulkan() noexcept;

        void initVMA() noexcept;

        void initSwapChain() noexcept;

        void initCommands() noexcept;

        void initSyncStructures() noexcept;

        void initDescriptors() noexcept;

        void initPipelines() noexcept;

        void initBackgroundPipelines() noexcept;

        void initMeshPipelines() noexcept;

        void createSwapChain(uint32_t width, uint32_t height) noexcept;

        void destroySwapChain() const;

        void initImgui() noexcept;

        void initDefaultData() noexcept;

        void recreateSwapchain() noexcept;

        // NOLINTBEGIN
        void uploadImage(const struct ImageCreateConfig& config, const struct VulkanImage& outImage,
                         const void* data) const noexcept;
        // NOLINTEND

        void updateScene() noexcept;


        [[nodiscard]] struct FrameData& currentFrame() const noexcept; // NOLINT(readability-identifier-naming)

        // NOLINTNEXTLINE
        friend std::optional<std::shared_ptr<struct LoadedGLTF>> loadGLTF(EphemeralEngine&,
                                                                          const std::filesystem::path&) noexcept;
    };

} // namespace ephemeral
