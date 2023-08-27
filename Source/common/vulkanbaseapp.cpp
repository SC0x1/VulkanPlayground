#include "VKPlayground_PCH.h"

#include "vulkan/config.h"
#include "vulkanbaseapp.h"

#define GLM_FORCE_RADIANS // make sure that we are using radians
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        // Message is important enough to show
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        int hit = 0;
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            hit = 1;
        }
    }

    return VK_FALSE;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

VulkanBaseApp::VulkanBaseApp()
{

}

VulkanBaseApp::~VulkanBaseApp()
{

}

void VulkanBaseApp::Run()
{
    SetCurrentDirectory();

    InitWindow();

    OnInitialize();

    MainLoop();

    OnCleanup();
}

void VulkanBaseApp::OnInitialize()
{
    CreateVulkanInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();

    // Create SwapChain
    int width, height;
    glfwGetWindowSize(m_Window, &width, &height);
    uint32_t windowSize[2] = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    m_Swapchain.Create(m_Device, m_PhysicalDevice, m_Surface.GetVkSurface(), windowSize);
    m_MaxFramesInFlight = m_Swapchain.GetImageCount();

    CreateRenderPass();

    CreateCommandPool();
    CreateColorResources();
    CreateDepthResources();
    CreateFramebuffers();

    m_FramesInFlight.Initialize(m_Device, m_MaxFramesInFlight);
    m_ImagesInFlight.resize(m_MaxFramesInFlight, VK_NULL_HANDLE);
}

void VulkanBaseApp::MainLoop()
{
    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();

        OnRender();

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (m_Device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_Device);
    }
}

//BOOKMARK: VulkanBaseApp::OnCleanup
void VulkanBaseApp::OnCleanup()
{
    //Vulkan
    {
        CleanupSwapChain();

        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
        }

        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

        // When the command pool is freed, the command buffers are also freed
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

        m_FramesInFlight.Destroy();

        vkDestroyDevice(m_Device, nullptr);

        if (m_EnableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
        }

        m_Surface.Destroy(m_Instance);

        vkDestroyInstance(m_Instance, nullptr);
    }

    //GLFW
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }
}

void VulkanBaseApp::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
}

void VulkanBaseApp::PresentFrame(Vk::SyncObject syncObject, uint32_t frameIdx, uint32_t imageIndex)
{
    //////////////////////////////////////////////////////////////////////////
    // Submitting the command buffer

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // Queue submission and synchronization is configured through
    // parameters in the VkSubmitInfo structure.
    
    VkSemaphore waitSemaphores[] = { syncObject.imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkCommandBuffer imGuiCommandBuffer = m_ImGuiLayer.GetCommandBuffer(frameIdx);
    std::array<VkCommandBuffer, 2> cmdBuffers = { m_CommandBuffers[frameIdx], imGuiCommandBuffer };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    // The first three parameters specify which semaphores to wait on before
    // execution begins and in which stage(s) of the pipeline to wait.
    // We want to wait with writing colors to the image until it's available,
    // so we're specifying the stage of the graphics pipeline that writes to the
    // color attachment. That means that theoretically the implementation can
    // already start executing our vertex shader and such while the image is not
    // yet available. Each entry in the waitStages array corresponds to the
    // semaphore with the same index in pWaitSemaphores.

    // The next two parameters specify which command buffers to actually submit
    // for execution.
    submitInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
    submitInfo.pCommandBuffers = cmdBuffers.data();

    VkSemaphore signalSemaphores[] = { syncObject.renderFinishedSemaphore };

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    // The signalSemaphoreCount and pSignalSemaphores parameters specify
    // which semaphores to signal once the command buffer(s)
    // have finished execution. In our case we're using
    // the m_RenderFinishedSemaphore for that purpose.

    if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, syncObject.fence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    // The function takes an array of VkSubmitInfo structures as argument for
    // efficiency when the workload is much larger.
    // The last parameter references an optional fence that will be signaled
    // when the command buffers finish execution.
    // This allows us to know when it is safe for the command buffer to be
    // reused, thus we want to give it inFlightFence.
    // Now on the next frame, the CPU will wait for this command buffer to finish
    // executing before it records new commands into it.

    //////////////////////////////////////////////////////////////////////////
    // Subpass dependencies
    // 
    // The subpasses in a render pass automatically take care of image layout
    // transitions. These transitions are controlled by subpass dependencies,
    // which specify memory and execution dependencies between subpasses.
    // 
    // There are two built-in dependencies that take care of the transition
    // at the start of the render pass and at the end of the render pass,
    // but the former does not occur at the right time.
    // It assumes that the transition occurs at the start of the pipeline,
    // but we haven't acquired the image yet at that point!
    // 
    // There are two ways to deal with this problem.
    // We could change the waitStages for the imageAvailableSemaphore to
    // VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes
    // don't begin until the image is available, or we can make the render pass
    // wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage.

    //////////////////////////////////////////////////////////////////////////
    // Presentation
    // The last step of drawing a frame is submitting the result back to the swap chain
    // to have it eventually show up on the screen.
    VkResult result;
    result = m_Swapchain.QueuePresent(m_PresentQueue, imageIndex, syncObject.renderFinishedSemaphore);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_IsFramebufferResized)
    {
        m_IsFramebufferResized = false;
        OnRecreateSwapchain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
}

std::optional<uint32_t> VulkanBaseApp::AcquireNextImage(Vk::SyncObject syncObject)
{
    vkWaitForFences(m_Device, 1, &syncObject.fence, VK_TRUE, UINT64_MAX);

    uint32_t frameBufferImageIndex;
    const VkResult result = m_Swapchain.AcquireNextImage(syncObject.imageAvailableSemaphore, &frameBufferImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // *VK_ERROR_OUT_OF_DATE_KHR* The swap chain has become incompatible with the surface
        // and can no longer be used for rendering. Usually happens after a window resize.
        // It can be returned by The vkAcquireNextImageKHR and vkQueuePresentKHR functions.

        OnRecreateSwapchain();
        return std::nullopt;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK(result);

        // *VK_SUBOPTIMAL_KHR* The swap chain can still be used to successfully
        // present to the surface, but the surface properties are no longer matched exactly.
        // It can be returned by The vkAcquireNextImageKHR and vkQueuePresentKHR functions.

        throw std::runtime_error("failed to acquire swap chain image!");
    }

    return frameBufferImageIndex;
}

void VulkanBaseApp::CreateVulkanInstance()
{
    if (m_EnableValidationLayers && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Debug layer
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_EnableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();

        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    const auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
        std::cout << "available extensions:\n";
        for (const auto& extension : extensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }
}

void VulkanBaseApp::CreateSurface()
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    m_Surface.InitSurface(m_Instance, (void*)::GetModuleHandle(nullptr), glfwGetWin32Window(m_Window));
#else
    m_Surface.InitSurface(m_Instance, m_Window);
#endif
}

void VulkanBaseApp::SetupDebugMessenger()
{
    if (!m_EnableValidationLayers)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void VulkanBaseApp::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    // Use an ordered map to automatically sort candidates by increasing score
    std::multimap<int, VkPhysicalDevice> candidates;

    for (const auto& device : devices)
    {
        int score = RateDeviceSuitability(device);
        candidates.insert(std::make_pair(score, device));
    }


    // Check if the best candidate is suitable at all
    if (candidates.rbegin()->first > 0)
    {
        m_PhysicalDevice = candidates.rbegin()->second;

        m_MsaaSamples = GetMaxUsableSampleCount();
    }
    else
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void VulkanBaseApp::CreateLogicalDevice()
{
    m_QueFamilyndices = Vk::Utils::FindQueueFamilies(m_PhysicalDevice, m_Surface.GetVkSurface());

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies =
    {
        m_QueFamilyndices.graphicsFamily.value(),
        m_QueFamilyndices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE; // enable sample shading feature for the device

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

    if (m_EnableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    // The queues are automatically created along with the logical device
    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(m_Device, m_QueFamilyndices.graphicsFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_QueFamilyndices.presentFamily.value(), 0, &m_PresentQueue);
}

void VulkanBaseApp::OnRecreateSwapchain()
{
    int width = 0, height = 0;
    while (width == 0 || height == 0)
    {
        // Window minimization a special case
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_Device);

    uint32_t windowSize[2] = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
   
    m_Swapchain.Rebuild(windowSize);

    CreateColorResources();

    CreateDepthResources();

    // The framebuffers directly depend on the swap chain images,
    // and thus must be recreated as well.
    CreateFramebuffers();

    // We don't recreate the renderpass here for simplicity.
    // In theory it can be possible for the swap chain/ image format to change
    // during an applications' lifetime, e.g. when moving a window from an
    // standard range to an high dynamic range monitor. This may require the
    // application to recreate the renderpass to make sure the change between
    // dynamic ranges is properly reflected.
}

void VulkanBaseApp::CleanupSwapChain()
{
    m_ImagesInFlight.resize(m_MaxFramesInFlight, VK_NULL_HANDLE);

    // Cleanup Color image MSAA
    vkDestroyImageView(m_Device, m_ColorImageView, nullptr);
    vkDestroyImage(m_Device, m_ColorImage, nullptr);
    vkFreeMemory(m_Device, m_ColorImageMemory, nullptr);

    // Cleanup Depth
    vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
    vkDestroyImage(m_Device, m_DepthImage, nullptr);
    vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

    for (size_t i = 0; i < m_SwapChainFramebuffers.size(); i++)
    {
        vkDestroyFramebuffer(m_Device, m_SwapChainFramebuffers[i], nullptr);
    }

    m_Swapchain.Destroy();
}

void VulkanBaseApp::CreateRenderPass()
{
    const VkFormat colorFormat = m_Swapchain.GetSurfaceFormat().format;

    VkAttachmentDescription colorAttachment{};
    // The format of the color attachment should match the format of the swapchain images.
    colorAttachment.format = colorFormat;
    colorAttachment.samples = m_MsaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // The loadOp and storeOp determine what to do with the data in the attachment.
    // before rendering and after rendering. We have the following choices for loadOp:
    // *VK_ATTACHMENT_LOAD_OP_LOAD*
    //  Preserve the existing contents of the attachment.
    // *VK_ATTACHMENT_LOAD_OP_CLEAR*
    //  Clear the values to a constant at the start.
    // *VK_ATTACHMENT_LOAD_OP_DONT_CARE*
    //  Existing contents are undefined; we don't care about them.
    // In our case we're going to use the clear operation to clear the framebuffer.
    // to black before drawing a new frame.
    // There are only two possibilities for the storeOp:
    // *VK_ATTACHMENT_STORE_OP_STORE*
    //  Rendered contents will be stored in memory and can be read later.
    // *VK_ATTACHMENT_STORE_OP_DONT_CARE*
    //  Contents of the framebuffer will be undefined after the rendering operation.
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // The loadOp and storeOp apply to color and depth data,
    // and stencilLoadOp / stencilStoreOp apply to stencil data.
    // Our application won't do anything with the stencil buffer, so the results
    // of loading and storing are irrelevant.
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Some of the most common layouts are:
    // * VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*
    //   Images used as color attachment.
    // * VK_IMAGE_LAYOUT_PRESENT_SRC_KHR*
    //   Images to be presented in the swap chain.
    // * VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL*
    //   Images to be used as destination for a memory copy operation.
    // The initialLayout specifies which layout the image will have before the render pass begins.
    // The finalLayout specifies the layout to automatically transition to when the render pass finishes.
    // Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we don't care
    // what previous layout the image was in.

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // The attachment parameter specifies which attachment to reference by its index
    // in the attachment descriptions array. Our array consists of a single
    // VkAttachmentDescription, so its index is 0. The layout specifies which layout
    // we would like the attachment to have during a subpass that uses this reference.

    // MSAA
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = colorFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = m_IsImGuiEnabled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachmentResolve.finalLayout = finalLayout;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth Attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat(); // The format should be the same as the depth image itself
    depthAttachment.samples = m_MsaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // This time we don't care about storing the depth data (storeOp),
    // because it will not be used after drawing has finished.
    // This may allow the hardware to perform additional optimizations.
    // Just like the color buffer, we don't care about the previous depth contents,
    // so we can use VK_IMAGE_LAYOUT_UNDEFINED as initialLayout.

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    // Unlike color attachments, a subpass can only use a single depth (+stencil) attachment.

    // The index of the attachment in this array is directly referenced from the fragment shader with the
    //     layout(location = 0) out vec4 outColor directive!
    //
    // The following other types of attachments can be referenced by a subpass:
    //     pInputAttachments: Attachments that are read from a shader
    //     pResolveAttachments: Attachments used for multisampling color attachments
    //     pDepthStencilAttachment: Attachment for depth and stencil data
    //     pPreserveAttachments: Attachments that are not used by this subpass,
    // but for which the data must be preserved

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    // The first two fields specify the indices of the dependency and the dependent subpass.
    // The special value VK_SUBPASS_EXTERNAL refers to the implicit subpass before or after the render pass
    // depending on whether it is specified in srcSubpass or dstSubpass. The index 0 refers to our subpass,
    // which is the first and only one. The dstSubpass must always be higher than srcSubpass to prevent cycles
    // in the dependency graph (unless one of the subpasses is VK_SUBPASS_EXTERNAL).
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    // The next two fields specify the operations to wait on and the stages in which these operations occur.
    // We need to wait for the swap chain to finish reading from the image before we can access it.
    // This can be accomplished by waiting on the color attachment output stage itself.
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    // The operations that should wait on this are in the color attachment
    // stage and involve the writing of the color attachment.
    // The depth image is first accessed in the early fragment test pipeline
    // stage and because we have a load operation that clears,
    // we should specify the access mask for writes.

    std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanBaseApp::CreateFramebuffers()
{
    // The attachments specified during render pass creation are bound by wrapping them into a
    // VkFramebuffer object. A framebuffer object references all of the VkImageView objects
    // that represent the attachments.
    // That means that we have to create a framebuffer for all of the images in the swap chain and
    // use the one that corresponds to the retrieved image at drawing time.
    const std::vector<VkImageView>& swapchainImageViews = m_Swapchain.GetImageViews();
    m_SwapChainFramebuffers.resize(swapchainImageViews.size());
    const VkExtent2D swapchainExtent = m_Swapchain.GetExtent();

    for (size_t i = 0; i < swapchainImageViews.size(); i++)
    {
        std::array<VkImageView, 3> attachments =
        {
            m_ColorImageView,
            m_DepthImageView,
            swapchainImageViews[i],
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;
        // We first need to specify with which renderPass the framebuffer needs to be compatible.
        // You can only use a framebuffer with the render passes that it is compatible with,
        // which roughly means that they use the same number and type of attachments.
        // The attachmentCount and pAttachments parameters specify the VkImageView objects
        // that should be bound to the respective attachment descriptions in the
        // render pass pAttachment array.
        // (layers refers to the number of layers in image arrays)
        // The color attachment differs for every swap chain image, but the same depth image can be used by all of them
        // because only a single subpass is running at the same time due semaphores.

        if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VulkanBaseApp::CreateCommandPool()
{
    const Vk::QueueFamilyIndices queueFamilyIndices = Vk::Utils::FindQueueFamilies(m_PhysicalDevice, m_Surface.GetVkSurface());

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    // There are two possible flags for command pools:
    //     VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are
    // rerecorded with new commands very often (may change memory allocation behavior)
    //     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be
    // rerecorded individually, without this flag they all have to be reset together
    // Each command pool can only allocate command buffers that are submitted on a single type of queue.

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}

void VulkanBaseApp::CreateColorResources()
{
    const VkExtent2D swapchainExtend = m_Swapchain.GetExtent();
    const VkFormat colorFormat = m_Swapchain.GetSurfaceFormat().format;

    Vk::Utils::CreateImage2D(m_Device, m_PhysicalDevice, swapchainExtend.width, swapchainExtend.height,
        1, m_MsaaSamples, colorFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ColorImage, m_ColorImageMemory);

    m_ColorImageView = CreateImageView(m_ColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void VulkanBaseApp::CreateDepthResources()
{
    const VkExtent2D swapchainExtend = m_Swapchain.GetExtent();
    const VkFormat depthFormat = FindDepthFormat();

    Vk::Utils::CreateImage2D(m_Device, m_PhysicalDevice, swapchainExtend.width, swapchainExtend.height, 1, m_MsaaSamples, depthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory);

    m_DepthImageView = CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    // That's it for creating the depth image. We don't need to map it or copy another image to it,
    // because we're going to clear it at the start of the render pass like the color attachment.
}

VkCommandBuffer VulkanBaseApp::BeginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanBaseApp::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
}

VkImageView VulkanBaseApp::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) const
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    // viewType parameter allows you to treat images as 1D textures, 2D textures, 3D textures and cube maps
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    // The components field allows you to swizzle the color channels around.
    // For example, you can map all of the channels to the red channel for a monochrome texture.
    // You can also map constant values of 0 and 1 to a channel
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    // subresourceRange field describes what the image's purpose is and which part of the image should be accessed.
    // Our images will be used as color targets without any mipmapping levels or multiple layers
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    viewInfo.subresourceRange.aspectMask = aspectFlags;

    VkImageView imageView;
    if (vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void VulkanBaseApp::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);
}

void VulkanBaseApp::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    // Just like with buffer copies, you need to specify which part of the buffer
    // is going to be copied to which part of the image.
    // This happens through VkBufferImageCopy structs.

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };
    // Most of these fields are self-explanatory. The bufferOffset specifies the
    // byte offset in the buffer at which the pixel values start.
    // The bufferRowLength and bufferImageHeight fields specify how the pixels are
    // laid out in memory. For example, you could have some padding bytes between
    // rows of the image. Specifying 0 for both indicates that the pixels are
    // simply tightly packed like they are in our case.
    // The imageSubresource, imageOffset and imageExtent fields indicate to which
    // part of the image we want to copy the pixels.

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    // The fourth parameter indicates which layout the image is currently using.
    // I'm assuming here that the image has already been transitioned to the layout
    // that is optimal for copying pixels to.
    // Right now we're only copying one chunk of pixels to the whole image,
    // but it's possible to specify an array of VkBufferImageCopy to perform many
    // different copies from this buffer to the image in one operation.

    EndSingleTimeCommands(commandBuffer);
}

void VulkanBaseApp::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
    const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    // A pipeline barrier like that is generally used to synchronize access to resources,
    // like ensuring that a write to a buffer completes before reading from it,
    // but it can also be used to transition image layouts and transfer queue family
    // ownership when VK_SHARING_MODE_EXCLUSIVE is used.

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    // The first two fields specify layout transition. It is possible to use VK_IMAGE_LAYOUT_UNDEFINED as oldLayout
    // if you don't care about the existing contents of the image.
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // If you are using the barrier to transfer queue family ownership, then these two fields should be the indices of the queue families.
    // They must be set to VK_QUEUE_FAMILY_IGNORED if you don't want to do this (not the default value!).
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    // The image and subresourceRange specify the image that is affected and the specific part of the image.
    // Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.

    // You must specify which types of operations that involve the resource must happen before the barrier,
    // and which operations that involve the resource must wait on the barrier.

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    // The first parameter after the command buffer specifies in which pipeline
    // stage the operations occur that should happen before the barrier.
    // The second parameter specifies the pipeline stage in which operations will
    // wait on the barrier.
    // The third parameter is either 0 or VK_DEPENDENCY_BY_REGION_BIT.
    // The latter turns the barrier into a per-region condition.
    // That means that the implementation is allowed to already begin reading from
    // the parts of a resource that were written so far, for example.
    // The last three pairs of parameters reference arrays of pipeline barriers of
    // the three available types: memory barriers, buffer memory barriers,
    // and image memory barriers like the one we're using here.

    EndSingleTimeCommands(commandBuffer);
}

void VulkanBaseApp::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, imageFormat, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("texture image format does not support linear blitting!");

        // There are two alternatives in this case. You could implement a function
        // that searches common texture image formats for one that does support
        // linear blitting, or you could implement the mipmap generation in software
        // with a library like stb_image_resize. Each mip level can then be loaded
        // into the image in the same way that you loaded the original image.
    }

    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        // First, we transition level i - 1 to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
        // This transition will wait for level i - 1 to be filled, either from the
        // previous blit command, or from vkCmdCopyBufferToImage.

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        // Next, we specify the regions that will be used in the blit operation.
        // The source mip level is i - 1 and the destination mip level is i.
        // The two elements of the srcOffsets array determine the 3D region that
        // data will be blitted from.
        // dstOffsets determines the region that data will be blitted to.
        // The X and Y dimensions of the dstOffsets[1] are divided by two since
        // each mip level is half the size of the previous level.
        // The Z dimension of srcOffsets[1] and dstOffsets[1] must be 1,
        // since a 2D image has a depth of 1.

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);
        // Record the blit command
        //
        // Note that textureImage is used for both the srcImage and dstImage parameter.
        // This is because we're blitting between different levels of the same image.
        // The source mip level was just transitioned to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        // and the destination level is still in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        // This barrier transitions mip level i - 1 to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

        if (mipWidth > 1)
        {
            mipWidth /= 2;
        }

        if (mipHeight > 1)
        {
            mipHeight /= 2;
        }
        // handles cases where the image is not square,
        // since one of the mip dimensions would reach 1 before the other dimension
        // When this happens, that dimension should remain 1 for all remaining levels.
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
    // Before we end the command buffer, we insert one more pipeline barrier.
    // This barrier transitions the last mip level from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to
    // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    // This wasn't handled by the loop, since the last mip level is never blitted from.

    EndSingleTimeCommands(commandBuffer);
}

bool VulkanBaseApp::CheckValidationLayerSupport() const
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : m_ValidationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

bool VulkanBaseApp::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkSampleCountFlagBits VulkanBaseApp::GetMaxUsableSampleCount() const
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &physicalDeviceProperties);

    const VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

std::vector<const char*> VulkanBaseApp::GetRequiredExtensions() const
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (m_EnableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VkFormat VulkanBaseApp::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        // The VkFormatProperties struct contains three fields:
        //     linearTilingFeatures: Use cases that are supported with linear tiling
        //     optimalTilingFeatures: Use cases that are supported with optimal tiling
        //     bufferFeatures: Use cases that are supported for buffers

        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

VkFormat VulkanBaseApp::FindDepthFormat() const
{
    return FindSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    // Make sure to use the VK_FORMAT_FEATURE_
    // All of these candidate formats contain a depth component, but the latter two also contain a stencil component
}

int VulkanBaseApp::RateDeviceSuitability(VkPhysicalDevice physicalDvice) const
{
    int score = 0;
    // Checks the necessary queue family
    const Vk::QueueFamilyIndices indices = Vk::Utils::FindQueueFamilies(physicalDvice, m_Surface.GetVkSurface());

    if (indices.IsComplete() == false)
    {
        return 0;
    }

    const bool extensionsSupported = CheckDeviceExtensionSupport(physicalDvice);

    if (extensionsSupported == false)
    {
        return 0;
    }

    bool swapChainAdequate = false;
    const Vk::SwapChainDetails swapChainSupport = Vk::SwapchainSupportDetails::QuerySwapChainSupport(physicalDvice, m_Surface.GetVkSurface());
    swapChainAdequate = !swapChainSupport.surfaceFormats.empty() && !swapChainSupport.presentModes.empty();
    if (swapChainAdequate == false)
    {
        return 0;
    }

    // Basic physicalDvice properties like the name, type and supported Vulkan version
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDvice, &deviceProperties);

    // The support for optional features like texture compression,
    // 64 bit floats and multi viewport rendering (useful for VR)
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDvice, &deviceFeatures);

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        score += 1000;
    }
    else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    {
        score += 500;
    }
    else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
    {
        score += 200;
    }

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    // Application can't function without geometry shaders
    if (!deviceFeatures.geometryShader)
    {
        return 0;
    }

    // We require the device that has support of anisotropic filtering
    if (!deviceFeatures.samplerAnisotropy)
    {
        return 0;
    }

    return score;
}

void VulkanBaseApp::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<VulkanBaseApp*>(glfwGetWindowUserPointer(window));
    app->m_IsFramebufferResized = true;
}

void VulkanBaseApp::SetCurrentDirectory()
{
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring pathToExe(buffer);

    std::size_t posBuildInPath = pathToExe.find(L"build");
    if (posBuildInPath != std::string::npos)
    {
        _wchdir(pathToExe.substr(0, posBuildInPath).c_str());
    }
    else
    {
        std::wstring::size_type pos = pathToExe.find_last_of(L"\\/");
        _wchdir(pathToExe.substr(0, pos).c_str());
    }
#endif
}