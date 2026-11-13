// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <cstdint>
#include <string>


/// Forward declarations
struct SDL_Window;


namespace ephemeral
{
    struct Extent2D
    {
        uint32_t width, height;
    };

    class EphemeralEngine
    {
    public:
        SDL_Window* _window{ nullptr };

        static EphemeralEngine& Get();

        // initializes everything in the engine
        void init(std::string appName);

        // shuts down the engine
        void cleanup();

        // draw loop
        void draw();

        // run main loop
        void run();

        // Config variables
        static constexpr auto ENGINE_NAME               = "Ephemeral";
        static constexpr bool VALIDATION_LAYERS_ENABLED = true;
        static constexpr size_t FRAME_OVERLAP    = 2;

    protected:
        bool _isInitialized{ false };
        int _frameNumber{ 0 };
        bool stopRendering{ false };
        Extent2D _windowExtent{ .width = 1700, .height = 900 };
        std::string _appName{};

        struct EphemeralEngineContext* _ctx{};

    private:
        void initVulkan() noexcept;

        void initSwapChain() noexcept;

        void initCommands() noexcept;

        void initSyncStructures() noexcept;

        void createSwapChain(uint32_t width, uint32_t height) noexcept;

        void destroySwapChain() const;

        struct FrameData& currentFrame() const noexcept;
    };

} // namespace ephemeral
