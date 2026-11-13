/**
 * @file VkImages.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Implementation of member function declared in VkImages.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "VkImages.h"
#include "VkInitializer.h"

namespace ephemeral
{
    namespace vkutils
    {
        void transitionImage(const VkCommandBuffer cmdBuffer, const VkImage img, const VkImageLayout oldLayout,
                             const VkImageLayout newLayout) noexcept
        {
            VkImageMemoryBarrier2 imageBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .image = img
            };

            const VkImageAspectFlags aspectFlag = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.subresourceRange = imageSubresourceRange(aspectFlag);

            const VkDependencyInfo depInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &imageBarrier
            };
            vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
        }

    }
}
