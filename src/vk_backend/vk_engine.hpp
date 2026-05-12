#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "vk_types.hpp"
#include "vk_descriptors.hpp"

struct ComputePushConstants
{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeSim
{
    std::string name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;

    std::string shaderPath;
    

    // Descriptors
    DescriptorAllocator descAllocator;
    VkDescriptorSet descSet;
    VkDescriptorSetLayout descSetLayout;

    std::vector<DescImgInfo> descriptors;

    int pushConstSize;

    // Images
    std::vector<AllocatedImage> images;
};


struct FrameData
{
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;
    VkSemaphore swapchainSemaphore;
    VkFence renderFence;
    DeletionQueue deletionQueue;
};

constexpr int FRAME_OVERLAP = 2;

struct VulkanEngine
{
    public:
        bool isInitialized = false;
        int frameNumber = 0;
        bool stop_rendering = false;

        VkExtent2D windowExtent{WIDTH, HEIGHT};

        struct GLFWwindow* window = nullptr;

        static VulkanEngine& Get();

        // Vulkan instance variables
        VkInstance instance;
        VkDebugUtilsMessengerEXT debug_messenger;
        VkPhysicalDevice chosenGPU;
        VkDevice device;
        VkSurfaceKHR surface;

        // Swapchain variables
        VkSwapchainKHR swapchain;
        VkFormat swapChainImageFormat;

        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkExtent2D swapchainExtent;
        std::vector<VkSemaphore> renderSemaphores;

        // Frame data
        FrameData frames[FRAME_OVERLAP];

        FrameData& get_current_framedata() {return frames[frameNumber % FRAME_OVERLAP];};

        // Queue info
        VkQueue graphicsQueue;
        uint32_t graphicsQueueFamily;

        // Deletion queue for global objects
        DeletionQueue mainDeletionQueue;

        // Memory allocator
        VmaAllocator allocator;

        // Draw resources
        AllocatedImage drawImage;
        VkExtent2D drawExtent;
        DescImgInfo drawImageDescInfo;

        // Different compute shaders
        std::vector<ComputeSim> computeSims;
        int currentComputeSim = 0;

        // Immediate submit structures
        VkFence immFence;
        VkCommandBuffer immCommandBuffer;
        VkCommandPool immCommandPool;

        void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

        // Some test images
        AllocatedImage whiteImage;
        AllocatedImage blackImage;
        AllocatedImage greyImage;
        AllocatedImage errorCheckerboardImage;
        AllocatedImage nimbusImage;

        VkSampler defaultSamplerLinear;
        VkSampler defaultSamplerNearest;

        // Initializes everything
        void init();

        // Shuts down and deletes things
        void cleanup();

        // Draw loop
        void draw();

        // Main loop
        void run();

    private:
        void init_GLFW();
        void init_vulkan();
        void init_swapchain();
        void init_commands();
        void init_sync_structures();
        void init_compute_sims();
        void init_descriptors();
        void init_default_data();

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();

        void draw_background(VkCommandBuffer commandBuffer);

        void init_pipelines();
        void init_background_pipelines();

        void init_imgui();
        void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

        AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        void destroy_buffer(const AllocatedBuffer& buffer);

        AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
        AllocatedImage create_texture_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
        AllocatedImage create_texture_image_fromfile(std::string filePath);
        void destroy_image(const AllocatedImage& img);

        void addTextureToSim(ComputeSim& sim, std::string textureFile, int binding);
};