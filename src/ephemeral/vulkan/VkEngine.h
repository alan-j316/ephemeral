// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <cstdint>

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
        void init();

        // shuts down the engine
        void cleanup();

        // draw loop
        void draw();

        // run main loop
        void run();

        // Global variables
        static constexpr auto ENGINE_NAME = "Ephemeral";

    protected:
        bool _isInitialized{ false };
        int _frameNumber{ 0 };
        bool stopRendering{ false };
        Extent2D _windowExtent{ .width = 1700, .height = 900 };
    };

} // namespace ephemeral
