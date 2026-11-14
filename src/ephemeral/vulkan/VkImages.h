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


#include <vulkan/vulkan.h>


namespace ephemeral::vkutils
{
    void transitionImage(VkCommandBuffer cmdBuffer, VkImage img, VkImageLayout oldLayout,
                         VkImageLayout newLayout) noexcept;
}
