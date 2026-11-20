#pragma once
/**
 * @file Extra.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 20, 2026
 *
 * @brief Utility and helper functions.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include <array>
#include <falcon/Falcon.h>

namespace flcn
{
    inline Vec3F min(const Vec3F& a, const Vec3F& b) noexcept
    { return { std::min(a.x(), b.x()), std::min(a.y(), b.y()), std::min(a.z(), b.z()) }; }

    inline Vec3F max(const Vec3F& a, const Vec3F& b) noexcept
    { return { std::max(a.x(), b.x()), std::max(a.y(), b.y()), std::max(a.z(), b.z()) }; }

    // Hadamard product
    inline Vec3F operator*(const Vec3F& a, const Vec3F& b) noexcept
    { return { a.x() * b.x(), a.y() * b.y(), a.z() * b.z() }; }

} // namespace flcn

namespace ephemeral
{
    inline bool isVisible(const RenderObject& object, const flcn::Mat4F& viewProj)
    {
        const std::array corners{
            flcn::Vec3F{ 1, 1, 1 },  flcn::Vec3F{ 1, 1, -1 },  flcn::Vec3F{ 1, -1, 1 },  flcn::Vec3F{ 1, -1, -1 },
            flcn::Vec3F{ -1, 1, 1 }, flcn::Vec3F{ -1, 1, -1 }, flcn::Vec3F{ -1, -1, 1 }, flcn::Vec3F{ -1, -1, -1 },
        };

        const flcn::Mat4F matrix = viewProj * object.transform;

        flcn::Vec3F min{ 1.5, 1.5, 1.5 };
        flcn::Vec3F max{ -1.5, -1.5, -1.5 };

        for (size_t i = 0; i < corners.size(); ++i)
        {
            // Project each corner into clip space
            flcn::Vec4 vert = matrix * flcn::Vec4F(object.bounds.origin + (corners[i] * object.bounds.extents), 1.0f);

            // Perspective correction
            if (vert.w() > 1e-5)
            {
                vert /= vert.w();
            }
            const auto vert3 = vert.swizzle<flcn::axis::X, flcn::axis::Y, flcn::axis::Z>();

            min = flcn::min(vert3, min);
            max = flcn::max(vert3, max);
        }

        // Check if the clip box is within the view
        // NDC -1 to 1 (For X and Y) and 0 to 1 for Z/Depth.
        if (min.z() > 1.0f || max.z() < 0.0f || min.x() > 1.0f || max.x() < -1.0f || min.y() > 1.0f || max.y() < -1.0f)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

} // namespace ephemeral
