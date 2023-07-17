#pragma once

#include "base/singleton.h"
#include "vulkan/vkutils.h"

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanBaseApp 
{
public:
    const int MAX_FRAMES_IN_FLIGHT = 2;

    // is measured in screen coordinates
    // But Vulkan works with pixels
    // glfwGetFramebufferSize to query the resolution of the window in pixel
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

public:

    VulkanBaseApp();
    virtual ~VulkanBaseApp();

    void Run();

    virtual void InitializeVulkan();

    void MainLoop();

    virtual void Cleanup();

    void InitWindow();

    void PrepeareFrame();
    void SubmitFrame();

    virtual void Render() = 0;
    virtual void UpdateUniformBuffer(uint32_t frameIdx) = 0;

    // Utils
    void* GetWindowHandle() const { return m_Window; }

    // Vulkan data
    VkInstance GetVkInstance() const { return m_Instance; }
    VkPhysicalDevice GetVkPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice GetVkDevice() const { return m_Device; }
    QueueFamilyIndices GetQueueFamilyIndices() const { return m_QueFamilyndices; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    uint32_t GetSwapChainImageCount() const { return (uint32_t)m_SwapChainImages.size(); }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }

protected:

    //////////////////////////////////////////////////////////////////////////
    /// Vulkan
    void CreateVulkanInstance();
    void CreateSurface();

    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapChain();
    void RecreateSwapChain();
    void CleanupSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateColorResources(); // MSAA image
    void CreateDepthResources(); // Depth image
    void CreateSyncObjects();

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    bool CheckValidationLayerSupport() const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

    std::vector<const char*> GetRequiredExtensions() const;

    // Swap Chain
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;

    // MSAA
    VkSampleCountFlagBits GetMaxUsableSampleCount() const;

    int RateDeviceSuitability(VkPhysicalDevice device) const;

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) const;

    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

    void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    VkFormat FindDepthFormat() const;

    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

    void SetCurrentDirectory();

protected:


    GLFWwindow* m_Window = nullptr;

    VkInstance m_Instance;

    VkDebugUtilsMessengerEXT m_DebugMessenger;
    VkSurfaceKHR m_Surface;

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // A Physical device
    VkDevice m_Device;                                  // A Logical device
    QueueFamilyIndices m_QueFamilyndices;
    VkQueue m_GraphicsQueue;                            // Device queues are implicitly cleaned up when the device is destroyed
    VkQueue m_PresentQueue;

    // Swap chain
    std::vector<VkImage> m_SwapChainImages;
    std::vector<VkImageView> m_SwapChainImageViews;
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

    VkSwapchainKHR m_SwapChain;
    VkFormat m_SwapChainImageFormat;
    VkExtent2D m_SwapChainExtent;

    VkRenderPass m_RenderPass;

    VkCommandPool m_CommandPool;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Depth
    VkImage m_DepthImage;
    VkDeviceMemory m_DepthImageMemory;
    VkImageView m_DepthImageView;

    // MSAA
    VkSampleCountFlagBits m_MsaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImage m_ColorImage;
    VkDeviceMemory m_ColorImageMemory;
    VkImageView m_ColorImageView;

    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    uint32_t m_CurrentFrameIdx = 0;

    uint32_t m_FrameBufferImageIndex = 0;

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

    bool m_IsFramebufferResized = false;
};