/**
 * @file Camera.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Implementation of member function declared in Camera.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "Camera.h"

#include <SDL3/SDL.h>


namespace ephemeral
{
    void Camera::processEvent(const SDL_Event& e)
    {
        if (e.type == SDL_EVENT_KEY_DOWN)
        {
            if (e.key.scancode == SDL_SCANCODE_W)
            {
                velocity.z() = -1;
            }
            if (e.key.scancode == SDL_SCANCODE_S)
            {
                velocity.z() = 1;
            }
            if (e.key.scancode == SDL_SCANCODE_A)
            {
                velocity.x() = -1;
            }
            if (e.key.scancode == SDL_SCANCODE_D)
            {
                velocity.x() = 1;
            }
        }

        if (e.type == SDL_EVENT_KEY_UP)
        {
            if (e.key.scancode == SDL_SCANCODE_W)
            {
                velocity.z() = 0;
            }
            if (e.key.scancode == SDL_SCANCODE_S)
            {
                velocity.z() = 0;
            }
            if (e.key.scancode == SDL_SCANCODE_A)
            {
                velocity.x() = 0;
            }
            if (e.key.scancode == SDL_SCANCODE_D)
            {
                velocity.x() = 0;
            }
        }

        if (e.type == SDL_EVENT_MOUSE_MOTION)
        {
            yaw += e.motion.xrel / 200.0f;
            pitch += e.motion.yrel / 200.0f;
        }
    }
} // namespace ephemeral
