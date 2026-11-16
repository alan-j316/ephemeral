/**
 * @file VkDescriptors.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Implementation of member functions declared in VkDescriptors.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "VkDescriptors.h"


namespace ephemeral
{
    void DescriptorSetLayoutBuilder::addBinding(const uint32_t binding, const VkDescriptorType type) noexcept
    {
        const VkDescriptorSetLayoutBinding layoutBinding{ .binding         = binding,
                                                          .descriptorType  = type,
                                                          .descriptorCount = 1 };
        _bindings.push_back(layoutBinding);
    }


    void DescriptorSetLayoutBuilder::clear() noexcept { _bindings.clear(); }


    VkDescriptorSetLayout DescriptorSetLayoutBuilder::build(const VkDevice device,
                                                            const VkShaderStageFlags shaderStages,
                                                            const VkAllocationCallbacks* pAllocator, void* pNext,
                                                            const VkDescriptorSetLayoutCreateFlags flags) noexcept
    {
        // Add the shader stage flags to the binding
        for (auto& binding : _bindings)
        {
            binding.stageFlags |= shaderStages;
        }

        const VkDescriptorSetLayoutCreateInfo layoutInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                          .pNext = pNext,
                                                          .flags = flags,
                                                          .bindingCount = static_cast<uint32_t>(_bindings.size()),
                                                          .pBindings    = _bindings.data() };

        // Create the descriptor set layout.
        VkDescriptorSetLayout descriptorLayout;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, pAllocator, &descriptorLayout));
        return descriptorLayout;
    }


    void DescriptorAllocator::init(const VkDevice device, const VkAllocationCallbacks* pAllocator,
                                   const uint32_t maxSets, const std::span<PoolSizeRatio> poolRatios) noexcept
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.resize(poolRatios.size());
        for (size_t i = 0; i < poolRatios.size(); ++i)
        {
            const auto& [type, ratio]    = poolRatios[i];
            poolSizes[i].descriptorCount = static_cast<uint32_t>(ratio) * maxSets;
            poolSizes[i].type            = type;
        }

        VkDescriptorPoolCreateInfo poolCreateInfo{ .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                   .pNext         = nullptr,
                                                   .flags         = 0,
                                                   .maxSets       = maxSets,
                                                   .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                                   .pPoolSizes    = poolSizes.data() };
        VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, pAllocator, &pool));
    }


    VkDescriptorSet DescriptorAllocator::allocate(VkDevice device,
                                                  VkDescriptorSetLayout descriptorLayout) const noexcept
    {
        VkDescriptorSetAllocateInfo allocateInfo{ .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                  .pNext              = nullptr,
                                                  .descriptorPool     = pool,
                                                  .descriptorSetCount = 1,
                                                  .pSetLayouts        = &descriptorLayout };

        VkDescriptorSet descriptorSet;
        VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet));
        return descriptorSet;
    }


    void DescriptorAllocator::clearDescriptors(const VkDevice device) const noexcept
    { vkResetDescriptorPool(device, pool, 0); }


    void DescriptorAllocator::destroyPool(const VkDevice device, const VkAllocationCallbacks* pAllocator) const noexcept
    { vkDestroyDescriptorPool(device, pool, pAllocator); }



    /// +---------------------------------------------------------------------------------------+
    /// |                          RESIZABLE DESCRIPTOR ALLOCATOR                               |
    /// +---------------------------------------------------------------------------------------+

    void DescriptorAllocatorResizable::init(const VkDevice device, const VkAllocationCallbacks* pAllocator,
                                            const uint32_t maxSets, const std::span<PoolSizeRatio> poolRatios) noexcept
    {
        // Add the ratios to the class member
        _ratios.clear();
        for (const auto ratio : poolRatios)
        {
            _ratios.push_back(ratio);
        }

        const VkDescriptorPool pool = createPool(device, maxSets, poolRatios, pAllocator);
        _setsPerPool                = static_cast<uint32_t>(static_cast<float>(maxSets) * 1.5f);
        _readyPools.push_back(pool);
    }


    void DescriptorAllocatorResizable::clear(const VkDevice device) noexcept
    {
        for (const auto pool : _readyPools)
        {
            vkResetDescriptorPool(device, pool, 0);
        }

        for (const auto pool : _fullPools)
        {
            vkResetDescriptorPool(device, pool, 0);
            _readyPools.push_back(pool);
        }
        _fullPools.clear();
    }


    void DescriptorAllocatorResizable::destroy(const VkDevice device, const VkAllocationCallbacks* pAllocator) noexcept
    {
        for (const auto pool : _readyPools)
        {
            vkDestroyDescriptorPool(device, pool, pAllocator);
        }

        for (const auto pool : _fullPools)
        {
            vkDestroyDescriptorPool(device, pool, pAllocator);
        }

        _readyPools.clear();
        _fullPools.clear();
    }


    VkDescriptorSet DescriptorAllocatorResizable::allocate(const VkDevice device, VkDescriptorSetLayout layout,
                                                           const VkAllocationCallbacks* pAllocator, void* pNext)
    {
        VkDescriptorPool pool = getPool(device, pAllocator);

        VkDescriptorSetAllocateInfo allocateInfo{ .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                  .pNext              = pNext,
                                                  .descriptorPool     = pool,
                                                  .descriptorSetCount = 1,
                                                  .pSetLayouts        = &layout };

        VkDescriptorSet descriptorSet;
        VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet);

        // Allocation failed due to low memory or fragmentation; try again.
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
        {
            _fullPools.push_back(pool);
            pool                        = getPool(device, pAllocator);
            allocateInfo.descriptorPool = pool;

            VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet));
        }
        // Add the pool back to ready pool
        _readyPools.push_back(pool);
        return descriptorSet;
    }


    VkDescriptorPool DescriptorAllocatorResizable::getPool(const VkDevice device,
                                                           const VkAllocationCallbacks* pAllocator) noexcept
    {
        VkDescriptorPool newPool;
        // Try to allocate from ready pools if they are not empty
        if (_readyPools.size() != 0)
        {
            newPool = _readyPools.back();
            _readyPools.pop_back();
        }
        // If the ready pool is empty then, allocate a new pool
        else
        {
            newPool      = createPool(device, _setsPerPool, _ratios, pAllocator);
            _setsPerPool = static_cast<uint32_t>(_setsPerPool * 1.5);
            if (_setsPerPool > 4092)
            {
                _setsPerPool = 4092;
            }
        }
        return newPool;
    }


    VkDescriptorPool DescriptorAllocatorResizable::createPool(const VkDevice device, const uint32_t setCount,
                                                              std::span<PoolSizeRatio> poolRatios,
                                                              const VkAllocationCallbacks* pAllocator) noexcept
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        for (const auto& [type, ratio] : poolRatios)
        {
            poolSizes.emplace_back(VkDescriptorPoolSize{
                .type = type, .descriptorCount = static_cast<uint32_t>(ratio * static_cast<float>(setCount)) });
        }

        VkDescriptorPoolCreateInfo poolInfo{ .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                             .pNext         = nullptr,
                                             .flags         = 0,
                                             .maxSets       = setCount,
                                             .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                             .pPoolSizes    = poolSizes.data() };

        VkDescriptorPool newPool;
        VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, pAllocator, &newPool));
        return newPool;
    }


} // namespace ephemeral
