#include "VKPlayground_PCH.h"

#include "example_app.h"

int main()
{
    VulkanExample app;

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