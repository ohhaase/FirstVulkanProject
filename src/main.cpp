#include <iostream>
#include <stdexcept>

#include "vulkan_backend/VulkanApplication.hpp"
#include "core/file_functions.hpp"


int main()
{
    try
    {
        VulkanApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}