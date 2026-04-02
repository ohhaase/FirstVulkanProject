#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>
#include <random>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t PARTICLE_COUNT = 8192;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Enable validation layers when building in DEBUG
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


// Helper function for reading files
static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file!");
    }

    // Make buffer to store file data
    std::vector<char> buffer(file.tellg());

    // Go back to beginning and copy all data
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return buffer;
}

struct UniformBufferObject
{
    float deltaTime = 1.0f;
};

// Particle struct
struct Particle
{
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec4 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {0, sizeof(Particle), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Particle, position)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, color))};
    }
};

class HelloTriangleApplication
{
    public:
        void run()
        {
            initWindow();
            initVulkan();
            mainLoop();
            cleanup();
        }

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

        void initWindow()
        {
            // Initialize GLFW
            glfwInit();

            // Tell GLFW to not use OpenGL
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

            // Create window
            window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
            glfwSetWindowUserPointer(window, this);
            glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

            lastTime = glfwGetTime();
        }

        void initVulkan()
        {
            createInstance();
            setupDebugMessenger();
            createSurface();

            pickPhysicalDevice(); // Will eventually be able to choose multiple? Applications to HPC?
            createLogicalDevice();

            createSwapChain();
            createImageViews();

            createDescriptorSetLayout();

            createGraphicsPipeline();
            createComputePipeline();

            createCommandPool();
            createShaderStorageBuffers();
            createUniformBuffers();

            createDescriptorPool();
            createDescriptorSets();

            createCommandBuffers();
            createComputeCommandBuffers();

            createSyncObjects();
        }

        void mainLoop()
        {
            // Loop until told to close
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
                drawFrame();

                double currentTime = glfwGetTime();
                lastFrameTime = (currentTime - lastTime) * 1000.0;
                lastTime = currentTime;
            }

            device.waitIdle();
        }

        void cleanup()
        {
            // The tutorial I'm following uses RAII, so this will not change anymore.
            // Usually you would have to remove all the Vulkan things at the end here.
            
            cleanupSwapChain();

            // Destroy window
            glfwDestroyWindow(window);
            
            // Terminate GLFW
            glfwTerminate();
        }

        void createInstance()
        {
            // Overarching application information
            constexpr vk::ApplicationInfo appInfo{
                .pApplicationName = "Hello Triange",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "No Engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = vk::ApiVersion14
            };

            // // List out available extensions
            // auto extensions = context.enumerateInstanceExtensionProperties();

            // std::cout << "Available extensions:\n";

            // for (const auto& extension : extensions) {
            //     std::cout << '\t' << extension.extensionName << '\n';
            // }

            // Get required extensions
            auto requiredExtensions = getRequiredInstanceExtensions();

            // Check if the required extensions are supported by the Vulkan implementation
            auto extensionProperties = context.enumerateInstanceExtensionProperties();
            auto unsupportedPropretyIt = std::ranges::find_if(requiredExtensions, 
                                                                [&extensionProperties](auto const &requiredExtension) {
                                                                    return std::ranges::none_of(extensionProperties, 
                                                                                                [requiredExtension](auto const &extensionProperty){
                                                                                                    return strcmp(extensionProperty.extensionName, requiredExtension) == 0; 
                                                                                                }); 
                                                                                            }); // Horrible awful no good unreadable code

            // Get required layers
            std::vector<char const*> requiredLayers;
            if (enableValidationLayers)
            {
                requiredLayers.assign(validationLayers.begin(), validationLayers.end());
            }

            // Check if the required layers are supported by the Vulkan implementation
            auto layerProperties = context.enumerateInstanceLayerProperties();
            // I hate this C++ garbage. why not just write an if statement
            auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, 
                                                            [&layerProperties](auto const &requiredLayer) {
                                                                return std::ranges::none_of(layerProperties, 
                                                                                            [requiredLayer](auto const &layerProperty) {
                                                                                                return strcmp(layerProperty.layerName, requiredLayer) == 0; 
                                                                                            });
                                                                                        }); // Horrible awful no good unreadable code part 2
            
            if (unsupportedLayerIt != requiredLayers.end())
            {
                throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
            }


            // Instance information, will change throughout program?
            vk::InstanceCreateInfo createInfo{
                .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
                .ppEnabledLayerNames = requiredLayers.data(),
                .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
                .ppEnabledExtensionNames = requiredExtensions.data()
            };

            // Now, finally create the instance
            instance = vk::raii::Instance(context, createInfo);

            // TODO: MIGHT NEED TO ADD STUFF HERE FOR MACOS
        }

        std::vector<const char*> getRequiredInstanceExtensions()
        {
            // Get the required instance extensions from GLFW (platform agnostic)
            uint32_t glfwExtensionCount = 0;
            auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            // If in debug mode, add debug extension
            if (enableValidationLayers)
            {
                extensions.push_back(vk::EXTDebugUtilsExtensionName);
            }

            extensions.emplace_back(vk::KHRPortabilityEnumerationExtensionName);

            return extensions;
        }

        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                              vk::DebugUtilsMessageTypeFlagsEXT type,
                                                              const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                              void* pUserData)
        {
            std::cerr << "Validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

            return vk::False;
        }

        void setupDebugMessenger()
        {
            if (!enableValidationLayers) return;

            // Can change which levels of severity are sent to debug output here
            vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

            // Can change which types are sent to debug output here
            vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
            
            vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
                                                                                    .messageType = messageTypeFlags,
                                                                                    .pfnUserCallback = &debugCallback};

            debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
        }

        void createSurface()
        {
            VkSurfaceKHR Csurface;
            
            if (glfwCreateWindowSurface(*instance, window, nullptr, &Csurface) != 0)
            {
                throw std::runtime_error("Failed to create window surface!");
            }

            surface = vk::raii::SurfaceKHR(instance, Csurface);
        }

        void pickPhysicalDevice()
        {
            // Get list of devices
            auto physicalDevices = instance.enumeratePhysicalDevices();

            // If empty, then we're SOL
            if (physicalDevices.empty())
            {
                throw std::runtime_error("Failed to find GPUs with Vulkan support!");
            }

            // Loop through and find suitable device
            // TODO: SCORE GPUs TO CHOOSE BEST ONE
            bool foundDevice = false;
            std::cout << "Found devices: \n";
            for (const auto& thisDevice : physicalDevices)
            {
                std::cout << thisDevice.getProperties().deviceName << "\n";

                if (isDeviceSuitable(thisDevice))
                {
                    physicalDevice = thisDevice;
                    foundDevice = true;
                    break;
                }              
            }

            if (foundDevice)
            {
                std::cout << "Chose device: " << physicalDevice.getProperties().deviceName << "\n";
            }

            // If we got to this point, we didn't find a gpu we're happy with
            if (!foundDevice)
            {
                throw std::runtime_error("Failed to find suitable GPU!");
            }
        }

        bool isDeviceSuitable(const vk::raii::PhysicalDevice& thisDevice)
        {
            // Can put a bunch of checks things in here!

            // Get device info
            vk::PhysicalDeviceProperties deviceProperties = thisDevice.getProperties();
            vk::PhysicalDeviceFeatures deviceFeatures = thisDevice.getFeatures();
            std::vector<vk::QueueFamilyProperties> queueFamilies = thisDevice.getQueueFamilyProperties(); // These queue families might have important things for compute-only GPUs
            std::vector<vk::ExtensionProperties> availableDeviceExtensions = thisDevice.enumerateDeviceExtensionProperties();

            // Check for dedicated GPU
            // bool isDedicatedGPU = deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu; //This one I chose to do myself :) <-- YOU MORON IT DOESNT WORK ON MAC

            // Supports our version of Vulkan
            bool supportsVulkan1_3 = deviceProperties.apiVersion >= vk::ApiVersion13;

            // Supports graphics commands
            bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) {return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

            // Supports compute
            bool supportsCompute = std::ranges::any_of(queueFamilies, [](auto const &qfp) {return !!(qfp.queueFlags & vk::QueueFlagBits::eCompute); });

            bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtension, 
                                                                     [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
                                                                        return std::ranges::any_of(availableDeviceExtensions,
                                                                                                   [requiredDeviceExtension](auto const& availableDeviceExtension){
                                                                                                    return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
                                                                                                   });
                                                                     });
                                                                    
            // Feature check
            auto features = thisDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT, vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>();
            bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering && 
                                            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
                                            features.template get<vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>().timelineSemaphore;

            bool supportsAnisotropy = thisDevice.getFeatures().samplerAnisotropy;

            return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsCompute; 
        }

        void createLogicalDevice()
        {
            // get the properties of the queue families
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

            // Find first index that supports graphics + present
            for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
            {
                if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface) && (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eCompute))
                {
                    queueIndex = qfpIndex;
                    break;
                }
            }

            // If none are found, crash
            if (queueIndex == ~0)
            {
                throw std::runtime_error("Could not find a queue for graphics and present. Terminating!");
            }

            // Chain of physical device features to make sure theyre vailable
            vk::StructureChain<vk::PhysicalDeviceFeatures2, 
                               vk::PhysicalDeviceVulkan13Features, 
                               vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                               vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR> featureChain = {
                                    {.features = {.samplerAnisotropy = true}}, 
                                    {.synchronization2 = true, .dynamicRendering = true},
                                    {.extendedDynamicState = true},
                                    {.timelineSemaphore = true}
                               };

            // Create device
            float queuePriority = 0.5f;

            vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex,
                                                            .queueCount = 1,
                                                            .pQueuePriorities = &queuePriority};

            // Physical device features
            vk::PhysicalDeviceFeatures deviceFeatures; // All false for now, will come back to later

            // Create logical device
            vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                                  .queueCreateInfoCount = 1,
                                                  .pQueueCreateInfos = &deviceQueueCreateInfo,
                                                  .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
                                                  .ppEnabledExtensionNames = requiredDeviceExtension.data()};

            device = vk::raii::Device(physicalDevice, deviceCreateInfo);

            queue = vk::raii::Queue(device, queueIndex, 0);
        }

        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
        {
            // This whole function needs to be rewritten to not be C++ slop
            const auto formatIt = std::ranges::find_if(availableFormats,
                                                        [](const auto& format) {
                                                            // Is this what I change if i need more precision?
                                                            return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; 
                                                        });
            
            return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
        }

        vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes)
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

        vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
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

        uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
        {
            uint32_t minImageCount = std::max(3u, surfaceCapabilities.minImageCount);

            if ((surfaceCapabilities.maxImageCount > 0) && (surfaceCapabilities.maxImageCount < minImageCount))
            {
                minImageCount = surfaceCapabilities.maxImageCount;
            }
            return minImageCount;
        }

        void createSwapChain()
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

        void createImageViews()
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

        void createGraphicsPipeline()
        {
            // Vertex + fragment shader
            vk::raii::ShaderModule shaderModule = createShaderModule(readFile(std::string(SHADER_DIR) + "slang.spv"));

            vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex,
                                                                  .module = shaderModule,
                                                                  .pName = "vertMain"};
                                                    
            vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment,
                                                                  .module = shaderModule,
                                                                  .pName = "fragMain"};

            vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            // Fixed parts of the graphics pipeline
            auto bindingDescription = Particle::getBindingDescription();
            auto attributeDescriptions = Particle::getAttributeDescriptions();

            vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {.vertexBindingDescriptionCount = 1,
                                                                      .pVertexBindingDescriptions = &bindingDescription,
                                                                      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
                                                                      .pVertexAttributeDescriptions = attributeDescriptions.data()};

            vk::PipelineInputAssemblyStateCreateInfo inputAssembly {.topology = vk::PrimitiveTopology::ePointList, .primitiveRestartEnable = vk::False};
            vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};
            vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
                                                                .rasterizerDiscardEnable = vk::False,
                                                                .polygonMode = vk::PolygonMode::eFill,
                                                                .cullMode = vk::CullModeFlagBits::eBack,
                                                                .frontFace = vk::FrontFace::eCounterClockwise,
                                                                .depthBiasEnable = vk::False,
                                                                .depthBiasSlopeFactor = 1.0f,
                                                                .lineWidth = 1.0f};
            vk::PipelineMultisampleStateCreateInfo multisampling {.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                                  .sampleShadingEnable = vk::False};
            vk::PipelineColorBlendAttachmentState colorBlendAttachment {.blendEnable = vk::False,
                                                                        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
            vk::PipelineColorBlendStateCreateInfo colorBlending {.logicOpEnable = vk::False,
                                                                 .logicOp = vk::LogicOp::eCopy,
                                                                 .attachmentCount = 1,
                                                                 .pAttachments = &colorBlendAttachment};
            // Dynamic states
            std::vector dynamicStates = {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor
            };

            vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                            .pDynamicStates = dynamicStates.data()};

            // Pipeline layout (textures eventually?)
            vk::PipelineLayoutCreateInfo pipelineLayoutInfo;

            pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

            // Finally make the pipeline
            vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo {.colorAttachmentCount = 1,
                                                                         .pColorAttachmentFormats = &swapChainSurfaceFormat.format};

            vk::GraphicsPipelineCreateInfo pipelineInfo {.pNext = &pipelineRenderingCreateInfo,
                                                         .stageCount = 2,
                                                         .pStages = shaderStages,
                                                         .pVertexInputState = &vertexInputInfo,
                                                         .pInputAssemblyState = &inputAssembly,
                                                         .pViewportState = &viewportState,
                                                         .pRasterizationState = &rasterizer,
                                                         .pMultisampleState = &multisampling,
                                                         .pColorBlendState = &colorBlending,
                                                         .pDynamicState = &dynamicState,
                                                         .layout = pipelineLayout,
                                                         .renderPass = nullptr};

            graphicsPipeline = vk::raii::Pipeline{device, nullptr, pipelineInfo};
        }

        [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
        {
            vk::ShaderModuleCreateInfo createInfo {.codeSize = code.size() *sizeof(char),
                                                   .pCode = reinterpret_cast<const uint32_t*>(code.data())};
            
            vk::raii::ShaderModule shaderModule{device, createInfo};

            return shaderModule;
        }

        void createCommandPool()
        {
            vk::CommandPoolCreateInfo poolInfo {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                .queueFamilyIndex = queueIndex};

            commandPool = vk::raii::CommandPool(device, poolInfo);
        }

        void createCommandBuffers()
        {
            commandBuffers.clear();

            vk::CommandBufferAllocateInfo allocInfo {.commandPool = commandPool,
                                                     .level = vk::CommandBufferLevel::ePrimary,
                                                     .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
            
            commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
        }

        void recordCommandBuffer(uint32_t imageIndex)
        {
            vk::raii::CommandBuffer& commandBuffer = commandBuffers[frameIndex];
            commandBuffer.reset();
            commandBuffer.begin({});

            // Before starting rendering, transition swapchain image
            transition_image_layout(imageIndex, 
                                    vk::ImageLayout::eUndefined,
                                    vk::ImageLayout::eColorAttachmentOptimal,
                                    {},
                                    vk::AccessFlagBits2::eColorAttachmentWrite,
                                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                    vk::PipelineStageFlagBits2::eColorAttachmentOutput);

            vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

            vk::RenderingAttachmentInfo attachmentInfo = {.imageView = swapChainImageViews[imageIndex],
                                                          .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                          .loadOp = vk::AttachmentLoadOp::eClear,
                                                          .storeOp = vk::AttachmentStoreOp::eStore,
                                                          .clearValue = clearColor};

            vk::RenderingInfo renderingInfo = {.renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
                                               .layerCount = 1,
                                               .colorAttachmentCount = 1,
                                               .pColorAttachments = &attachmentInfo};

            commandBuffer.beginRendering(renderingInfo);

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

            commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
            
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

            commandBuffer.bindVertexBuffers(0, {*shaderStorageBuffers[frameIndex]}, {0});

            commandBuffer.draw(PARTICLE_COUNT, 1, 0, 0);

            commandBuffer.endRendering();

            // Aftering rendering, transition image layout back
            transition_image_layout(imageIndex, 
                                    vk::ImageLayout::eColorAttachmentOptimal,
                                    vk::ImageLayout::ePresentSrcKHR,
                                    vk::AccessFlagBits2::eColorAttachmentWrite,
                                    {},
                                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                    vk::PipelineStageFlagBits2::eBottomOfPipe);

            commandBuffer.end();
        }

        void transition_image_layout(uint32_t imageIndex,
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

        void drawFrame()
        {            
            auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, nullptr, *inFlightFences[frameIndex]);
            vk::Result fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);

            if (fenceResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Failed to wait for fence!");
            }
            
            if (result == vk::Result::eErrorOutOfDateKHR)
            {
                recreateSwapChain();
                return;
            }
            if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
            {
                assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
                throw std::runtime_error("Failed to acquire swap chain image!");
            }

            // Only reset fence if above passes
            device.resetFences(*inFlightFences[frameIndex]);

            // Update timeline value for this frame
            uint64_t computeWaitValue = timelineValue;
            uint64_t computeSignalValue = ++timelineValue;
            uint64_t graphicsWaitValue = computeSignalValue;
            uint64_t graphicsSignalValue = ++timelineValue;

            updateUniformBuffer(frameIndex);

            {// Needs to be in braces for some reason?
                recordComputeCommandBuffer();

                //Submit compute work
                vk::TimelineSemaphoreSubmitInfo computeTimelineInfo{.waitSemaphoreValueCount = 1,
                                                                    .pWaitSemaphoreValues = &computeWaitValue,
                                                                    .signalSemaphoreValueCount = 1,
                                                                    .pSignalSemaphoreValues = &computeSignalValue};

                vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eComputeShader};

                vk::SubmitInfo computeSubmitInfo{.pNext = &computeTimelineInfo,
                                                 .waitSemaphoreCount = 1,
                                                 .pWaitSemaphores = &*semaphore,
                                                 .pWaitDstStageMask = waitStages,
                                                 .commandBufferCount = 1,
                                                 .pCommandBuffers = &*computeCommandBuffers[frameIndex],
                                                 .signalSemaphoreCount = 1,
                                                 .pSignalSemaphores = &*semaphore};

                queue.submit(computeSubmitInfo, nullptr);
            }

            {
                // Record graphics command bufffer
                recordCommandBuffer(imageIndex);

                // Submit graphics work (after waiting on compute)
                vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eVertexInput;

                vk::TimelineSemaphoreSubmitInfo graphicsTimelineInfo{.waitSemaphoreValueCount = 1,
                                                                     .pWaitSemaphoreValues = &graphicsWaitValue,
                                                                     .signalSemaphoreValueCount = 1,
                                                                     .pSignalSemaphoreValues = &graphicsSignalValue};

                vk::SubmitInfo graphicsSubmitInfo{.pNext = &graphicsTimelineInfo,
                                                  .waitSemaphoreCount = 1,
                                                  .pWaitSemaphores = &*semaphore,
                                                  .pWaitDstStageMask = &waitStage,
                                                  .commandBufferCount = 1,
                                                  .pCommandBuffers = &*commandBuffers[frameIndex],
                                                  .signalSemaphoreCount = 1,
                                                  .pSignalSemaphores = &*semaphore};

                queue.submit(graphicsSubmitInfo, nullptr);

                // Present image (after waiting for graphics)
                vk::SemaphoreWaitInfo waitInfo{.semaphoreCount = 1,
                                               .pSemaphores = &*semaphore,
                                               .pValues = &graphicsSignalValue};

                vk::Result result = device.waitSemaphores(waitInfo, UINT64_MAX);
                if (result != vk::Result::eSuccess)
                {
                    throw std::runtime_error("Failed to wait for semaphore!");
                }

                vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 0,
                                               .pWaitSemaphores = nullptr,
                                               .swapchainCount = 1,
                                               .pSwapchains = &*swapChain,
                                               .pImageIndices = &imageIndex};

                result = queue.presentKHR(presentInfo);

                // Due to VULKAN_HPP_HANDLE_ERROR_OUT_DATE_AS_SUCCESS being defined, we can check to see if the screen gets resized here
                if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || frameBufferResized)
                {
                    frameBufferResized = false;
                    recreateSwapChain();
                }
                else
                {
                    assert(result == vk::Result::eSuccess);
                }
            }

            frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        void createSyncObjects()
        {
            inFlightFences.clear();

            vk::SemaphoreTypeCreateInfo semaphoreType{.semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0};
            semaphore = vk::raii::Semaphore(device, {.pNext = &semaphoreType});
            timelineValue = 0;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                inFlightFences.emplace_back(device, vk::FenceCreateInfo{});
            }
        }

        void cleanupSwapChain()
        {
            swapChainImageViews.clear();
            swapChain = nullptr;
        }
        
        void recreateSwapChain()
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
            createImageViews();
        }

        static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
        {
            auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
            app->frameBufferResized = true;
        }

        void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory)
        {
            // Create buffer
            vk::BufferCreateInfo bufferInfo{.size = size,
                                            .usage = usage,
                                            .sharingMode = vk::SharingMode::eExclusive};

            buffer = vk::raii::Buffer(device, bufferInfo);

            // Allocate memory for buffer
            vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

            vk::MemoryAllocateInfo memoryAllocateInfo{.allocationSize = memRequirements.size,
                                                      .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};

            bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);

            buffer.bindMemory(*bufferMemory, 0);
        }

        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
        {
            // For finding the different memory types available on the GPU. might be useful in future
            vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties(); // Contains both memory types and memory heaps (locations: useful for performance)

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
            {
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                {
                    return i;
                }
            }

            throw std::runtime_error("Failed to find suitable memory type!");
        }

        void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size)
        {
            // Temporary command buffer to send the copy commands
            vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();

            commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));

            endSingleTimeCommands(commandCopyBuffer);
        }

        vk::raii::CommandBuffer beginSingleTimeCommands()
        {
            vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                    .level = vk::CommandBufferLevel::ePrimary,
                                                    .commandBufferCount = 1};

            vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

            vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
            commandBuffer.begin(beginInfo);

            return commandBuffer;
        }

        void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer)
        {
            commandBuffer.end();

            vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                                      .pCommandBuffers = &*commandBuffer};

            queue.submit(submitInfo, nullptr);
            queue.waitIdle();
        }

        void createDescriptorSetLayout()
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings = {
                vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr), // uboLayoutBinding
                vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr), // Compute shader storage buffer 1
                vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr) // Compute shader storage buffer 2
            };

            vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

            computeDescriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
        }

        void createUniformBuffers()
        {
            uniformBuffers.clear();
            uniformBuffersMemory.clear();
            uniformBuffersMapped.clear();

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
                vk::raii::Buffer buffer({});
                vk::raii::DeviceMemory bufferMem({});
                createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem);
                uniformBuffers.emplace_back(std::move(buffer));
                uniformBuffersMemory.emplace_back(std::move(bufferMem));
                uniformBuffersMapped.emplace_back(uniformBuffersMemory[i].mapMemory(0, bufferSize));
            }
        }

        void updateUniformBuffer(uint32_t currentImage)
        {
            UniformBufferObject ubo{};

            ubo.deltaTime = static_cast<float>(lastFrameTime) * 2.0f;

            memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
        }

        void createDescriptorPool()
        {
            std::vector<vk::DescriptorPoolSize> poolSizes{
                vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT), //ubo
                vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT*2) // shader storage buffer object
            };
            
            vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 
                                                  .maxSets = MAX_FRAMES_IN_FLIGHT, 
                                                  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                                  .pPoolSizes = poolSizes.data()};

            descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
        }

        void createDescriptorSets()
        {
            std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout);
            vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *descriptorPool,
                                                    .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                                                    .pSetLayouts = layouts.data()};
            
            computeDescriptorSets.clear();
            computeDescriptorSets = device.allocateDescriptorSets(allocInfo);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBuffers[i],
                                                    .offset = 0,
                                                    .range = sizeof(UniformBufferObject)};
                
                vk::WriteDescriptorSet uboDescriptorWrite{.dstSet = *computeDescriptorSets[i],
                                                          .dstBinding = 0,
                                                          .dstArrayElement = 0,
                                                          .descriptorCount = 1,
                                                          .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                          .pBufferInfo = &bufferInfo};

                vk::DescriptorBufferInfo storageBufferInfoLastFrame{.buffer = shaderStorageBuffers[(i-1) % MAX_FRAMES_IN_FLIGHT],
                                                                    .offset = 0,
                                                                    .range = sizeof(Particle)*PARTICLE_COUNT};

                vk::DescriptorBufferInfo storageBufferInfoCurrentFrame{.buffer = shaderStorageBuffers[i],
                                                                       .offset = 0,
                                                                       .range = sizeof(Particle)*PARTICLE_COUNT};

                vk::WriteDescriptorSet storageBufferLastFrameDescriptorWrite{.dstSet = *computeDescriptorSets[i],
                                                                             .dstBinding = 1,
                                                                             .dstArrayElement = 0,
                                                                             .descriptorCount = 1,
                                                                             .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                                             .pBufferInfo = &storageBufferInfoLastFrame};

                vk::WriteDescriptorSet storageBufferCurrentFrameDescriptorWrite{.dstSet = *computeDescriptorSets[i],
                                                                                .dstBinding = 2,
                                                                                .dstArrayElement = 0,
                                                                                .descriptorCount = 1,
                                                                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                                                .pBufferInfo = &storageBufferInfoCurrentFrame};

                std::vector<vk::WriteDescriptorSet> descriptorWrites = {uboDescriptorWrite, storageBufferLastFrameDescriptorWrite, storageBufferCurrentFrameDescriptorWrite};

                device.updateDescriptorSets(descriptorWrites, {});
            }
        }

        void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
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

        void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
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

        void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height)
        {
            vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

            vk::BufferImageCopy region{.bufferOffset = 0,
                                       .bufferRowLength = 0,
                                       .bufferImageHeight = 0,
                                       .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                       .imageOffset = {0, 0, 0},
                                       .imageExtent = {width, height, 1}};

            commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});

            endSingleTimeCommands(commandBuffer);
        }

        vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format)
        {
            vk::ImageViewCreateInfo imageViewCreateInfo{.image = image,
                                                        .viewType = vk::ImageViewType::e2D,
                                                        .format = format,
                                                        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
            
            return vk::raii::ImageView(device, imageViewCreateInfo);
        }

        void createShaderStorageBuffers()
        {
            // Initialize particles
            std::default_random_engine rndEngine((unsigned) time(nullptr));
            std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

            // Initial particle positions on circle
            std::vector<Particle> particles(PARTICLE_COUNT);
            for (auto& particle : particles)
            {
                float r = 0.25f * sqrtf(rndDist(rndEngine));
                float theta = rndDist(rndEngine) * 2.0f * 3.1415f;
                float x = r * cosf(theta) * HEIGHT/WIDTH;
                float y = r * sinf(theta);
                particle.position = glm::vec2(x, y);
                particle.velocity = normalize(glm::vec2(x, y)) * 0.00025f;
                particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
            }

            vk::DeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

            // Create staging buffer to hold data on host side
            vk::raii::Buffer stagingBuffer({});
            vk::raii::DeviceMemory stagingBufferMemory({});
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

            void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
            memcpy(dataStaging, particles.data(), (size_t)bufferSize);
            stagingBufferMemory.unmapMemory();

            shaderStorageBuffers.clear();
            shaderStorageBuffersMemory.clear();

            // Copy initial particle data to all storage buffers
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vk::raii::Buffer shaderStorageBufferTemp({});
                vk::raii::DeviceMemory shaderStorageBufferTempMemory({});
                createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, shaderStorageBufferTemp, shaderStorageBufferTempMemory);
                copyBuffer(stagingBuffer, shaderStorageBufferTemp, bufferSize);
                shaderStorageBuffers.emplace_back(std::move(shaderStorageBufferTemp));
                shaderStorageBuffersMemory.emplace_back(std::move(shaderStorageBufferTempMemory));
            }
        }

        void createComputePipeline()
        {
            vk::raii::ShaderModule shaderModule = createShaderModule(readFile(std::string(SHADER_DIR) + "slang.spv"));

            vk::PipelineShaderStageCreateInfo compShaderStageInfo{.stage = vk::ShaderStageFlagBits::eCompute,
                                                                  .module = shaderModule,
                                                                  .pName = "compMain"};

            vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1,
                                                            .pSetLayouts = &*computeDescriptorSetLayout};

            computePipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
            
            vk::ComputePipelineCreateInfo pipelineInfo{.stage = compShaderStageInfo,
                                                       .layout = *computePipelineLayout};

            computePipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
        }

        void createComputeCommandBuffers()
        {
            computeCommandBuffers.clear();

            vk::CommandBufferAllocateInfo allocInfo{.commandPool = *commandPool,
                                                    .level = vk::CommandBufferLevel::ePrimary,
                                                    .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

            computeCommandBuffers = vk::raii::CommandBuffers(device, allocInfo);
        }

        void recordComputeCommandBuffer()
        {
            vk::raii::CommandBuffer& commandBuffer = computeCommandBuffers[frameIndex];

            commandBuffer.reset();
            commandBuffer.begin({});

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, computePipelineLayout, 0, {computeDescriptorSets[frameIndex]}, {});
            commandBuffer.dispatch(PARTICLE_COUNT / 256, 1, 1);

            commandBuffer.end();
        }
};


int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}