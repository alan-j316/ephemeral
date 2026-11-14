#pragma once
/**
 * @file Geometry.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 14, 2026
 *
 * @brief Structure for geometries.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include <falcon/Falcon.h>

namespace ephemeral
{
    struct Vertex
    {
        flcn::Vec4F color;
        flcn::Vec3F position;
        float uvX;
        flcn::Vec3F normal;
        float uvY;
    };

} // namespace ephemeral
