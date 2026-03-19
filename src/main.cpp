#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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

// Vertex structs
struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))};
    }
};

// Vertex buffer
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // Top left
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // Top right
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, // Bottom right
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}} // Bottom left
};

// Index buffer
const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
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
        std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
        std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
        std::vector<vk::raii::Fence> inFlightFences;

        // Frame index
        uint32_t frameIndex = 0;

        // Check for resizing
        bool frameBufferResized = false;

        // Vertex buffer
        vk::raii::Buffer vertexBuffer = nullptr;
        vk::raii::DeviceMemory vertexBufferMemory = nullptr; // They recommend combining these two buffers eventually, saving on memory access and stuff?

        // Index buffer
        vk::raii::Buffer indexBuffer = nullptr;
        vk::raii::DeviceMemory indexBufferMemory = nullptr;

        void initWindow()
        {
            // Initialize GLFW
            glfwInit();

            // Tell GLFW to not use OpenGL
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            // Create window
            window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
            glfwSetWindowUserPointer(window, this);
            glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
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
            createGraphicsPipeline();
            createCommandPool();
            createVertexBuffer();
            createIndexBuffer();
            createCommandBuffers();
            createSyncObjects();
        }

        void mainLoop()
        {
            // Loop until told to close
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
                drawFrame();
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

            std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            // If in debug mode, add debug extension
            if (enableValidationLayers)
            {
                extensions.push_back(vk::EXTDebugUtilsExtensionName);
            }

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

            for (const auto& thisDevice : physicalDevices)
            {
                if (isDeviceSuitable(thisDevice))
                {
                    physicalDevice = thisDevice;
                }
                break;
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
            bool isDedicatedGPU = deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu; //This one I chose to do myself :)

            // Supports our version of Vulkan
            bool supportsVulkan1_3 = deviceProperties.apiVersion >= vk::ApiVersion13;

            // Supports graphics commands
            bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) {return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

            bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtension, 
                                                                     [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
                                                                        return std::ranges::any_of(availableDeviceExtensions,
                                                                                                   [requiredDeviceExtension](auto const& availableDeviceExtension){
                                                                                                    return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
                                                                                                   });
                                                                     });
                                                                    
            // Feature check
            auto features = thisDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering && features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            return isDedicatedGPU && supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions;
        }

        void createLogicalDevice()
        {
            // get the properties of the queue families
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

            // Find first index that supports graphics + present
            for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
            {
                if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
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
                               vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
                                    {},
                                    {.dynamicRendering = true},
                                    {.extendedDynamicState = true}
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
            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();

            vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {.vertexBindingDescriptionCount = 1,
                                                                      .pVertexBindingDescriptions = &bindingDescription,
                                                                      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
                                                                      .pVertexAttributeDescriptions = attributeDescriptions.data()};

            vk::PipelineInputAssemblyStateCreateInfo inputAssembly {.topology = vk::PrimitiveTopology::eTriangleList};
            vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};
            vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
                                                                .rasterizerDiscardEnable = vk::False,
                                                                .polygonMode = vk::PolygonMode::eFill,
                                                                .cullMode = vk::CullModeFlagBits::eBack,
                                                                .frontFace = vk::FrontFace::eClockwise,
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

            commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
            commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);

            commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));

            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

            commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 0);

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
            vk::Result fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);

            if (fenceResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Failed to wait for fence!");
            }

            auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

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

            commandBuffers[frameIndex].reset();
            recordCommandBuffer(imageIndex);

            vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

            const vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                                            .pWaitSemaphores = &*presentCompleteSemaphores[frameIndex],
                                            .pWaitDstStageMask = &waitDestinationStageMask,
                                            .commandBufferCount = 1,
                                            .pCommandBuffers = &*commandBuffers[frameIndex],
                                            .signalSemaphoreCount = 1,
                                            .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]};
                                    
            queue.submit(submitInfo, *inFlightFences[frameIndex]);

            const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                                    .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
                                                    .swapchainCount = 1,
                                                    .pSwapchains = &*swapChain,
                                                    .pImageIndices = &imageIndex};

            result = queue.presentKHR(presentInfoKHR);

            if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || frameBufferResized)
            {
                frameBufferResized = false;
                recreateSwapChain();
            }
            else
            {
                assert(result == vk::Result::eSuccess);
            }

            frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        void createSyncObjects()
        {
            assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

            for (size_t i = 0; i < swapChainImages.size(); i++)
            {
                renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
            }

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
                inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
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
                glfwWaitEvents;
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

        void createVertexBuffer()
        {
            vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

            // Temporary device buffer
            vk::raii::Buffer stagingBuffer = nullptr;
            vk::raii::DeviceMemory stagingBufferMemory = nullptr;
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

            // Fill staging buffer
            void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
            memcpy(dataStaging, vertices.data(), bufferSize);
            stagingBufferMemory.unmapMemory();

            // Vertex buffer on device memory
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);

            // Copy from staging buffer to vertex buffer (on GPU)
            copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
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
            vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                    .level = vk::CommandBufferLevel::ePrimary,
                                                    .commandBufferCount = 1};

            vk::raii::CommandBuffer commandCopyBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

            commandCopyBuffer.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));

            commandCopyBuffer.end();

            queue.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer}, nullptr);
            queue.waitIdle();
        }

        void createIndexBuffer()
        {
            vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

            // Staging buffer
            vk::raii::Buffer stagingBuffer = nullptr;
            vk::raii::DeviceMemory stagingBufferMemory = nullptr;
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

            // Fill staging buffer
            void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
            memcpy(dataStaging, indices.data(), (size_t)bufferSize);
            stagingBufferMemory.unmapMemory();

            // Index buffer on device memory
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);

            // Copy from staging buffer to index buffer (on GPU)
            copyBuffer(stagingBuffer, indexBuffer, bufferSize);
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