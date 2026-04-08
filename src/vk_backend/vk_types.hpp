#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <stdexcept>
#include <iostream>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <VMA/vk_mem_alloc.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "globals.hpp"

#define VK_CHECK(x) do {VkResult err = x; if (err != VK_SUCCESS) {throw std::runtime_error(std::string("Detected Vulkan Error: {}") + string_VkResult(err));}} while (0)