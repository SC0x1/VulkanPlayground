#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <cstring>
#include <iostream>
#include <vector>
#include <optional>

#include <vulkan/vulkan.h>

/*
    https://vulkan-tutorial.com/
*/

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;

    bool IsComplete()
    {
        return graphicsFamily.has_value();
    }
};

class HelloTriangleApplication
{
public:

    void Run();

private:
    void InitVulkan();

    void MainLoop();

    void Cleanup();

    void InitWindow();

    //////////////////////////////////////////////////////////////////////////
    /// Vulkan
    void CreateInstance();

    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    bool CheckValidationLayerSupport();

    std::vector<const char*> GetRequiredExtensions();

    int RateDeviceSuitability(VkPhysicalDevice device);

private:
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    GLFWwindow* m_Window = nullptr;

    VkInstance m_Instance; 

    VkDebugUtilsMessengerEXT m_DebugMessenger;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // A Physical device
    VkDevice m_Device;                                  // A Logical device
    VkQueue m_GraphicsQueue;                            // Device queues are implicitly cleaned up when the device is destroyed

    const std::vector<const char*> m_ValidationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif
};