#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <algorithm>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Enable validation layers when building in DEBUG
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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

        // Graphics queue
        vk::raii::Queue graphicsQueue = nullptr;

        // Swap chain
        vk::raii::SwapchainKHR swapChain = nullptr;
        std::vector<vk::Image> swapChainImages;
        vk::SurfaceFormatKHR swapChainSurfaceFormat;
        vk::Extent2D swapChainExtent;
        std::vector<vk::raii::ImageView> swapChainImageViews;

        void initWindow()
        {
            // Initialize GLFW
            glfwInit();

            // Tell GLFW to not use OpenGL nor allow resizability (for now)
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            // Create window
            window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
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
        }

        void mainLoop()
        {
            // Loop until told to close
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
            }
        }

        void cleanup()
        {
            // The tutorial I'm following uses RAII, so this will not change anymore.
            // Usually you would have to remove all the Vulkan things at the end here.
            
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

            // Supports the required extensions (only one for now?)
            std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

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
            uint32_t queueIndex = ~0;
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
            
            // Required device extensions
            std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName}; // will add more later

            // Create logical device
            vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                                  .queueCreateInfoCount = 1,
                                                  .pQueueCreateInfos = &deviceQueueCreateInfo,
                                                  .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
                                                  .ppEnabledExtensionNames = requiredDeviceExtension.data()};

            device = vk::raii::Device(physicalDevice, deviceCreateInfo);

            graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
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