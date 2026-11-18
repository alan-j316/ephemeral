#pragma once
/**
 * @file Camera.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Camera.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include <falcon/Falcon.h>

/// Forward declaration
union SDL_Event; // NOLINT

namespace ephemeral
{
    class Camera
    {

        static constexpr float SENSITIVITY_MULTIPLIER = 0.001f;
        static constexpr float VELOCITY_REDUCER       = 0.1f;

    public:
        flcn::Vec3F velocity;
        flcn::Vec3F position;

        float pitch{}, yaw{};

        [[nodiscard]] inline flcn::Mat4F getRotationMatrix() const noexcept
        {
            const flcn::Mat4F pitchRotation = flcn::Mat4F::makeRotationX(pitch);
            const flcn::Mat4F yawRotation   = flcn::Mat4F::makeRotationY(-yaw);
            return yawRotation * pitchRotation;
        }

        [[nodiscard]] inline flcn::Mat4F getViewMatrix() noexcept
        {
            flcn::Mat4F cameraTranslation    = flcn::Mat4F::identity();
            cameraTranslation(0, 3)          = position.x();
            cameraTranslation(1, 3)          = position.y();
            cameraTranslation(2, 3)          = position.z();
            const flcn::Mat4F cameraRotation = getRotationMatrix();
            return (cameraTranslation * cameraRotation).inverse();
        }

        inline void update()
        {
            const auto cameraRotation = getRotationMatrix();
            position += (cameraRotation * flcn::Vec4F(velocity * VELOCITY_REDUCER, 0.0f))
                            .swizzle<flcn::axis::X, flcn::axis::Y, flcn::axis::Z>();
        }

        void processEvent(const SDL_Event& e);
    };

} // namespace ephemeral
