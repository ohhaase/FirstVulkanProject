#include <iostream>
#include <stdexcept>

#include "vk_backend/vk_engine.hpp"


int main()
{
    try
    {
        VulkanEngine engine;

        engine.init();

        engine.run();

        engine.cleanup();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}