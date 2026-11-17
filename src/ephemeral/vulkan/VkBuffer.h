#pragma once
/**
 * @file VkBuffer.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 14, 2026
 *
 * @brief Vulkan Buffer abstraction.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include <vk_mem_alloc.h>

namespace ephemeral
{
    struct AllocatedBuffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
    };

    /// Allocate a vulkan buffer with VMA.
    AllocatedBuffer createBuffer(size_t size, VmaAllocator allocator, VkBufferUsageFlags usageFlags,
                                 VmaMemoryUsage memoryUsage) noexcept;

    /// Map the allocation and return a pointer to the underlying memory.
    void* mapBuffer(VmaAllocator allocator, const AllocatedBuffer& buffer) noexcept;

    /// Unmap the buffer.
    void unmapBuffer(VmaAllocator allocator, const AllocatedBuffer& buffer) noexcept;

    /// Free a VMA allocated vulkan buffer.
    void destroyBuffer(VmaAllocator allocator, const AllocatedBuffer& buffer) noexcept;

} // namespace ephemeral
