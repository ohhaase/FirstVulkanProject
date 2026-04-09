#include "vk_engine.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#define VMA_IMPLEMENTATION
#include <VMA/vk_mem_alloc.h>

#include <chrono>
#include <thread>

#include "vk_initializers.hpp"
#include "vk_types.hpp"
#include "vk_images.hpp"
#include "vk_pipelines.hpp"

// Enable validation layers when building in DEBUG
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}


void VulkanEngine::init()
{
    // Only one engine per application
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    init_GLFW();

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_pipelines();

    // If everything went fine...
    isInitialized = true;
}


void VulkanEngine::cleanup()
{
    if (isInitialized)
    {
        // Make sure gpu has stopped doing its things
        vkDeviceWaitIdle(device);

        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            vkDestroyCommandPool(device, frames[i].commandPool, nullptr);

            // Destroy sync objects
            vkDestroyFence(device, frames[i].renderFence, nullptr);
            vkDestroySemaphore(device, frames[i].swapchainSemaphore, nullptr);
            
            frames[i].deletionQueue.flush();
        }
        
        for (int i = 0; i < swapchainImages.size(); i++)
        {
            vkDestroySemaphore(device, renderSemaphores[i], nullptr);
        }

        // Flush global deletion queue
        mainDeletionQueue.flush();

        destroy_swapchain();

        // Destroy vulkan objects
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyDevice(device, nullptr);

        vkb::destroy_debug_utils_messenger(instance, debug_messenger);
        vkDestroyInstance(instance, nullptr);

        // Destroy window
        glfwDestroyWindow(window);
    }

    // Clear engine pointer
    loadedEngine = nullptr;
}


void VulkanEngine::draw()
{
    // Wait until gpu has finished rendering last frame
    VK_CHECK(vkWaitForFences(device, 1, &get_current_framedata().renderFence, true, 1000000000));
    
    // Delete frame objects after previous gpu call is finished
    get_current_framedata().deletionQueue.flush();
    
    // Get image from swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1000000000, get_current_framedata().swapchainSemaphore, nullptr, &swapchainImageIndex));
    
    VK_CHECK(vkResetFences(device, 1, &get_current_framedata().renderFence)); // Reset fences after

    // Get current command buffer and reset
    VkCommandBuffer commandBuffer = get_current_framedata().mainCommandBuffer;

    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    // Ensure drawExtent is right size
    drawExtent.width = drawImage.imageExtent.width;
    drawExtent.height = drawImage.imageExtent.height;

    // Begin command buffer recording
    VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    // Make drawImage writable (we don't care about overwriting)
    vkutil::transition_image(commandBuffer, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(commandBuffer);

    // transition drawImage and swapchain image into their correct transfer layouts
    vkutil::transition_image(commandBuffer, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(commandBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy drawImage into swapchain image
    vkutil::copy_image_to_image(commandBuffer, drawImage.image, swapchainImages[swapchainImageIndex], drawExtent, swapchainExtent);

    // transition swapchain back to presentable
    vkutil::transition_image(commandBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Finalize command buffer
    VK_CHECK(vkEndCommandBuffer(commandBuffer));


    // Prepare submission to the queue
    // Wait on presentSemaphore, signal renderSemaphore when done
    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(commandBuffer);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_framedata().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, renderSemaphores[swapchainImageIndex]);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

    // Submit to queue
    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, get_current_framedata().renderFence));

    // Prepare presentation
    // Needs to wait on renderSemaphore to ensure drawing is done
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &renderSemaphores[swapchainImageIndex];
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));
}


void VulkanEngine::run()
{
    double lastTime = 0;
    double deltaTime = 0;

    // Loop until told to close
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        double currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        if ((frameNumber % 500) == 0)
        {
            std::cout << "FPS: " << 1000 / deltaTime << "\n";
        }

        draw();

        frameNumber++;
    }
}


void VulkanEngine::init_GLFW()
{
    // Initialize GLFW and create window
    glfwInit();

    // Tell GLFW to not use OpenGL
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // To be fixed soon

    // Create window
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    // glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}


void VulkanEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

    // Make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("MyVulkanApp")
        .request_validation_layers(enableValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();
    
    vkb::Instance vkb_inst = inst_ret.value();

    // Grab instance
    instance = vkb_inst.instance;
    debug_messenger = vkb_inst.debug_messenger;

    // Surface
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != 0)
    {
        throw std::runtime_error("Failed to create window surface!");
    }

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // Select gpu with vk bootstrap
    // Must be able to write to surface and support vulkan 1.3 features
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(surface)
        .select()
        .value();

    // Create final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get handles for use in rest of application
    device = vkbDevice.device;
    chosenGPU = physicalDevice.physical_device;

    // Get graphics queue with vkbootstrap (might change here if wanting to get compute specific queues?)
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // Initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = chosenGPU;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&allocatorInfo, &allocator);

    mainDeletionQueue.push_function([&](){vmaDestroyAllocator(allocator);});
}


void VulkanEngine::init_swapchain()
{
    create_swapchain(windowExtent.width, windowExtent.height);

    // drawImage size will match window
    VkExtent3D drawImageExtent{windowExtent.width, windowExtent.height, 1};

    drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    // drawImage.imageFormat = VK_FORMAT_R64G64B64A64_SFLOAT; // For later (actually maybe not here, because the drawImage is just for drawing)
    drawImage.imageExtent = drawImageExtent;

    // This feels like it should be more specialized for more performance 
    // (actually maybe not here, because the drawImage is just for drawing)
    VkImageUsageFlags drawImageUsages{}; 
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // copy source
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; // copy destination
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT; // compute shader can write
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // graphics pipeline can write

    VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent);

    // Allocate draw image from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // never accessable from cpu (since drawImage)
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create image
    vmaCreateImage(allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);

    // Build image view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &drawImage.imageView));

    // Add to deletion queues
    mainDeletionQueue.push_function([this](){
        vkDestroyImageView(device, drawImage.imageView, nullptr);
        vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
    });
}


void VulkanEngine::init_commands()
{
    // Create command pool for commands submitted to graphics queue
    // Pool will allow for resetting of indiviudal command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool));

        // Allocate default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].mainCommandBuffer));
    }
    
}


void VulkanEngine::init_sync_structures()
{
    // Create 1 fence to signal gpu rendering finish
    // Create 2 semaphores to syncronize swapchain

    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT); // Starts signaled to wait for 1st frame
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
    }

    for (int i = 0; i < swapchainImages.size(); i++)
    {
        renderSemaphores.push_back({});
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSemaphores[i]));
    }
}


void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{chosenGPU, device, surface};

    swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM; // Hardcoded

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{.format = swapChainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR) // Vsync present mode (need to make sure this is avail?)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchainExtent = vkbSwapchain.extent;

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}


void VulkanEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    // Destroy swapchain resources
    for (int i = 0; i < swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
}


void VulkanEngine::draw_background(VkCommandBuffer commandBuffer)
{
    // // Make a clear color from frame number (temporary)
    // VkClearColorValue clearValue;
    // float flash = std::abs(std::sin(frameNumber / 120.f));
    // clearValue = {{0.0f, 0.0f, flash, 1.0f}};

    // VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // //clear image
    // vkCmdClearColorImage(commandBuffer, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    //Bind gradient drawing compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout, 0, 1, &drawImageDescriptors, 0, nullptr);

    // Dispatch compute shader
    vkCmdDispatch(commandBuffer, std::ceil(drawExtent.width / 16.0), std::ceil(drawExtent.height / 16.0), 1);
}


void VulkanEngine::init_descriptors()
{
    // Create a descriptor pool to hold 10 (?) sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

    globalDescriptorAllocator.init_pool(device, 10, sizes);

    // Make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // Allocate a descriptor set for out draw image
    drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = drawImage.imageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;

    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;
    
    vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);

    // Make sure both the descriptor and allocator and the new layout get cleaned up
    mainDeletionQueue.push_function([&](){
        globalDescriptorAllocator.destroy_pool(device);
        vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
    });
}


void VulkanEngine::init_pipelines()
{
    init_background_pipelines();
}


void VulkanEngine::init_background_pipelines()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;

    computeLayout.pSetLayouts = &drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &gradientPipelineLayout));

    VkShaderModule computeDrawShader;
    if (!vkutil::load_shader_module(std::string(SHADER_DIR) + "gradient.comp.spv", device, &computeDrawShader))
    {
        throw std::runtime_error("Error while building compute shader!");
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;

    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeDrawShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;

    computePipelineCreateInfo.layout = gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientPipeline));

    vkDestroyShaderModule(device, computeDrawShader, nullptr);
    mainDeletionQueue.push_function([&](){
        vkDestroyPipelineLayout(device, gradientPipelineLayout, nullptr);
        vkDestroyPipeline(device, gradientPipeline, nullptr);
    });
}