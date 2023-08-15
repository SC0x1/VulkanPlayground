
#include "vulkan/swapchain.h"

vkBEGIN_NAMESPACE

void SwapChain::Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t preferredDimensions[2])
{
    m_Device = device;
    m_PhysicalDevice = physicalDevice;
    m_Surface = surface;

    const SwapchainSupportDetails supportDetails(physicalDevice, surface);

    m_Capabilities = supportDetails.GetCapabilities();
    m_SurfaceFormat = supportDetails.GetOptimalSurfaceFormat();
    m_PresentMode = supportDetails.GetOptimalPresentMode();
    m_Extent = supportDetails.GetOptimalExtent(preferredDimensions);
    m_ImageCount = supportDetails.GetOptimalImageCount();

    CreateSwapchain();
}

void SwapChain::CreateSwapchain()
{
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_Surface;
    createInfo.minImageCount = m_ImageCount;
    createInfo.imageFormat = m_SurfaceFormat.format;
    createInfo.imageColorSpace = m_SurfaceFormat.colorSpace;
    createInfo.imageExtent = m_Extent;
    createInfo.imageArrayLayers = 1;
    // The imageArrayLayers specifies the amount of layers each image consists of.
    // This is always 1 unless you are developing a stereoscopic 3D application.
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const Vk::QueueFamilyIndices indices = Vk::Utils::FindQueueFamilies(m_PhysicalDevice, m_Surface);
    const uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        // VK_SHARING_MODE_CONCURRENT
        // Images can be used across multiple queue families without explicit ownership transfers.
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        // VK_SHARING_MODE_EXCLUSIVE
        // An image is owned by one queue family at a time and ownership must be explicitly
        // transferred before using it in another queue family. This option offers the best performance.
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    // We can specify that a certain transform should be applied to images in the swap chain if it is supported
    // (supportedTransforms in capabilities), like a 90 degree clockwise rotation or horizontal flip
    createInfo.preTransform = m_Capabilities.currentTransform;

    // The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system. 
    // You'll almost always want to simply ignore the alpha channel
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = m_PresentMode;
    // If the clipped member is set to VK_TRUE then that means that we don't care about the color
    // of pixels that are obscured, for example because another window is in front of them.
    // Unless you really need to be able to read these pixels back and get predictable results,
    // you'll get the best performance by enabling clipping
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &m_ImageCount, nullptr);
    m_Images.resize(m_ImageCount);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &m_ImageCount, m_Images.data());

    m_ImageViews.resize(m_Images.size());

    for (size_t i = 0; i < m_Images.size(); i++)
    {
        m_ImageViews[i] = Vk::Utils::CreateImageView2D(
            m_Device, m_Images[i], m_SurfaceFormat.format,
            VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    //We now have a set of images that can be drawn onto and can be presented to the window
}

std::vector<VkFramebuffer> SwapChain::CreateFramebuffers(VkRenderPass renderPass)
{
    std::vector<VkFramebuffer> framebuffers(m_ImageViews.size());

    for (size_t i = 0; i < m_ImageViews.size(); i++)
    {
        VkImageView attachments[] = { m_ImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_Extent.width;
        framebufferInfo.height = m_Extent.height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &framebuffers[i]));
    }

    return framebuffers;
}

void SwapChain::Rebuild(uint32_t preferredDimensions[2])
{
    Destroy();

    SwapchainSupportDetails details{ m_PhysicalDevice, m_Surface };
    m_Extent = details.GetOptimalExtent(preferredDimensions);

    CreateSwapchain();
}

void SwapChain::Destroy()
{
    for (auto imageView : m_ImageViews)
    {
        vkDestroyImageView(m_Device, imageView, nullptr);
    }

    m_ImageViews.clear();

    if (m_Swapchain)
    {
        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    }
    m_Swapchain = nullptr;
}

SwapChain::SwapChain()
{

}

SwapChain::~SwapChain()
{
    Destroy();
}

SwapChainBuffer SwapChain::GetSwapChainBuffer(uint32_t imageIndex) const
{
    return { m_Images[imageIndex], m_ImageViews[imageIndex] };
}

VkResult SwapChain::AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
{
    return vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, presentCompleteSemaphore, VK_NULL_HANDLE, imageIndex);
}

VkResult SwapChain::QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    if (waitSemaphore != VK_NULL_HANDLE)
    {
        // The first two parameters specify which semaphores to wait on
        // before presentation can happen, just like VkSubmitInfo.
        // Since we want to wait on the command buffer to finish execution,
        // thus our triangle being drawn, we take the semaphores which will be
        // signaled and wait on them.
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &waitSemaphore;
    }

    presentInfo.pNext = nullptr;
    presentInfo.swapchainCount = 1;
    // The next two parameters specify the swap chains to present images to
    // and the index of the image for each swap chain.
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;
    // There is one last optional parameter called pResults.
    // It allows you to specify an array of VkResult values
    // to check for every individual swap chain if presentation was successful.
    // It's not necessary if you're only using a single swap chain,
    // because you can simply use the return value of the present function.
    presentInfo.pResults = nullptr; // Optional

    // The vkQueuePresentKHR function submits the request to present an image to the swap chain.
    return vkQueuePresentKHR(queue, &presentInfo);
}

//////////////////////////////////////////////////////////////////////////
// SwapchainSupportDetails
/// 

SwapchainSupportDetails::SwapchainSupportDetails(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    m_Details = QuerySwapChainSupport(physicalDevice, surface);
}

VkSurfaceFormatKHR SwapchainSupportDetails::GetOptimalSurfaceFormat() const
{
    return ChooseSwapSurfaceFormat(m_Details.surfaceFormats);
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

VkSurfaceFormatKHR SwapchainSupportDetails::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (auto format : availableFormats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB/*VK_FORMAT_B8G8R8A8_UNORM*/ &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
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

    auto width = std::max(std::min(preferredDimensions[0], capabilities.maxImageExtent.width),
        capabilities.minImageExtent.width);

    auto height = std::max(std::min(preferredDimensions[1], capabilities.maxImageExtent.height),
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