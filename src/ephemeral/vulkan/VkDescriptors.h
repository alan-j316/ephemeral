#pragma once
/**
 * @file VkDescriptors.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Vulkan Descriptor abstraction.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */
#include "VkTypes.h"


namespace ephemeral
{

    struct DescriptorSetLayoutBuilder
    {
        void addBinding(uint32_t binding, VkDescriptorType type) noexcept;

        void clear() noexcept;

        VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages,
                                    const VkAllocationCallbacks* pAllocator, void* pNext = nullptr,
                                    VkDescriptorSetLayoutCreateFlags flags = 0) noexcept;

    private:
        std::vector<VkDescriptorSetLayoutBinding> _bindings;
    };

    struct DescriptorAllocator
    {
        struct PoolSizeRatio
        {
            VkDescriptorType type;
            float ratio;
        };

        VkDescriptorPool pool;

        /// Initialize the descriptor pool.
        void init(VkDevice device, const VkAllocationCallbacks* pAllocator, uint32_t maxSets,
                  std::span<PoolSizeRatio> poolRatios) noexcept;

        /// Allocate a descriptor set with @p descriptorLayout from the pool.
        VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout descriptorLayout) const noexcept;

        /// Reset the descriptor pool.
        /// @note This will not destroy the descriptor pool and only clear the allocator descriptors.
        void clearDescriptors(VkDevice device) const noexcept;

        /// Destroy the descriptor pool.
        void destroyPool(VkDevice device, const VkAllocationCallbacks* pAllocator) const noexcept;
    };
} // namespace ephemeral
