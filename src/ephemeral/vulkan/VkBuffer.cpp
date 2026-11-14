/**
 * @file VkBuffer.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 14, 2026
 *
 * @brief Implementation of functions declared in VkBuffer.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */


#include "VkBuffer.h"

#include "VkTypes.h"


namespace ephemeral
{
    AllocatedBuffer createBuffer(const size_t size, const VmaAllocator allocator, const VkBufferUsageFlags usageFlags,
                                 const VmaMemoryUsage memoryUsage) noexcept
    {
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size  = size,
            .usage = usageFlags,
        };

        const VmaAllocationCreateInfo vmaInfo{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT, // Maps the memory automatically
            .usage = memoryUsage,
        };

        AllocatedBuffer buffer;
        VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaInfo, &buffer.buffer, &buffer.allocation,
                                 &buffer.allocationInfo));
        return buffer;
    }


    void destroyBuffer(const VmaAllocator allocator, const AllocatedBuffer& buffer) noexcept
    { vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation); }
} // namespace ephemeral
