#pragma once

#include "vulkan/swapchaindetails.h"

vkBEGIN_NAMESPACE

//////////////////////////////////////////////////////////////////////////
// SwapchainSupportDetails
/// 

SwapchainSupportDetails::SwapchainSupportDetails(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    m_Details = QuerySwapChainSupport(physicalDevice, surface);
}

VkSurfaceFormatKHR SwapchainSupportDetails::GetOptimalSurfaceFormat(VkSurfaceFormatKHR preferedOptimalFormat) const
{
    return ChooseSwapSurfaceFormat(m_Details.surfaceFormats, preferedOptimalFormat);
}

VkSurfaceFormatKHR SwapchainSupportDetails::GetOptimalSurfaceFormat(const SurfaceFormatsRequest& sfRequest) const
{
    return ChooseSwapSurfaceFormat(m_Details.surfaceFormats, sfRequest);
}

VkPresentModeKHR SwapchainSupportDetails::GetOptimalPresentMode() const
{
    return ChooseSwapPresentMode(m_Details.presentModes);
}

VkExtent2D SwapchainSupportDetails::GetOptimalExtent(uint32_t preferredDimensions[2]) const
{
    return ChooseSwapExtent(m_Details.capabilities, preferredDimensions);
}

SwapChainDetails SwapchainSupportDetails::QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    SwapChainDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.surfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.surfaceFormats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

uint32_t SwapchainSupportDetails::GetOptimalImageCount() const
{
    const VkSurfaceCapabilitiesKHR& capabilities = m_Details.capabilities;

    // Simply sticking to this minimum means that we may sometimes have to wait on
    // the driver to complete internal operations before we can acquire another image to render to.
    // Therefore it is recommended to request at least one more image than the minimum
    uint32_t imageCount = capabilities.minImageCount + 1;

    // We should also make sure to not exceed the maximum number of images while doing this,
    // where 0 is a special value that means that there is no maximum
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    return imageCount;
}

VkSurfaceFormatKHR SwapchainSupportDetails::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats, VkSurfaceFormatKHR requestFormatKHR)
{
    for (auto format : availableFormats)
    {
        if (format.format == requestFormatKHR.format &&
            format.colorSpace == requestFormatKHR.colorSpace)
        {
            return format;
        }
    }
    return availableFormats[0];
}

VkSurfaceFormatKHR SwapchainSupportDetails::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats, const SurfaceFormatsRequest& sfRequest)
{
    const uint32_t availCount = (uint32_t)availableFormats.size();

    if (availCount == 1)
    {
        // First check if only one format, VK_FORMAT_UNDEFINED,
        // is available, which would imply that any format is available

        if (availableFormats[0].format == VK_FORMAT_UNDEFINED &&
            sfRequest.formatsCount > 0)
        {
            VkSurfaceFormatKHR ret;
            ret.format = sfRequest.formats[0];
            ret.colorSpace = sfRequest.colorSpace;
            return ret;
        }
    }
    else
    {
        // Request several formats, the first found will be used
        for (uint32_t request_i = 0; request_i < sfRequest.formatsCount; request_i++)
        {
            for (uint32_t avail_i = 0; avail_i < availCount; avail_i++)
            {
                if (availableFormats[avail_i].format == sfRequest.formats[request_i] &&
                    availableFormats[avail_i].colorSpace == sfRequest.colorSpace)
                {
                    return availableFormats[avail_i];
                }
            }
        }
    }

    // If none of the requested image formats could be found, use the first available
    return availableFormats[0];
}

VkPresentModeKHR SwapchainSupportDetails::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    // VK_PRESENT_MODE_MAILBOX_KHR is a very nice trade-off if energy usage is not
    // a concern. It allows us to avoid tearing while still maintaining a fairly
    // low latency by rendering new images that are as up-to-date as possible
    // right until the vertical blank.
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainSupportDetails::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t preferredDimensions[2])
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    auto width = std::max(std::min(preferredDimensions[0],
        capabilities.maxImageExtent.width),
        capabilities.minImageExtent.width);

    auto height = std::max(std::min(preferredDimensions[1],
        capabilities.maxImageExtent.height),
        capabilities.minImageExtent.height);

    return VkExtent2D{ width, height };

    //if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    //{
    //    return capabilities.currentExtent;
    //}
    //else
    //{
    //    // Get the resolution of the surface in pixels
    //    int width, height;
    //    glfwGetFramebufferSize(m_Window, &width, &height);

    //    VkExtent2D actualExtent =
    //    {
    //        static_cast<uint32_t>(width),
    //        static_cast<uint32_t>(height)
    //    };

    //    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    //    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    //    return actualExtent;
}

vkEND_NAMESPACE