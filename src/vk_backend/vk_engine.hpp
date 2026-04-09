#pragma once

#include "vk_types.hpp"
#include "vk_descriptors.hpp"

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

        // Descriptors
        DescriptorAllocator globalDescriptorAllocator;

        VkDescriptorSet drawImageDescriptors;
        VkDescriptorSetLayout drawImageDescriptorLayout;

        // Pipelines
        VkPipeline gradientPipeline;
        VkPipelineLayout gradientPipelineLayout;

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
        void init_descriptors();

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();

        void draw_background(VkCommandBuffer commandBuffer);

        void init_pipelines();
        void init_background_pipelines();

};