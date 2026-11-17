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

    struct GPUSceneData
    {
        flcn::Mat4F view;
        flcn::Mat4F proj;
        flcn::Mat4F viewProj;
        flcn::Vec4F ambientColor;
        flcn::Vec4F sunlightDir; // W-coordinate reserved light power.
        flcn::Vec4F sunlightColor;
    };

} // namespace ephemeral
