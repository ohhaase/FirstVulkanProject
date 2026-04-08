#pragma once

#include "vk_types.hpp"

struct FrameData
{
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;
    VkSemaphore swapchainSemaphore, renderSemaphore;
    VkFence renderFence;
};

constexpr int FRAME_OVERLAP = 2;

class VulkanEngine
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

        // Frame data
        FrameData frames[FRAME_OVERLAP];

        FrameData& get_current_framedata() {return frames[frameNumber % FRAME_OVERLAP];};

        // Queue info
        VkQueue graphicsQueue;
        uint32_t graphicsQueueFamily;

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

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();
};