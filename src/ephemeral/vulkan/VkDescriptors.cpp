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


} // namespace ephemeral
