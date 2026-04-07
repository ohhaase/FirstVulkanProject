#include "VulkanApplication.hpp"


vk::SurfaceFormatKHR VulkanApplication::chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
{
    // This whole function needs to be rewritten to not be C++ slop
    const auto formatIt = std::ranges::find_if(availableFormats,
                                                [](const auto& format) {
                                                    // Is this what I change if i need more precision?
                                                    return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; 
                                                });
    
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}


vk::PresentModeKHR VulkanApplication::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes)
{
    // Make sure at least eFifo is available
    assert(std::ranges::any_of(availablePresentModes, 
                                [](auto presentMode) {
                                    return presentMode == vk::PresentModeKHR::eFifo;
                                }));

    // If it's available, choose eMailBox. Otherwise, eFifo.
    return std::ranges::any_of(availablePresentModes,
                                [](const vk::PresentModeKHR value) {
                                    return value == vk::PresentModeKHR::eMailbox;
                                })
                                ? vk::PresentModeKHR::eMailbox
                                : vk::PresentModeKHR::eFifo;
    // AHHHHH C++ CODE IS SO UGLY AND HARD TO READ WHY WOULD YOU EVER WRITE IT LIKE THISSSSS
}


vk::Extent2D VulkanApplication::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}


uint32_t VulkanApplication::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
{
    uint32_t minImageCount = std::max(3u, surfaceCapabilities.minImageCount);

    if ((surfaceCapabilities.maxImageCount > 0) && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}


void VulkanApplication::createSwapChain()
{
    // Get surface capability info
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);

    swapChainExtent = chooseSwapExtent(surfaceCapabilities);
    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    // Get format info
    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);

    swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

    // Get present mode info
    std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

    // Create swapchain structure
    vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface = *surface,
                                                    .minImageCount = minImageCount,
                                                    .imageFormat = swapChainSurfaceFormat.format,
                                                    .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                                                    .imageExtent = swapChainExtent,
                                                    .imageArrayLayers = 1,
                                                    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                                                    .imageSharingMode = vk::SharingMode::eExclusive,
                                                    .preTransform = surfaceCapabilities.currentTransform,
                                                    .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                    .presentMode = presentMode,
                                                    .clipped = true};

    swapChainCreateInfo.oldSwapchain = nullptr; // Will eventually need to be changed once we deal with resizing

    swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();
}


void VulkanApplication::cleanupSwapChain()
{
    swapChainImageViews.clear();
    swapChain = nullptr;
}


void VulkanApplication::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    
    // Minimized
    while(width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
    
    device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createSwapchainImageViews();
}


void VulkanApplication::createSwapchainImageViews()
{
    assert(swapChainImageViews.empty());

    vk::ImageViewCreateInfo imageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
                                                .format = swapChainSurfaceFormat.format,
                                                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    
    for (auto& image : swapChainImages)
    {
        imageViewCreateInfo.image = image;
        swapChainImageViews.emplace_back(device, imageViewCreateInfo);
    }
}