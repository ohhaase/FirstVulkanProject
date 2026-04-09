#include "vk_pipelines.hpp"

#include <fstream>
#include "vk_types.hpp"


bool vkutil::load_shader_module(std::string filePath, VkDevice device, VkShaderModule* outShaderModule)
{
    // open file with cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    // Find what the size of the file is by looking at location of cursor
    size_t fileSize = (size_t)file.tellg();

    // Reserve uint32 vector to store the file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // Put cursor at start and load entire file
    file.seekg(0);

    file.read((char*)buffer.data(), fileSize);

    file.close();

    // Create a new shader module
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return false;
    }

    *outShaderModule = shaderModule;
    return true;
}