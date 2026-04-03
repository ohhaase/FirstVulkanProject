#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Enable validation layers when building in DEBUG
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class VulkanApplication
{
    public:
        void run();


    private:
        // GLFW Window
        GLFWwindow* window;

        // Vulkan instance and RAII context
        vk::raii::Context context;
        vk::raii::Instance instance = nullptr;

        // Debug messenger
        vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

        // Surface
        vk::raii::SurfaceKHR surface = nullptr;

        // Physical device pointer
        vk::raii::PhysicalDevice physicalDevice = nullptr;

        // Logical device pointer
        vk::raii::Device device = nullptr;

        // Index of device in our family queue
        uint32_t queueIndex = ~0;

        // Graphics queue
        vk::raii::Queue queue = nullptr;

        // Swap chain
        vk::raii::SwapchainKHR swapChain = nullptr;
        std::vector<vk::Image> swapChainImages;
        vk::SurfaceFormatKHR swapChainSurfaceFormat;
        vk::Extent2D swapChainExtent;
        std::vector<vk::raii::ImageView> swapChainImageViews;

        // Descriptor pool and sets
        vk::raii::DescriptorPool descriptorPool = nullptr;
        std::vector<vk::raii::DescriptorSet> computeDescriptorSets;

        // Descriptor layout (uniforms)
        vk::raii::DescriptorSetLayout computeDescriptorSetLayout = nullptr;

        // Pipeline layout (textures eventually?)
        vk::raii::PipelineLayout pipelineLayout = nullptr;

        // Graphics pipeline
        vk::raii::Pipeline graphicsPipeline = nullptr; 

        // Supports the required extensions (only one for now?)
        std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

        // Command pool
        vk::raii::CommandPool commandPool = nullptr;

        // Command buffers
        std::vector<vk::raii::CommandBuffer> commandBuffers;

        // Synchronization objects
        vk::raii::Semaphore semaphore = nullptr;
        uint64_t timelineValue = 0;
        std::vector<vk::raii::Fence> inFlightFences;

        // Frame index
        uint32_t frameIndex = 0;

        // Check for resizing
        bool frameBufferResized = false;

        // Uniform buffer (one for each frame in flight)
        std::vector<vk::raii::Buffer> uniformBuffers;
        std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        // Compute shader things
        std::vector<vk::raii::Buffer> shaderStorageBuffers;
        std::vector<vk::raii::DeviceMemory> shaderStorageBuffersMemory;

        vk::raii::PipelineLayout computePipelineLayout = nullptr;
        vk::raii::Pipeline computePipeline = nullptr;

        std::vector<vk::raii::CommandBuffer> computeCommandBuffers;

        double lastFrameTime = 0.0;
        double lastTime = 0.0;

        // Core functions
        void initWindow();

        void initVulkan();

        void mainLoop();

        void cleanup();

        void createInstance();

        std::vector<const char*> getRequiredInstanceExtensions();

        void setupDebugMessenger();

        void createSurface();

        void pickPhysicalDevice();

        bool isDeviceSuitable(const vk::raii::PhysicalDevice& thisDevice);

        void createLogicalDevice();

        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats);

        vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes);

        vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities);

        uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities);

        void createSwapChain();

        void createImageViews();

        void createGraphicsPipeline();

        [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

        void createCommandPool();

        void createCommandBuffers();

        void recordCommandBuffer(uint32_t imageIndex);

        void transition_image_layout(uint32_t imageIndex,
                                     vk::ImageLayout oldLayout,
                                     vk::ImageLayout newLayout,
                                     vk::AccessFlags2 srcAccessMask,
                                     vk::AccessFlags2 dstAccessMask,
                                     vk::PipelineStageFlags2 srcStageMask,
                                     vk::PipelineStageFlags2 dstStageMask);

        void drawFrame();

        void createSyncObjects();

        void cleanupSwapChain();
        
        void recreateSwapChain();

        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

        void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);

        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);

        vk::raii::CommandBuffer beginSingleTimeCommands();

        void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer);

        void createDescriptorSetLayout();

        void createUniformBuffers();

        void updateUniformBuffer(uint32_t currentImage);

        void createDescriptorPool();

        void createDescriptorSets();

        void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);

        void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

        void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);

        vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format);

        void createShaderStorageBuffers();

        void createComputePipeline();

        void createComputeCommandBuffers();

        void recordComputeCommandBuffer();
};