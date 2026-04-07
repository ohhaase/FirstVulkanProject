#include "vk_engine.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <chrono>
#include <thread>

#include "vk_initializers.hpp"
#include "vk_types.hpp"

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

    
    // Initialize GLFW and create window
    glfwInit();

    // Tell GLFW to not use OpenGL
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // To be fixed soon

    // Create window
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    // glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    // If everything went fine...
    isInitialized = true;
}


void VulkanEngine::cleanup()
{
    if (isInitialized)
    {
        // Destroy window
        glfwDestroyWindow(window);
    }

    // Clear engine pointer
    loadedEngine = nullptr;
}


void VulkanEngine::draw()
{

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