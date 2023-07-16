#include "VKPlayground_PCH.h"

#include "example_app.h"

int main()
{
    VulkanExample::CreateInstance();
    VulkanExample& app = *VulkanExample::GetInstance();

    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    VulkanExample::DestroyInstance();

    return 0;
}