#pragma once

#include <vulkan/vulkan.h>

namespace vkutil
{
    void transition_image(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
}

