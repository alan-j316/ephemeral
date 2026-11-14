// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include "VkBuffer.h"

#include <array>
#include <deque>
#include <falcon/Falcon.h>
#include <fmt/core.h>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define VK_CHECK(x)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        VkResult err = x;                                                                                              \
        if (err)                                                                                                       \
        {                                                                                                              \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err));                                           \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)


namespace ephemeral
{
    // Holds resources required for a mesh
    struct GPUMeshBuffers
    {
        AllocatedBuffer indexBuffer;
        AllocatedBuffer vertexBuffer;
        VkDeviceAddress vertexBufferAddress;
    };


    // Push constants for our mesh object
    struct GPUDrawPushConstants
    {
        flcn::Mat4F worldMatrix;
        VkDeviceAddress vertexBuffer;
    };
} // namespace ephemeral
