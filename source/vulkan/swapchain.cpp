
#include "vulkan/swapchain.h"

vkBEGIN_NAMESPACE

void SwapChain::Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    uint32_t preferredDimensions[2], VkSwapchainKHR oldSwapchain)
{
    m_Device = device;
    m_PhysicalDevice = physicalDevice;
    m_Surface = surface;
    m_OldSwapchain = oldSwapchain;

    const SwapchainSupportDetails supportDetails(physicalDevice, surface);

    m_Capabilities = supportDetails.GetCapabilities();
    m_SurfaceFormat = supportDetails.GetOptimalSurfaceFormat();
    m_PresentMode = supportDetails.GetOptimalPresentMode();
    m_Extent = supportDetails.GetOptimalExtent(preferredDimensions);
    m_ImageCount = supportDetails.GetOptimalImageCount();

    CreateSwapchain();
}

void SwapChain::Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    uint32_t preferredDimensions[2], const SurfaceFormatsRequest& sfRequest,
    VkSwapchainKHR oldSwapchain)
{
    m_Device = device;
    m_PhysicalDevice = physicalDevice;
    m_Surface = surface;
    m_OldSwapchain = oldSwapchain;

    const SwapchainSupportDetails supportDetails(physicalDevice, surface);

    m_Capabilities = supportDetails.GetCapabilities();
    m_SurfaceFormat = supportDetails.GetOptimalSurfaceFormat(sfRequest);
    m_PresentMode = supportDetails.GetOptimalPresentMode();
    m_Extent = supportDetails.GetOptimalExtent(preferredDimensions);
    m_ImageCount = supportDetails.GetOptimalImageCount();

    CreateSwapchain();
}

void SwapChain::Create(const SwapchainSupportDetails& details, VkSurfaceKHR surface,
    uint32_t preferredDimensions[2], const SurfaceFormatsRequest& sfRequest,
    VkSwapchainKHR oldSwapchain)
{
    m_Capabilities = details.GetCapabilities();
    m_SurfaceFormat = details.GetOptimalSurfaceFormat(sfRequest);
    m_PresentMode = details.GetOptimalPresentMode();
    m_Extent = details.GetOptimalExtent(preferredDimensions);
    m_ImageCount = details.GetOptimalImageCount();

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

    createInfo.oldSwapchain = m_OldSwapchain;

    VkResult result;
    result = vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain);
    VK_CHECK(result);

    result = vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &m_ImageCount, nullptr);
    VK_CHECK(result);

    m_Images.resize(m_ImageCount);

    result = vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &m_ImageCount, m_Images.data());
    VK_CHECK(result);

    m_ImageViews.resize(m_Images.size());

    for (uint32_t i = 0; i < m_Images.size(); i++)
    {
        m_ImageViews[i] = Vk::Utils::CreateImageView2D(m_Device, m_Images[i],
            m_SurfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    if (m_OldSwapchain)
    {
        vkDestroySwapchainKHR(m_Device, m_OldSwapchain, nullptr);
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
    for (uint32_t i = 0; i < m_ImageViews.size(); ++i)
    {
        VkImageView imageView = m_ImageViews[i];
        vkDestroyImageView(m_Device, imageView, nullptr);
    }

    //m_ImageViews.clear();
    m_ImageViews.resize(0);

    if (m_SwapChain)
    {
        vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
    }
    m_SwapChain = nullptr;
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
    return vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX, presentCompleteSemaphore, VK_NULL_HANDLE, imageIndex);
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
    presentInfo.pSwapchains = &m_SwapChain;
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

vkEND_NAMESPACE