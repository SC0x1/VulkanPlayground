#include "vulkan_app.h"

/*
    https://vulkan-tutorial.com/
*/

int main()
{
    VulkanApplication app;

    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}