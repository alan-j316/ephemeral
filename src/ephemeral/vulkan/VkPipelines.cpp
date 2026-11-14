/**
 * @file VkPipelines.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 14, 2026
 *
 * @brief Implementation of functions declared in VkPipelines.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "VkPipelines.h"

#include <fstream>


namespace ephemeral
{
    bool vkutils::loadShaderModule(const char* filePath, const VkDevice device, const VkAllocationCallbacks* pAllocator,
                                   VkShaderModule* outShaderModule) noexcept
    {
        // ------------------------ FILE READ ------------------------
        // Open the file with the cursor at the end to read the size.
        std::ifstream file(filePath, std::ios::ate | std::ios::binary); // NOLINT

        if (!file.is_open())
        {
            return false;
        }

        const size_t fileSize = static_cast<size_t>(file.tellg());

        // SPIRV expect a uin32_t* so reserve a vector large enough for the entire file.
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        file.seekg(0); // Move the cursor to the start.
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
        file.close();

        // ------------------------ SHADER MODULE CREATION ------------------------
        const VkShaderModuleCreateInfo shaderCreateInfo{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode    = buffer.data(),
        };

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &shaderCreateInfo, pAllocator, &shaderModule) != VK_SUCCESS)
        {
            return false;
        }
        *outShaderModule = shaderModule;
        return true;
    }


} // namespace ephemeral
