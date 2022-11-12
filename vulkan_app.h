#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <cstdint>   // Necessary for uint32_t
#include <limits>    // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

#include <cstring>
#include <iostream>
#include <vector>
#include <optional>
#include <map>
#include <set>

#include <vulkan/vulkan.h>

/*
    https://vulkan-tutorial.com/
*/

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool IsComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
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
    void CreateSurface();

    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();

    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) const;

    void DrawFrame();

    bool CheckValidationLayerSupport() const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

    std::vector<const char*> GetRequiredExtensions() const;

    // Swap Chain
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;

    int RateDeviceSuitability(VkPhysicalDevice device) const;

    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;

private:
    const int MAX_FRAMES_IN_FLIGHT = 2;

    // is measured in screen coordinates
    // But Vulkan works with pixels
    // glfwGetFramebufferSize to query the resolution of the window in pixel
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    GLFWwindow* m_Window = nullptr;

    VkInstance m_Instance; 

    VkDebugUtilsMessengerEXT m_DebugMessenger;
    VkSurfaceKHR m_Surface;

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // A Physical device
    VkDevice m_Device;                                  // A Logical device
    VkQueue m_GraphicsQueue;                            // Device queues are implicitly cleaned up when the device is destroyed
    VkQueue m_PresentQueue;

    // Swap chain
    std::vector<VkImage> m_SwapChainImages;
    std::vector<VkImageView> m_SwapChainImageViews;
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

    VkSwapchainKHR m_SwapChain;
    VkFormat m_SwapChainImageFormat;
    VkExtent2D m_SwapChainExtent;

    // Pipeline
    VkRenderPass m_RenderPass;
    VkPipelineLayout m_PipelineLayout;
    VkPipeline m_GraphicsPipeline;

    VkCommandPool m_CommandPool;

    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    uint32_t m_CurrentFrameIdx = 0;

    const std::vector<const char*> m_ValidationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> m_DeviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif
};