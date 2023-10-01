#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

//////////////////////////////////////////////////////////////////////////
// SwapchainSupportDetails
/// 

struct SwapChainDetails final
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct SurfaceFormatsRequest
{
    const VkFormat* formats = nullptr;
    uint32_t formatsCount = 0;
    VkColorSpaceKHR colorSpace;
};

class SwapchainSupportDetails final
{
public:
    SwapchainSupportDetails(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

    const SwapChainDetails& GetSwapChainDetails() const;

    VkSurfaceFormatKHR GetOptimalSurfaceFormat(VkSurfaceFormatKHR preferedOptimalFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) const;
    
    VkSurfaceFormatKHR GetOptimalSurfaceFormat(const SurfaceFormatsRequest& sfRequest) const;

    VkPresentModeKHR GetOptimalPresentMode() const;

    VkExtent2D GetOptimalExtent(uint32_t preferredDimensions[2]) const;

    uint32_t GetOptimalImageCount() const;

    VkSurfaceCapabilitiesKHR GetCapabilities() const;

    const std::vector<VkSurfaceFormatKHR>& GetSurfaceFormats() const;

    const std::vector<VkPresentModeKHR>& GetPresentModes() const;

    // Utils
    static SwapChainDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    
    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats, VkSurfaceFormatKHR requestFormat);
    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats, const SurfaceFormatsRequest& sfRequest);

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