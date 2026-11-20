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

    struct ImageCreateConfig
    {
        VkExtent3D size;
        VkFormat format;
        VkImageUsageFlags usageFlags;
        bool mipmapped{ false };
    };


    /// Create a @ref VulkanImage of the given @p size, @p format and with the @p usageFlags.
    VulkanImage createImage(VkDevice device, const VkAllocationCallbacks* allocator, VmaAllocator vma,
                            const ImageCreateConfig& config) noexcept;


    /// Destroy the vulkan image.
    void destroyImage(VkDevice device, const VkAllocationCallbacks* allocator, const VulkanImage& image,
                 VmaAllocator vma) noexcept;


    namespace vkutils
    {
        void generateMipmaps(VkCommandBuffer cmdBuffer, VkImage image, VkExtent2D extent) noexcept;

        void transitionImage(VkCommandBuffer cmdBuffer, VkImage img, VkImageLayout oldLayout,
                             VkImageLayout newLayout) noexcept;

        void copyImageToImage(VkCommandBuffer cmdBuffer, VkImage src, VkImage dst, VkExtent2D srcSize,
                              VkExtent2D dstSize) noexcept;

    } // namespace vkutils
} // namespace ephemeral
