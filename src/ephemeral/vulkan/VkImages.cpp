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


namespace ephemeral::vkutils
{
    void transitionImage(const VkCommandBuffer cmdBuffer, const VkImage img, const VkImageLayout oldLayout,
                         const VkImageLayout newLayout) noexcept
    {
        VkImageMemoryBarrier2 imageBarrier{ .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                            .pNext         = nullptr,
                                            .srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                                            .dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                                            .oldLayout     = oldLayout,
                                            .newLayout     = newLayout,
                                            .image         = img };

        const VkImageAspectFlags aspectFlag = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange       = imageSubresourceRange(aspectFlag);

        const VkDependencyInfo depInfo{ .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                        .pNext                   = nullptr,
                                        .imageMemoryBarrierCount = 1,
                                        .pImageMemoryBarriers    = &imageBarrier };
        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }


    void copyImageToImage(const VkCommandBuffer cmdBuffer, const VkImage src, const VkImage dst,
                          const VkExtent2D srcSize, const VkExtent2D dstSize) noexcept
    {
        VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };
        // NOTE: srcOffsets[0] and dstOffsets[0] is the top-left corner offset which is 0 in our case
        //       and can be left uninitialized
        blitRegion.srcOffsets[1].x = static_cast<int>(srcSize.width);
        blitRegion.srcOffsets[1].y = static_cast<int>(srcSize.height);
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = static_cast<int>(dstSize.width);
        blitRegion.dstOffsets[1].y = static_cast<int>(dstSize.height);
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount     = 1;
        blitRegion.srcSubresource.mipLevel       = 0;

        blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount     = 1;
        blitRegion.dstSubresource.mipLevel       = 0;

        const VkBlitImageInfo2 blitInfo{ .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                                         .pNext          = nullptr,
                                         .srcImage       = src,
                                         .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         .dstImage       = dst,
                                         .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         .regionCount    = 1,
                                         .pRegions       = &blitRegion };

        vkCmdBlitImage2(cmdBuffer, &blitInfo);
    }

} // namespace ephemeral::vkutils
