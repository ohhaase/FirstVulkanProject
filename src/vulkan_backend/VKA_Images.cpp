#include "VulkanApplication.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

void VulkanApplication::transition_image_layout(uint32_t imageIndex,
                                                vk::ImageLayout oldLayout,
                                                vk::ImageLayout newLayout,
                                                vk::AccessFlags2 srcAccessMask,
                                                vk::AccessFlags2 dstAccessMask,
                                                vk::PipelineStageFlags2 srcStageMask,
                                                vk::PipelineStageFlags2 dstStageMask)
{
    vk::ImageMemoryBarrier2 barrier = {.srcStageMask = srcStageMask,
                                        .srcAccessMask = srcAccessMask,
                                        .dstStageMask = dstStageMask,
                                        .dstAccessMask = dstAccessMask,
                                        .oldLayout = oldLayout,
                                        .newLayout = newLayout,
                                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                        .image = swapChainImages[imageIndex],
                                        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                            .baseMipLevel = 0,
                                                            .levelCount = 1,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1}};
                                                
    vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                            .imageMemoryBarrierCount = 1,
                                            .pImageMemoryBarriers = &barrier};

    commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
}


void VulkanApplication::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
    vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
                                    .format = format,
                                    .extent = {width, height, 1},
                                    .mipLevels = 1,
                                    .arrayLayers = 1,
                                    .samples = vk::SampleCountFlagBits::e1,
                                    .tiling = tiling,
                                    .usage = usage,
                                    .sharingMode = vk::SharingMode::eExclusive};

    image = vk::raii::Image(device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size,
                                        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};

    imageMemory = vk::raii::DeviceMemory(device, allocInfo);
    image.bindMemory(imageMemory, 0);
}


void VulkanApplication::transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{.oldLayout = oldLayout,
                                    .newLayout = newLayout,
                                    .image = image,
                                    .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);

    endSingleTimeCommands(commandBuffer);
}


vk::raii::ImageView VulkanApplication::createImageView(vk::raii::Image& image, vk::Format format)
{
    vk::ImageViewCreateInfo imageViewCreateInfo{.image = image,
                                                .viewType = vk::ImageViewType::e2D,
                                                .format = format,
                                                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    
    return vk::raii::ImageView(device, imageViewCreateInfo);
}


void VulkanApplication::createTextureImage()
{
    // Read image
    int texWidth, texHeight, texChannels;

    stbi_uc* pixels = stbi_load((std::string(TEXTURE_DIR) + "texture.JPEG").data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    vk::DeviceSize imageSize = texWidth * texHeight * 4; //4 because rgba?

    if (!pixels)
    {
        throw std::runtime_error("Failed to load texture image!");
    }

    // Staging buffer
    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});

    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    // Create image
    createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, 
                vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);

    // Copy staging buffer to texture image
    transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    // Prepare for shader access
    transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

}


void VulkanApplication::createTextureSampler()
{
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo{.magFilter = vk::Filter::eLinear,
                                        .minFilter = vk::Filter::eLinear,
                                        .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                        .addressModeU = vk::SamplerAddressMode::eRepeat,
                                        .addressModeV = vk::SamplerAddressMode::eRepeat,
                                        .addressModeW = vk::SamplerAddressMode::eRepeat,
                                        .mipLodBias = 0.0f,
                                        .anisotropyEnable = vk::True,
                                        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                                        .compareEnable = vk::False,
                                        .compareOp = vk::CompareOp::eAlways,
                                        .minLod = 0.0f,
                                        .maxLod = 0.0f,
                                        .borderColor = vk::BorderColor::eIntOpaqueBlack,
                                        .unnormalizedCoordinates = vk::False};

    textureSampler = vk::raii::Sampler(device, samplerInfo);
}


void VulkanApplication::createTextureImageView()
{
    textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb);
}