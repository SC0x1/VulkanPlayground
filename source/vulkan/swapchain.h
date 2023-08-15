#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

typedef struct _SwapChainBuffers
{
    VkImage image;
    VkImageView view;
} SwapChainBuffer;

class SwapChain
{
public:
    SwapChain();
    ~SwapChain();

    void Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t preferredDimensions[2]);
    VkResult AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
    VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

    std::vector<VkFramebuffer> CreateFramebuffers(VkRenderPass renderPass);

    void Rebuild(uint32_t preferredDimensions[2]);

    void Destroy();

    VkSwapchainKHR GetSwapchain() const;

    VkSurfaceFormatKHR GetSurfaceFormat() const;
    VkPresentModeKHR GetPresentMode() const;

    VkExtent2D GetExtent() const;

    uint32_t GetImageCount() const;

    const VkSurfaceCapabilitiesKHR& GetSurfaceCapabilities() const;

    SwapChainBuffer GetSwapChainBuffer(uint32_t imageIndex) const;

    const std::vector<VkImage>& GetImages() const;
    const std::vector<VkImageView>& GetImageViews() const;

private:

    void CreateSwapchain();

    VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };

    VkSurfaceKHR m_Surface{nullptr};

    VkExtent2D m_Extent{0, 0};
    uint32_t m_ImageCount = 0;

    VkSurfaceFormatKHR m_SurfaceFormat;
    VkPresentModeKHR m_PresentMode;

    VkSurfaceCapabilitiesKHR m_Capabilities;

    std::vector<VkImage> m_Images;
    std::vector<VkImageView> m_ImageViews;

    // Context related
    VkDevice m_Device{ VK_NULL_HANDLE };
    VkPhysicalDevice m_PhysicalDevice{ VK_NULL_HANDLE };
};

inline VkSwapchainKHR SwapChain::GetSwapchain() const
{
    return m_Swapchain;
}

inline VkSurfaceFormatKHR SwapChain::GetSurfaceFormat() const
{
    return m_SurfaceFormat;
}

inline VkPresentModeKHR SwapChain::GetPresentMode() const
{
    return m_PresentMode;
}

inline VkExtent2D SwapChain::GetExtent() const
{
    return m_Extent;
}

inline uint32_t SwapChain::GetImageCount() const
{
    return m_ImageCount;
}

inline const VkSurfaceCapabilitiesKHR& SwapChain::GetSurfaceCapabilities() const
{
    return m_Capabilities;
}

inline const std::vector<VkImage>& SwapChain::GetImages() const
{
    return m_Images;
}

inline const std::vector<VkImageView>& SwapChain::GetImageViews() const
{
    return m_ImageViews;
}

//////////////////////////////////////////////////////////////////////////
// SwapchainSupportDetails
/// 

struct SwapChainDetails final
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::vector<VkPresentModeKHR> presentModes;
};

class SwapchainSupportDetails final
{
public:
    SwapchainSupportDetails(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

    const SwapChainDetails& GetSwapChainDetails() const;

    VkSurfaceFormatKHR GetOptimalSurfaceFormat() const;

    VkPresentModeKHR GetOptimalPresentMode() const;

    VkExtent2D GetOptimalExtent(uint32_t preferredDimensions[2]) const;

    uint32_t GetOptimalImageCount() const;

    VkSurfaceCapabilitiesKHR GetCapabilities() const;

    const std::vector<VkSurfaceFormatKHR>& GetSurfaceFormats() const;

    const std::vector<VkPresentModeKHR>& GetPresentModes() const;

    // Utils
    static SwapChainDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t preferredDimensions[2]);

private:

    SwapChainDetails m_Details;
};

inline const SwapChainDetails& SwapchainSupportDetails::GetSwapChainDetails() const
{
    return m_Details;
}

inline VkSurfaceCapabilitiesKHR SwapchainSupportDetails::GetCapabilities() const
{
    return m_Details.capabilities;
}

inline const std::vector<VkSurfaceFormatKHR>& SwapchainSupportDetails::GetSurfaceFormats() const
{
    return m_Details.surfaceFormats;
}

inline const std::vector<VkPresentModeKHR>& SwapchainSupportDetails::GetPresentModes() const
{
    return m_Details.presentModes;
}

vkEND_NAMESPACE