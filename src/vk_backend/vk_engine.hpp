#pragma once

#include "vk_types.hpp"

class VulkanEngine
{
    public:
        bool isInitialized = false;
        int frameNumber = 0;
        bool stop_rendering = false;

        VkExtent2D windowExtent{WIDTH, HEIGHT};

        struct GLFWwindow* window = nullptr;

        static VulkanEngine& Get();

        // Initializes everything
        void init();

        // Shuts down and deletes things
        void cleanup();

        // Draw loop
        void draw();

        // Main loop
        void run();
};