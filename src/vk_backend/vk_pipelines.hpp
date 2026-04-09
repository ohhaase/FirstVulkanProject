#pragma once

#include "vk_types.hpp"

namespace vkutil
{
    bool load_shader_module(std::string filePath, VkDevice device, VkShaderModule* outShaderModule);
}