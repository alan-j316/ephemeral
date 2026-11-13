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
#include <cassert>
#include <chrono>
#include <thread>

namespace ephemeral
{

    EphemeralEngine* loadedEngine = nullptr;

    EphemeralEngine& EphemeralEngine::Get() { return *loadedEngine; }

    void EphemeralEngine::init()
    {
        // only one engine initialization is allowed with the application.
        assert(loadedEngine == nullptr);
        loadedEngine = this;

        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);

        _window = SDL_CreateWindow(ENGINE_NAME, _windowExtent.width, _windowExtent.height, SDL_WINDOW_VULKAN);

        // everything went fine
        _isInitialized = true;
    }


    void EphemeralEngine::cleanup()
    {
        if (_isInitialized)
        {

            SDL_DestroyWindow(_window);
        }

        // clear engine pointer
        loadedEngine = nullptr;
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
                if (e.type == SDL_EVENT_QUIT)
                {
                    bQuit = true;
                }

                // Stop rendering when the window is in the background.
                if (e.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    stopRendering = true;
                }
                if (e.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    stopRendering = false;
                }
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

} // namespace ephemeral
