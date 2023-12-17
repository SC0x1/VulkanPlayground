#pragma once

#include "base/singleton.h"
#include "vulkan/utils.h"
#include "vulkan/framesinflight.h"
#include "vulkan/swapchain.h"
#include "vulkan/surface.h"
#include "vulkan/instance.h"

#include "common/imgui/ImGUIVulkan.h"

class VulkanBaseApp 
{
public:
    uint32_t m_MaxFramesInFlight = 0;

    // is measured in screen coordinates
    // But Vulkan works with pixels
    // glfwGetFramebufferSize to query the resolution of the window in pixel
    const uint32_t WIDTH = 1024;
    const uint32_t HEIGHT = 768;

public:

    VulkanBaseApp();
    virtual ~VulkanBaseApp();

    void Run();

    virtual void OnInitialize();
    virtual void OnCleanup();

    virtual void OnRecreateSwapchain() = 0;

    void MainLoop();

    void InitWindow();

    void SubmitFrame(Vk::SyncObject syncObject, uint32_t frameIndex, uint32_t imageIndex);

    void PresentFrame(Vk::SyncObject syncObject, uint32_t frameIndex, uint32_t imageIndex);

    virtual void OnRender() = 0;
    virtual void UpdateUniformBuffer(uint32_t frameIndex) = 0;

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Utils
    void* GetWindowHandle() const { return m_Window; }

    // Vulkan data
    VkInstance GetVkInstance() const;
    VkPhysicalDevice GetVkPhysicalDevice() const;
    VkDevice GetVkDevice() const;
    Vk::QueueFamilyIndices GetQueueFamilyIndices() const;
    VkQueue GetVkGraphicsQueue() const;
    VkQueue GetVkPresentQueue() const;

    uint32_t GetSwapChainImageCount() const;
    VkRenderPass GetVkRenderPass() const;
    VkCommandPool GetCommandPool() const;
    VkFormat GetVkSwapchainImageFormat() const;

    VkSwapchainKHR GetVkSwapChain() const;
    VkExtent2D GetSwapChainExtend() const;
    const std::vector<VkImage>& GetSwapChainImages() const;
    const std::vector<VkImageView>& GetSwapChainImageViews() const;

    uint32_t GetMaxFramesInFlight() const;

protected:

    //////////////////////////////////////////////////////////////////////////
    /// Vulkan
    void CreateVulkanInstance();
    void CreateSurface();

    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    //void RecreateSwapchain();
    void CleanupSwapChain();

    void CreateRenderPass();

    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateColorResources(); // MSAA image
    void CreateDepthResources(); // Depth image

    //bool CheckValidationLayerSupport() const;

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

    //std::vector<const char*> GetRequiredExtensions() const;

    std::optional<uint32_t> AcquireNextImage(Vk::SyncObject syncObject);

    // MSAA
    VkSampleCountFlagBits GetMaxUsableSampleCount() const;

    int RateDeviceSuitability(VkPhysicalDevice physicalDvice) const;

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

    //VkInstance m_Instance;

    //VkDebugUtilsMessengerEXT m_DebugMessenger;

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // A Physical device
    VkDevice m_Device;                                  // A Logical device
    Vk::QueueFamilyIndices m_QueueFamilyndices;
    VkQueue m_GraphicsQueue;                            // Device queues are implicitly cleaned up when the device is destroyed
    VkQueue m_PresentQueue;

    Vk::Surface m_Surface;

    // Swap chain
    Vk::SwapChain m_SwapChain;
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

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

    Vk::FramesInFlight m_FramesInFlight;

    Vk::Instance m_Instance;

    //std::vector<VkFence> m_ImagesInFlight;

    const std::vector<const char*> m_ValidationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> m_DeviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // ImGui
    ImGuiRenderer m_ImGuiLayer;
    bool m_IsImGuiEnabled = true;

#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif

    bool m_IsFramebufferResized = false;
};

inline VkInstance VulkanBaseApp::GetVkInstance() const
{
    return m_Instance.GetVkInstance();
}

inline VkPhysicalDevice VulkanBaseApp::GetVkPhysicalDevice() const
{
    return m_PhysicalDevice;
}

inline VkDevice VulkanBaseApp::GetVkDevice() const
{
    return m_Device;
}

inline Vk::QueueFamilyIndices VulkanBaseApp::GetQueueFamilyIndices() const
{
    return m_QueueFamilyndices;
}

inline VkQueue VulkanBaseApp::GetVkPresentQueue() const
{
    return m_PresentQueue;
}

inline VkQueue VulkanBaseApp::GetVkGraphicsQueue() const
{
    return m_GraphicsQueue;
}

inline uint32_t VulkanBaseApp::GetSwapChainImageCount() const
{
    return (uint32_t)m_SwapChain.GetImages().size();
}

inline VkRenderPass VulkanBaseApp::GetVkRenderPass() const
{
    return m_RenderPass;
}

inline VkCommandPool VulkanBaseApp::GetCommandPool() const
{
    return m_CommandPool;
}

inline VkFormat VulkanBaseApp::GetVkSwapchainImageFormat() const
{
    return m_SwapChain.GetSurfaceFormat().format;
}

inline VkSwapchainKHR VulkanBaseApp::GetVkSwapChain() const
{
    return m_SwapChain.GetVkSwapChain();
}

inline VkExtent2D VulkanBaseApp::GetSwapChainExtend() const
{
    return m_SwapChain.GetExtent();
}

inline const std::vector<VkImage>& VulkanBaseApp::GetSwapChainImages() const
{
    return m_SwapChain.GetImages();
}

inline const std::vector<VkImageView>& VulkanBaseApp::GetSwapChainImageViews() const
{
    return m_SwapChain.GetVkImageViews();
}

inline uint32_t VulkanBaseApp::GetMaxFramesInFlight() const
{
    return m_MaxFramesInFlight;
}