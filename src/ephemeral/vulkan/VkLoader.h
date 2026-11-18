#pragma once
/**
 * @file VkLoader.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Vulkan Loaders.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "RenderPrimitives.h"
#include "VkTypes.h"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

namespace ephemeral
{
    struct GLTFMaterial
    { MaterialInstance material; };

    struct GeoSurface
    {
        uint32_t startIndex;
        uint32_t count;
        std::shared_ptr<GLTFMaterial> material;
    };

    struct MeshAsset
    {
        std::string name;
        std::vector<GeoSurface> surfaces;
        GPUMeshBuffers meshBuffers;
    };


    // Forward Declaration
    class EphemeralEngine; // NOLINT

    std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
        EphemeralEngine& engine, const std::filesystem::path& filePath) noexcept;

} // namespace ephemeral
