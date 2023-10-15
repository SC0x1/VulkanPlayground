#pragma once

#include "vulkan/utils.h"
#include "vulkan/swapchaindetails.h"

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

    void Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
        uint32_t preferredDimensions[2], VkSwapchainKHR oldSwapchain = nullptr);

    void Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
        uint32_t preferredDimensions[2], const SurfaceFormatsRequest& sfRequest,
        VkSwapchainKHR oldSwapchain = nullptr);

    void Create(const SwapchainSupportDetails& details, VkSurfaceKHR surface,
        uint32_t preferredDimensions[2], const SurfaceFormatsRequest& sfRequest,
        VkSwapchainKHR oldSwapchain = nullptr);

    VkResult AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
    VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

    std::vector<VkFramebuffer> CreateFramebuffers(VkRenderPass renderPass);

    void Rebuild(uint32_t preferredDimensions[2]);

    void Destroy();

    VkSwapchainKHR GetVkSwapChain() const;

    VkSurfaceFormatKHR GetSurfaceFormat() const;
    VkPresentModeKHR GetPresentMode() const;

    VkExtent2D GetExtent() const;

    uint32_t GetImageCount() const;

    const VkSurfaceCapabilitiesKHR& GetSurfaceCapabilities() const;

    SwapChainBuffer GetSwapChainBuffer(uint32_t imageIndex) const;

    const std::vector<VkImage>& GetImages() const;
    const std::vector<VkImageView>& GetVkImageViews() const;

private:

    void CreateSwapchain();

    VkSwapchainKHR m_SwapChain{ VK_NULL_HANDLE };
    VkSwapchainKHR m_OldSwapchain{ VK_NULL_HANDLE };

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

inline VkSwapchainKHR SwapChain::GetVkSwapChain() const
{
    return m_SwapChain;
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

inline const std::vector<VkImageView>& SwapChain::GetVkImageViews() const
{
    return m_ImageViews;
}

vkEND_NAMESPACE