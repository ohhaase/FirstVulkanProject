#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

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

        // Physical device pointer
        vk::raii::PhysicalDevice physicalDevice = nullptr;

        // Logical device pointer
        vk::raii::Device device = nullptr;

        // Graphics queue
        vk::raii::Queue graphicsQueue = nullptr;

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
            pickPhysicalDevice(); // Will eventually be able to choose multiple? Applications to HPC?
            createLogicalDevice();
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
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

            auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties,
                                                                    [](auto const &qfp) {
                                                                        return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
                                                                    });
            
            auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty)); // Does all this just find the index??? wtf why not just loop and choose the right i!??

            // We need a priority for some reason even if only a single queue?
            float queuePriority = 0.5f;

            vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = graphicsIndex,
                                                            .queueCount = 1,
                                                            .pQueuePriorities = &queuePriority};

            // Physical device features
            vk::PhysicalDeviceFeatures deviceFeatures; // All false for now, will come back to later

            // Chain of physical device features
            vk::StructureChain<vk::PhysicalDeviceFeatures2, 
                               vk::PhysicalDeviceVulkan13Features, 
                               vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
                                    {},
                                    {.dynamicRendering = true},
                                    {.extendedDynamicState = true}
                               };
            
            // Required device extensions
            std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName}; // will add more later

            // Create logical device
            vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                                  .queueCreateInfoCount = 1,
                                                  .pQueueCreateInfos = &deviceQueueCreateInfo,
                                                  .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
                                                  .ppEnabledExtensionNames = requiredDeviceExtension.data()};

            device = vk::raii::Device(physicalDevice, deviceCreateInfo);

            graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
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