#pragma once
/**
 * @file VkImages.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Vulkan image wrapper.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "VkTypes.h"

namespace ephemeral
{
    struct VulkanImage
    {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        VkExtent3D extent;
        VkFormat format;
    };


    namespace vkutils
    {
        void transitionImage(VkCommandBuffer cmdBuffer, VkImage img, VkImageLayout oldLayout,
                             VkImageLayout newLayout) noexcept;

        void copyImageToImage(VkCommandBuffer cmdBuffer, VkImage src, VkImage dst, VkExtent2D srcSize,
                              VkExtent2D dstSize) noexcept;

    } // namespace vkutils
} // namespace ephemeral
