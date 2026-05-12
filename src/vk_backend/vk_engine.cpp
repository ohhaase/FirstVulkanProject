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

    init_default_data();

    init_descriptors();

    init_pipelines();

    init_imgui();

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

    // Set swapchain image layout to attachment optimal so we can draw it
    vkutil::transition_image(commandBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Draw imgui into the swapchain image
    draw_imgui(commandBuffer, swapchainImageViews[swapchainImageIndex]);

    // transition swapchain back to presentable
    vkutil::transition_image(commandBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
        // Poll events
        glfwPollEvents();

        // Imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("background"))
        {
            ComputeSim& selected = computeSims[currentComputeSim];

            ImGui::Text("Selected effect: ", selected.name);

            ImGui::SliderInt("Effect Index", &currentComputeSim, 0, computeSims.size() - 1);
            
            ImGui::InputFloat4("data1", (float*)& selected.data.data1);
            ImGui::InputFloat4("data2", (float*)& selected.data.data2);
            ImGui::InputFloat4("data3", (float*)& selected.data.data3);
            ImGui::InputFloat4("data4", (float*)& selected.data.data4);
        }
        ImGui::End();

        // Make imgui calculate internal draw structures
        ImGui::Render();
        
        // Draw
        draw();
        
        // Frame number/fps stuff
        double currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        if ((frameNumber % 500) == 0)
        {
            std::cout << "FPS: " << 1000 / deltaTime << "\n";
        }

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

    mainDeletionQueue.push_function([=, this](){vmaDestroyAllocator(allocator);});
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
    
    // Immediate submit commands
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immCommandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &immCommandBuffer));

    // Add immediate command pool to deletion queue
    mainDeletionQueue.push_function([=, this](){
        vkDestroyCommandPool(device, immCommandPool, nullptr);
    });
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

    // Immediate commands fence
    VK_CHECK((vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence)));
    mainDeletionQueue.push_function([=, this]() {vkDestroyFence(device, immFence, nullptr);});
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


void VulkanEngine::init_compute_sims()
{

}


void VulkanEngine::draw_background(VkCommandBuffer commandBuffer)
{
    // Get our current shader
    ComputeSim& thisSim = computeSims[currentComputeSim];

    //Bind the background compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, thisSim.pipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, thisSim.layout, 0, 1, &globalDescriptorSet, 0, nullptr);

    // Push constants
    vkCmdPushConstants(commandBuffer, thisSim.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &thisSim.data);

    // Dispatch compute shader
    vkCmdDispatch(commandBuffer, std::ceil(drawExtent.width / 16.0), std::ceil(drawExtent.height / 16.0), 1);
}


void VulkanEngine::init_descriptors()
{
    // TODO: MAKE THE LOOPS IN THIS FUNC BETTER

    // Create a descriptor pool to hold 1 set (maybe more, for each sim?) with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}}; // Draw image by default

    ComputeSim thisComputeSim = computeSims[0];

    for (int i = 0; i < thisComputeSim.descriptors.size(); i++)
    {
        DescriptorAllocator::PoolSizeRatio size = {thisComputeSim.descriptors[i].type, 1};
        sizes.push_back(size);
    }

    globalDescriptorAllocator.init_pool(device, 1, sizes);

    // Make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Draw image by default

        // Loop over descriptors in the sim
        for (int i = 0; i < thisComputeSim.descriptors.size(); i++)
        {
            ComputeSim::descInfo thisDescriptor = thisComputeSim.descriptors[i];
            builder.add_binding(thisDescriptor.binding, thisDescriptor.type);
        }

        globalDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // Allocate a descriptor set for our draw image
    globalDescriptorSet = globalDescriptorAllocator.allocate(device, globalDescriptorLayout);

    {
        DescriptorWriter writer;
        writer.write_image(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Draw image by default

        // Other descriptors
        for (int i = 0; i < thisComputeSim.descriptors.size(); i++)
        {
            ComputeSim::descInfo thisDescriptor = thisComputeSim.descriptors[i];
            writer.write_image(thisDescriptor.binding, thisDescriptor.imageView, thisDescriptor.sampler, thisDescriptor.layout, thisDescriptor.type);
        }

        writer.update_set(device, globalDescriptorSet);
    }

    // Make sure both the descriptor and allocator and the new layout get cleaned up
    mainDeletionQueue.push_function([=, this](){
        globalDescriptorAllocator.destroy_pool(device);
        vkDestroyDescriptorSetLayout(device, globalDescriptorLayout, nullptr);
    });
}


void VulkanEngine::init_pipelines()
{
    // init_background_pipelines();

    for (ComputeSim& thisSim: computeSims)
    {
        // Push constant info
        VkPushConstantRange pushConstant{};
        pushConstant.offset = 0;
        pushConstant.size = thisSim.pushConstSize;
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // Compute pipeline layout info
        VkPipelineLayoutCreateInfo computeLayout{};
        computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        computeLayout.pNext = nullptr;

        computeLayout.pSetLayouts = &globalDescriptorLayout; // Maybe make descriptors on a per-sim basis?
        computeLayout.setLayoutCount = 1;

        computeLayout.pPushConstantRanges = &pushConstant;
        computeLayout.pushConstantRangeCount = 1;

        // Create pipeline layout
        VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &thisSim.layout));

        // Get shader
        VkShaderModule shader;
        if (!vkutil::load_shader_module(std::string(SHADER_DIR) + thisSim.shaderPath + ".spv", device, &shader))
        {
            throw std::runtime_error("Error while loading shader for " + thisSim.name);
        }

        // Create stageInfo and pipelineInfo
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.pNext = nullptr;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = shader;
        stageInfo.pName = "main";
        
        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;    
        computePipelineCreateInfo.layout = thisSim.layout;
        computePipelineCreateInfo.stage = stageInfo;

        VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &thisSim.pipeline));

        // Destroy all structures
        vkDestroyShaderModule(device, shader, nullptr);

        mainDeletionQueue.push_function([=, this](){
            vkDestroyPipelineLayout(device, thisSim.layout, nullptr);
            vkDestroyPipeline(device, thisSim.pipeline, nullptr);
        });
    }
}


// void VulkanEngine::init_background_pipelines()
// {
//     // Push constant info
//     VkPushConstantRange pushConstant{};
//     pushConstant.offset = 0;
//     pushConstant.size = sizeof(ComputePushConstants);
//     pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

//     // Compute layout info
//     VkPipelineLayoutCreateInfo computeLayout{};
//     computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
//     computeLayout.pNext = nullptr;

//     computeLayout.pSetLayouts = &globalDescriptorLayout;
//     computeLayout.setLayoutCount = 1;

//     computeLayout.pPushConstantRanges = &pushConstant;
//     computeLayout.pushConstantRangeCount = 1;

//     VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &gradientPipelineLayout));

//     // Get both shaders
//     VkShaderModule gradientShader;
//     if (!vkutil::load_shader_module(std::string(SHADER_DIR) + "gradient_color.comp.spv", device, &gradientShader))
//     {
//         throw std::runtime_error("Error while building compute shader!");
//     }

//     VkShaderModule skyShader;
//     if (!vkutil::load_shader_module(std::string(SHADER_DIR) + "sky.comp.spv", device, &skyShader))
//     {
//         throw std::runtime_error("Error while building sky shader!");
//     }

//     // Create common stageInfo and pipeline info
//     VkPipelineShaderStageCreateInfo stageInfo{};
//     stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//     stageInfo.pNext = nullptr;
//     stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    
//     VkComputePipelineCreateInfo computePipelineCreateInfo{};
//     computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//     computePipelineCreateInfo.pNext = nullptr;    
//     computePipelineCreateInfo.layout = gradientPipelineLayout;

//     // Gradient shader + default colors
//     stageInfo.module = gradientShader;
//     stageInfo.pName = "main";
//     computePipelineCreateInfo.stage = stageInfo;

//     ComputeEffect gradient;
//     gradient.layout = gradientPipelineLayout;
//     gradient.name = "gradient";
//     gradient.data = {};

//     gradient.data.data1 = glm::vec4(1, 0, 0, 1);
//     gradient.data.data2 = glm::vec4(0, 0, 1, 1);

//     VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

//     backgroundEffects.push_back(gradient);

//     // Sky shader + default params
//     stageInfo.module = skyShader;
//     stageInfo.pName = "main";
//     computePipelineCreateInfo.stage = stageInfo;

//     ComputeEffect sky;
//     sky.layout = gradientPipelineLayout;
//     sky.name = "sky";
//     sky.data = {};

//     sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

//     VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

//     backgroundEffects.push_back(sky);
    
//     // Destroy all structures
//     vkDestroyShaderModule(device, gradientShader, nullptr);
//     vkDestroyShaderModule(device, skyShader, nullptr);

//     mainDeletionQueue.push_function([=, this](){
//         vkDestroyPipelineLayout(device, gradientPipelineLayout, nullptr);
//         vkDestroyPipeline(device, sky.pipeline, nullptr);
//         vkDestroyPipeline(device, gradient.pipeline, nullptr);
//     });
// }


void VulkanEngine::init_imgui()
{
    // 1: create descriptor pool for IMGUI
    //   Size of the pool is oversize (?) but copied from imgui demo

    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		                                 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		                                 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		                                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		                                 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		                                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		                                 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // initialize core structures of imgui
    ImGui::CreateContext();

    // initialize imgui for GLFW
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // initialize imgui for vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = chosenGPU;
    init_info.Device = device;
    init_info.Queue = graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // dynamics rendering parameters for imgui to use
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainImageFormat;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    // Add the destroy to the deletion queue
    mainDeletionQueue.push_function([=, this](){
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
    });
}


void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}


void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    // Reset fence and buffer
    VK_CHECK(vkResetFences(device, 1, &immFence));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

    VkCommandBuffer cmd = immCommandBuffer;

    // Begin command buffer
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // Run function
    function(cmd);

    // End buffer
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit, execute, and wait for it to finish
    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, immFence));

    VK_CHECK(vkWaitForFences(device, 1, &immFence, true, 9999999999));
}


AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // Allocate buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;

    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}


void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}


AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped /*= false*/)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped)
    {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // Always allocate images on dedicated gpu memory
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create image
    VK_CHECK(vmaCreateImage(allocator, &img_info, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

    // If the format is a depth format (??) we need to use correct aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // Build an image view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}


AllocatedImage VulkanEngine::create_texture_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped /*= false*/)
{
    size_t data_size = size.depth * size.width * size.height * 4; // 4 for the RGBA
    AllocatedBuffer uploadBuffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadBuffer.info.pMappedData, data, data_size);

    AllocatedImage newImage = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);


    // Immediate submit the data transfer
    immediate_submit([&](VkCommandBuffer cmd){
        vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // Copy buffer to image
        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    destroy_buffer(uploadBuffer);

    return newImage;
}


void VulkanEngine::destroy_image(const AllocatedImage& img)
{
    vkDestroyImageView(device, img.imageView, nullptr);
    vmaDestroyImage(allocator, img.image, img.allocation);
}


void VulkanEngine::init_default_data()
{
    // Default solid textures, 1 pixel each
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    whiteImage = create_texture_image((void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    greyImage = create_texture_image((void*)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
    blackImage = create_texture_image((void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // Default checkerboard image
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16*16> pixels;
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    errorCheckerboardImage = create_texture_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // Create samplers
    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    // Nearest
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(device, &sampl, nullptr, &defaultSamplerNearest);

    // Linear
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device, &sampl, nullptr, &defaultSamplerLinear);

    // Add things to deletion queue
    mainDeletionQueue.push_function([&](){
        vkDestroySampler(device, defaultSamplerNearest, nullptr);
        vkDestroySampler(device, defaultSamplerLinear, nullptr);

        destroy_image(whiteImage);
        destroy_image(greyImage);
        destroy_image(blackImage);
        destroy_image(errorCheckerboardImage);
    });

    // Make compute sim
    ComputeSim newSim;

    newSim.name = "FirstSim";
    newSim.shaderPath = "gradient_color.comp";

    // Default data
    newSim.pushConstSize = sizeof(ComputePushConstants);
    newSim.data.data1 = glm::vec4(1, 0, 0, 1);
    newSim.data.data2 = glm::vec4(0, 0, 1, 1);

    // Draw image included in all sims by default?

    // Checkerboard image
    ComputeSim::descInfo testImage;
    testImage.binding = 1;
    testImage.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    testImage.imageView = errorCheckerboardImage.imageView;
    testImage.sampler = defaultSamplerNearest;
    testImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


    newSim.descriptors.push_back(testImage);

    computeSims.push_back(newSim);


    // Sky shader sim
    ComputeSim skySim;

    skySim.name = "SkySim";
    skySim.shaderPath = "sky.comp";

    // Default data
    skySim.pushConstSize = sizeof(ComputePushConstants);
    skySim.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    // Shouldn't need another image?
    
    computeSims.push_back(skySim);
}