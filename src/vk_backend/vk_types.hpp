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
#include <glm/packing.hpp>

#include "globals.hpp"

#define VK_CHECK(x) do {VkResult err = x; if (err != VK_SUCCESS) {throw std::runtime_error(std::string("Detected Vulkan Error: {}") + string_VkResult(err));}} while (0)


struct AllocatedImage
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};


struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};


struct DescImgInfo
{
    int binding;
    VkImageView imageView;
    VkSampler sampler;
    VkImageLayout layout;
    VkDescriptorType type;
};


struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto func = deletors.rbegin(); func != deletors.rend(); func++)
        {
            (*func)();
        }

        deletors.clear();
    }
};