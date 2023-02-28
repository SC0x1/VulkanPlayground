#include "vulkan_app.h"

#include <chrono>
#include <random>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <map>

#define GLM_FORCE_RADIANS // make sure that we are using radians
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

const std::string MODEL_PATH = "Models/viking_room.obj";
const std::string TEXTURE_PATH = "Textures/viking_room.png";

/*
    https://vulkan-tutorial.com/
*/

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

void ReadFile(const std::string& filename, std::vector<char>& buffer)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    buffer.resize(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
}

void VulkanApplication::Run()
{
    InitWindow();

    InitVulkan();

    MainLoop();

    Cleanup();
}

void VulkanApplication::InitVulkan()
{
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();

    CreateComputeDescriptorSetLayout();

    CreateGraphicsPipeline();
    CreateComputePipeline();
    CreateFramebuffers();

    CreateCommandPool();
    CreateShaderStorageBuffers();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateComputeDescriptorSets();
    CreateCommandBuffers();
    CreateComputeCommandBuffers();
    CreateSyncObjects();
}

void VulkanApplication::MainLoop()
{
    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();
        DrawFrame();

        // We want to animate the particle system using the last frames time to get smooth,
        // frame-rate independent animation
        const double currentTime = glfwGetTime();
        m_LastFrameTime = (currentTime - m_LastTime) * 1000.0f;
        m_LastTime = currentTime;

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    vkDeviceWaitIdle(m_Device);
}

void VulkanApplication::Cleanup()
{
    //Vulkan
    {
        CleanupSwapChain();

        vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

        vkDestroyPipeline(m_Device, m_ComputePipeline, nullptr);
        vkDestroyPipelineLayout(m_Device, m_ComputePipelineLayout, nullptr);

        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyBuffer(m_Device, m_ShaderStorageBuffers[i], nullptr);
            vkFreeMemory(m_Device, m_ShaderStorageBuffersMemory[i], nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
            vkFreeMemory(m_Device, m_UniformBuffersMemory[i], nullptr);
        }

        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(m_Device, m_ComputeDescriptorSetLayout, nullptr);

        // should be cleaned onto the main drawing function!
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_Device, m_ComputeFinishedSemaphores[i], nullptr);

            vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
            vkDestroyFence(m_Device, m_ComputeInFlightFences[i], nullptr);
        }

        // when the command pool is freed, the command buffers are also freed
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

        vkDestroyDevice(m_Device, nullptr);

        if (m_EnableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        vkDestroyInstance(m_Instance, nullptr);
    }

    //GLFW
    glfwDestroyWindow(m_Window);

    glfwTerminate();
}

void VulkanApplication::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
}

void VulkanApplication::CreateInstance()
{
    if (m_EnableValidationLayers && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Compute Application";
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

void VulkanApplication::CreateSurface()
{
    // Platform specific creation of the surface
    /* 
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = glfwGetWin32Window(m_Window);
        createInfo.hinstance = GetModuleHandle(nullptr);

        if (vkCreateWin32SurfaceKHR(m_Instance, &createInfo, nullptr, &m_Surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    */

    if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

void VulkanApplication::SetupDebugMessenger()
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

void VulkanApplication::PickPhysicalDevice()
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

void VulkanApplication::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsAndComputeFamily.value(), indices.presentFamily.value() };

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
    /*
        Previous implementations of Vulkan made a distinction between instance and device specific validation layers,
        but this is no longer the case. That means that the enabledLayerCount and ppEnabledLayerNames fields
        of VkDeviceCreateInfo are ignored by up-to-date implementations.
        However, it is still a good idea to set them anyway to be compatible with older implementations.
    */
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

    vkGetDeviceQueue(m_Device, indices.graphicsAndComputeFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, indices.graphicsAndComputeFamily.value(), 0, &m_ComputeQueue);
    vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void VulkanApplication::CreateSwapChain()
{
    const SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

    const VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    const VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    const VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

    /*
        Simply sticking to this minimum means that we may sometimes have to wait on
        the driver to complete internal operations before we can acquire another image to render to.
        Therefore it is recommended to request at least one more image than the minimum
    */
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    /*
        We should also make sure to not exceed the maximum number of images while doing this,
        where 0 is a special value that means that there is no maximum
    */
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    /*
        The imageArrayLayers specifies the amount of layers each image consists of.
        This is always 1 unless you are developing a stereoscopic 3D application
    */
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_Surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    const QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
    const uint32_t queueFamilyIndices[] = { indices.graphicsAndComputeFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsAndComputeFamily != indices.presentFamily)
    {
        // VK_SHARING_MODE_CONCURRENT
        // Images can be used across multiple queue families without explicit ownership transfers.
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        /*
        VK_SHARING_MODE_EXCLUSIVE
        An image is owned by one queue family at a time and ownership must be explicitly
        transferred before using it in another queue family. This option offers the best performance.
        */
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    // We can specify that a certain transform should be applied to images in the swap chain if it is supported
    // (supportedTransforms in capabilities), like a 90 degree clockwise rotation or horizontal flip
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

    // The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system. 
    // You'll almost always want to simply ignore the alpha channel
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    /*
        If the clipped member is set to VK_TRUE then that means that we don't care about the color
        of pixels that are obscured, for example because another window is in front of them.
        Unless you really need to be able to read these pixels back and get predictable results,
        you'll get the best performance by enabling clipping
    */
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
    m_SwapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

    m_SwapChainImageFormat = surfaceFormat.format;
    m_SwapChainExtent = extent;

    /*
        We now have a set of images that can be drawn onto and can be presented to the window
    */
}

void VulkanApplication::RecreateSwapChain()
{
    int width = 0, height = 0;
    while (width == 0 || height == 0)
    {
        /*
        window minimization a special case
        */
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_Device);

    CleanupSwapChain();

    CreateSwapChain();
    // The image views need to be recreated because they are based directly on the swap chain images.
    CreateImageViews();

    // The framebuffers directly depend on the swap chain images, and thus must be recreated as well.
    CreateFramebuffers();

    /*
    We don't recreate the renderpass here for simplicity. In theory it can be possible for the swap chain
    image format to change during an applications' lifetime, e.g. when moving a window from an standard range
    to an high dynamic range monitor. This may require the application to recreate the renderpass to make sure
    the change between dynamic ranges is properly reflected.
    */
}

void VulkanApplication::CleanupSwapChain()
{
    for (size_t i = 0; i < m_SwapChainFramebuffers.size(); i++)
    {
        vkDestroyFramebuffer(m_Device, m_SwapChainFramebuffers[i], nullptr);
    }

    for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
    {
        vkDestroyImageView(m_Device, m_SwapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
}

void VulkanApplication::CreateImageViews()
{
    m_SwapChainImageViews.resize(m_SwapChainImages.size());

    for (uint32_t i = 0; i < m_SwapChainImages.size(); i++)
    {
        m_SwapChainImageViews[i] = CreateImageView(m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

void VulkanApplication::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_SwapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanApplication::CreateComputeDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> layoutBindings{};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].pImmutableSamplers = nullptr;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    /*
        Note that you can combine shader stages here, so if you want the descriptor to be accessible
        from the vertex and compute stage, e.g. for a uniform buffer with parameters shared across them,
        you simply set the bits for both stages
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    */

    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].pImmutableSamplers = nullptr;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[2].pImmutableSamplers = nullptr;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_ComputeDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create compute descriptor set layout!");
    }
    /*
        Looking at this setup, you might wonder why we have two layout bindings for shader storage buffer objects,
        even though we'll only render a single particle system.
        This is because the particle positions are updated frame by frame based on a delta time.
        This means that each frame needs to know about the last frames' particle positions,
        so it can update them with a new delta time and write them to it's own SSBO
    */
}

void VulkanApplication::CreateGraphicsPipeline()
{
    std::vector<char> vertShaderCode;
    ReadFile("shaders/compute_vert.spv", vertShaderCode);
    std::vector<char>  fragShaderCode;
    ReadFile("shaders/compute_frag.spv", fragShaderCode);

    const VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    const VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    //////////////////////////////////////////////////////////////////////////
    // Input assembly
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Particle::GetBindingDescription();
    auto attributeDescriptions = Particle::GetAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    //////////////////////////////////////////////////////////////////////////
    // Viewports and scissors
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_SwapChainExtent.width;
    viewport.height = (float)m_SwapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_SwapChainExtent;

    std::vector<VkDynamicState> dynamicStates =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // For static states
    //VkPipelineViewportStateCreateInfo viewportState{};
    //viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    //viewportState.viewportCount = 1;
    //viewportState.pViewports = &viewport;
    //viewportState.scissorCount = 1;
    //viewportState.pScissors = &scissor;

    //////////////////////////////////////////////////////////////////////////
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    /*
        If depthClampEnable is set to VK_TRUE, then fragments that are beyond the near and far planes
        are clamped to them as opposed to discarding them
    */
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    /*
        If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through the rasterizer stage.
        This basically disables any output to the framebuffer
    */
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    /*
        VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
        VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
        VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
            Using any mode other than fill requires enabling a GPU feature.
    */
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    //////////////////////////////////////////////////////////////////////////
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE; // enable sample shading in the pipeline
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // m_MsaaSamples;

    //////////////////////////////////////////////////////////////////////////
    // Depth stencil
    //VkPipelineDepthStencilStateCreateInfo depthStencil{};
    //{
    //    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    //    depthStencil.depthTestEnable = VK_TRUE;  // field specifies if the depth of new fragments should be compared to the depth buffer to see if they should be discarded
    //    depthStencil.depthWriteEnable = VK_TRUE; // field specifies if the new depth of fragments that pass the depth test should actually be written to the depth buffer
    //    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    //    /*
    //    depthCompareOp field specifies the comparison that is performed to keep or discard fragments.
    //    We're sticking to the convention of lower depth = closer, so the depth of new fragments should be less.
    //    */
    //    depthStencil.depthBoundsTestEnable = VK_FALSE;
    //    depthStencil.minDepthBounds = 0.0f; // Optional
    //    depthStencil.maxDepthBounds = 1.0f; // Optional
    //    /*
    //    The depthBoundsTestEnable, minDepthBounds and maxDepthBounds fields are used for the optional depth bound test.
    //    Basically, this allows you to only keep fragments that fall within the specified depth range.
    //    */
    //    depthStencil.stencilTestEnable = VK_FALSE;
    //    depthStencil.front = {}; // Optional
    //    depthStencil.back = {}; // Optional
    //}

    //////////////////////////////////////////////////////////////////////////
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    /*
    This per-framebuffer struct allows you to configure the first way of color blending. The operations that will be performed are best demonstrated using the following pseudocode:

        if (blendEnable) {
            finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
            finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
        } else {
            finalColor = newColor;
        }

        finalColor = finalColor & colorWriteMask;

    The most common way to use color blending is to implement alpha blending, where we want the new color to be blended with the old color based on its opacity. The finalColor should then be computed as follows:

        finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
        finalColor.a = newAlpha.a;
        This can be accomplished with the following parameters:

        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    */

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional
    /*
        If you want to use the second method of blending (bitwise combination),
        then you should set logicOpEnable to VK_TRUE. The bitwise operation can then be specified in the logicOp field.
        Note that this will automatically disable the first method, as if you had set blendEnable to VK_FALSE for every
        attached framebuffer! The colorWriteMask will also be used in this mode to determine which channels in the
        framebuffer will actually be affected. It is also possible to disable both modes, as we've done here,
        in which case the fragment colors will be written to the framebuffer unmodified.
    */
    //////////////////////////////////////////////////////////////////////////
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
    /*
        You can use uniform values in shaders, which are globals similar to dynamic state variables that can be
        changed at drawing time to alter the behavior of your shaders without having to recreate them
    */
    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    // Then we reference all of the structures describing the fixed-function stage.
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    //pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = m_PipelineLayout;

    pipelineInfo.renderPass = m_RenderPass;
    pipelineInfo.subpass = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional
    /*
    There are actually two more parameters: basePipelineHandle and basePipelineIndex.
    Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
    The idea of pipeline derivatives is that it is less expensive to set up pipelines when
    they have much functionality in common with an existing pipeline and switching between
    pipelines from the same parent can also be done quicker. You can either specify the handle
    of an existing pipeline with basePipelineHandle or reference another pipeline that is about
    to be created by index with basePipelineIndex. 
    */

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
}

void VulkanApplication::CreateComputePipeline()
{
    std::vector<char> computeShaderCode;
    ReadFile("shaders/compute_comp.spv", computeShaderCode);

    VkShaderModule computeShaderModule = CreateShaderModule(computeShaderCode);

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_ComputeDescriptorSetLayout;

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_ComputePipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_ComputePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ComputePipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    vkDestroyShaderModule(m_Device, computeShaderModule, nullptr);
}

void VulkanApplication::CreateFramebuffers()
{
    /*
    The attachments specified during render pass creation are bound by wrapping them into a
    VkFramebuffer object. A framebuffer object references all of the VkImageView objects
    that represent the attachments.

    That means that we have to create a framebuffer for all of the images in the swap chain and
    use the one that corresponds to the retrieved image at drawing time.
    */
    m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());

    for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
    {
        VkImageView attachments[] =
        {
            m_SwapChainImageViews[i],
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_SwapChainExtent.width;
        framebufferInfo.height = m_SwapChainExtent.height;
        framebufferInfo.layers = 1;
        /*
            We first need to specify with which renderPass the framebuffer needs to be compatible. 
            You can only use a framebuffer with the render passes that it is compatible with, which roughly means
            that they use the same number and type of attachments.

            The attachmentCount and pAttachments parameters specify the VkImageView objects that should be bound
            to the respective attachment descriptions in the render pass pAttachment array.

            layers refers to the number of layers in image arrays

            The color attachment differs for every swap chain image, but the same depth image can be used by all of them
            because only a single subpass is running at the same time due semaphores.
        */
        if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VulkanApplication::CreateCommandPool()
{
    const QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value();
    /*
    There are two possible flags for command pools:
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
    Each command pool can only allocate command buffers that are submitted on a single type of queue.
    */
    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}

void VulkanApplication::CreateShaderStorageBuffers()
{
    // Initialize particles
    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    // Initial particle positions on a circle
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto& particle : particles)
    {
        const float r = 0.25f * sqrt(rndDist(rndEngine));
        const float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
        const float x = r * cosf(theta) * HEIGHT / WIDTH;
        const float y = r * sinf(theta);
        particle.position = glm::vec2(x, y);
        particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
        particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    }

    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

    // Create a staging buffer used to upload data to the GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    CreateBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, particles.data(), (size_t)bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    m_ShaderStorageBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_ShaderStorageBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

    // Copy initial particle data to all storage buffers
    /*
        Using this staging buffer as a source we then create the per-frame shader storage buffers
        and copy the particle properties from the staging buffer
    */
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_ShaderStorageBuffers[i], m_ShaderStorageBuffersMemory[i]);

        CopyBuffer(stagingBuffer, m_ShaderStorageBuffers[i], bufferSize);
    }

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void VulkanApplication::CreateUniformBuffers()
{
    const VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_UniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_UniformBuffers[i], m_UniformBuffersMemory[i]);

        vkMapMemory(m_Device, m_UniformBuffersMemory[i], 0, bufferSize, 0, &m_UniformBuffersMapped[i]);
    }
}

void VulkanApplication::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // UBO
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // Storage buffer
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    /*
    Inadequate descriptor pools are a good example of a problem that the validation layers will not catch:
    As of Vulkan 1.1, vkAllocateDescriptorSets may fail with the error code VK_ERROR_POOL_OUT_OF_MEMORY
    if the pool is not sufficiently large, but the driver may also try to solve the problem internally.
    This means that sometimes (depending on hardware, pool size and allocation size) the driver will let us
    get away with an allocation that exceeds the limits of our descriptor pool.
    Other times, vkAllocateDescriptorSets will fail and return VK_ERROR_POOL_OUT_OF_MEMORY.
    This can be particularly frustrating if the allocation succeeds on some machines, but fails on others.
    */

    if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanApplication::CreateComputeDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_ComputeDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    m_ComputeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_Device, &allocInfo, m_ComputeDescriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo uniformBufferInfo{};
        uniformBufferInfo.buffer = m_UniformBuffers[i];
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(UniformBufferObject);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_ComputeDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

        VkDescriptorBufferInfo storageBufferInfoLastFrame{};
        storageBufferInfoLastFrame.buffer = m_ShaderStorageBuffers[(i - 1) % MAX_FRAMES_IN_FLIGHT];
        storageBufferInfoLastFrame.offset = 0;
        storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_ComputeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

        VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
        storageBufferInfoCurrentFrame.buffer = m_ShaderStorageBuffers[i];
        storageBufferInfoCurrentFrame.offset = 0;
        storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_ComputeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;

        vkUpdateDescriptorSets(m_Device, 3, descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanApplication::CreateCommandBuffers()
{
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

    if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    /*
    The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
        VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
        VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers.
        (it's helpful to reuse common operations from primary command buffers.)
    */
}

void VulkanApplication::CreateComputeCommandBuffers()
{
    m_ComputeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_ComputeCommandBuffers.size();

    if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_ComputeCommandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate compute command buffers!");
    }
}

void VulkanApplication::CreateSyncObjects()
{
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_ComputeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_ComputeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    /*
    Create the fence in the signaled state, so that the first call to vkWaitForFences() returns immediately
    since the fence is already signaled. To do this, we add the VK_FENCE_CREATE_SIGNALED_BIT
    */

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create semaphores!");
        }

        if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ComputeFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Device, &fenceInfo, nullptr, &m_ComputeInFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute synchronization objects for a frame!");
        }
    }
}

void VulkanApplication::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) const
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    /*
    The flags parameter specifies how we're going to use the command buffer. The following values are available:
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
        VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
    The pInheritanceInfo parameter is only relevant for secondary command buffers.
    It specifies which state to inherit from the calling primary command buffers
    */
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_SwapChainFramebuffers[imageIndex];

    // Define the size of the render area
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_SwapChainExtent;

    VkClearValue clearColor{};
    clearColor.color = { {0.0f, 0.0f, 0.0f, 1.0f} };

    /*
    The range of depths in the depth buffer is 0.0 to 1.0 in Vulkan, where 1.0 lies at the far view plane and 0.0 at the near view plane.
    The initial value at each point in the depth buffer should be the furthest possible depth, which is 1.0.

    !!! Note that the order of clearColor should be identical to the order of your attachments.
    */

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        /*
        The first parameter for every command is always the command buffer to record the command to.
        The second parameter specifies the details of the render pass we've just provided.
        The final parameter controls how the drawing commands within the render pass will be provided.
        It can have one of two values:
            VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
            VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands will be executed from secondary command buffers.
        All of the functions that record commands can be recognized by their vkCmd prefix.
        */

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
        /*
        We've now told Vulkan which operations to execute in the graphics pipeline and
        which attachment to use in the fragment shader.
        */

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_SwapChainExtent.width);
        viewport.height = static_cast<float>(m_SwapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_SwapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        /*
        We did specify viewport and scissor state for this pipeline to be dynamic.
        So we need to set them in the command buffer before issuing our draw command.
        */

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_ShaderStorageBuffers[m_CurrentFrameIdx], offsets);

        vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);

        /*
        It has the following parameters, aside from the command buffer:
            vertexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
            instanceCount: Used for instanced rendering, use 1 if you're not doing that.
            firstVertex: Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
            firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
        */
    }
    vkCmdEndRenderPass(commandBuffer);
    
    // We've finished recording the command buffer
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void VulkanApplication::RecordComputeCommandBuffer(VkCommandBuffer commandBuffer) const
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording compute command buffer!");
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelineLayout, 0, 1, &m_ComputeDescriptorSets[m_CurrentFrameIdx], 0, nullptr);

    vkCmdDispatch(commandBuffer, PARTICLE_COUNT / 256, 1, 1);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record compute command buffer!");
    }
}

VkCommandBuffer VulkanApplication::BeginSingleTimeCommands()
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

void VulkanApplication::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
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

void VulkanApplication::UpdateUniformBuffer(uint32_t currentImage)
{
    UniformBufferObject ubo{};
    ubo.deltaTime = m_LastFrameTime * 2.0f;

    memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanApplication::DrawFrame()
{
    /*
    At a high level, rendering a frame in Vulkan consists of a common set of steps:
        Wait for the previous frame to finish
        Acquire an image from the swap chain
        Record a command buffer which draws the scene onto that image
        Submit the recorded command buffer
        Present the swap chain image
    
    Semaphores - GPU sync
    Fences - CPU -> GPU sync
     if the host(CPU) needs to know when the GPU has finished something, we use a fence.

    Semaphores are used to specify the execution order of operations on the GPU while
    fences are used to keep the CPU and GPU in sync with each-other.
    */

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Compute submission

    vkWaitForFences(m_Device, 1, &m_ComputeInFlightFences[m_CurrentFrameIdx], VK_TRUE, UINT64_MAX);

    UpdateUniformBuffer(m_CurrentFrameIdx);
    vkResetFences(m_Device, 1, &m_ComputeInFlightFences[m_CurrentFrameIdx]);

    vkResetCommandBuffer(m_ComputeCommandBuffers[m_CurrentFrameIdx], /*VkCommandBufferResetFlagBits*/ 0);
    RecordComputeCommandBuffer(m_ComputeCommandBuffers[m_CurrentFrameIdx]);

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_ComputeCommandBuffers[m_CurrentFrameIdx];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_ComputeFinishedSemaphores[m_CurrentFrameIdx];

    if (vkQueueSubmit(m_ComputeQueue, 1, &submitInfo, m_ComputeInFlightFences[m_CurrentFrameIdx]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit compute command buffer!");
    };

    //////////////////////////////////////////////////////////////////////////
    // Graphics submission

    vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrameIdx], VK_TRUE, UINT64_MAX);
    /*
    At the start of the frame, we want to wait until the previous frame has finished,
    so that the command buffer and semaphores are available to use.

    The vkWaitForFences function takes an array of fences and waits on the host for either any
    or all of the fences to be signaled before returning. The VK_TRUE we pass here indicates that
    we want to wait for all fences, but in the case of a single one it doesn't matter.
    This function also has a timeout parameter that we set to the maximum value of a 64 bit unsigned integer,
    UINT64_MAX, which effectively disables the timeout.
    */

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrameIdx], VK_NULL_HANDLE, &imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        /*
        VK_ERROR_OUT_OF_DATE_KHR: The swap chain has become incompatible with the surface and can no longer be used for rendering.
        Usually happens after a window resize. It can be returned by The vkAcquireNextImageKHR and vkQueuePresentKHR functions.
        */
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        /*
        VK_SUBOPTIMAL_KHR: The swap chain can still be used to successfully present to the surface,
        but the surface properties are no longer matched exactly.It can be returned by The vkAcquireNextImageKHR and vkQueuePresentKHR functions.
        */
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    /*
    The first two parameters of vkAcquireNextImageKHR are the logical device and the swap chain from which we wish to acquire an image.
    The third parameter specifies a timeout in nanoseconds for an image to become available.
    Using the maximum value of a 64 bit unsigned integer means we effectively disable the timeout.

    The next two parameters specify synchronization objects that are to be signaled when the presentation engine is finished using the image.
    That's the point in time where we can start drawing to it. It is possible to specify a semaphore, fence or both.
    We're going to use our imageAvailableSemaphore for that purpose here.

    The last parameter specifies a variable to output the index of the swap chain image that has become available.
    The index refers to the VkImage in our swapChainImages array. We're going to use that index to pick the VkFrameBuffer.
    */

    // After waiting, we need to manually reset the fence to the unsignaled state
    vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrameIdx]);

    //////////////////////////////////////////////////////////////////////////
    // Recording the command buffer

    vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrameIdx], 0);
    /*
    With the imageIndex specifying the swap chain image to use in hand, we can now record the command buffer.
    First, we call vkResetCommandBuffer on the command buffer to make sure it is able to be recorded.
    */

    // Now call the function recordCommandBuffer to record the commands we want.
    RecordCommandBuffer(m_CommandBuffers[m_CurrentFrameIdx], imageIndex);

    //////////////////////////////////////////////////////////////////////////
    // Submitting the command buffer

    /*
    Queue submission and synchronization is configured through parameters in the VkSubmitInfo structure.
    */

    VkSemaphore waitSemaphores[] = { m_ComputeFinishedSemaphores[m_CurrentFrameIdx], m_ImageAvailableSemaphores[m_CurrentFrameIdx] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 2;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    /*
    The first three parameters specify which semaphores to wait on before execution begins and in which stage(s) of the
    pipeline to wait. We want to wait with writing colors to the image until it's available, so we're specifying the stage of the
    graphics pipeline that writes to the color attachment. That means that theoretically the implementation can already start executing
    our vertex shader and such while the image is not yet available.
    Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores.
    */

    // The next two parameters specify which command buffers to actually submit for execution.
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrameIdx];

    VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrameIdx]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    /*
    The signalSemaphoreCount and pSignalSemaphores parameters specify which semaphores to signal once the command buffer(s)
    have finished execution. In our case we're using the m_RenderFinishedSemaphore for that purpose.
    */

    if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrameIdx]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    /*
     The function takes an array of VkSubmitInfo structures as argument for efficiency when the workload is much larger.
     The last parameter references an optional fence that will be signaled when the command buffers finish execution.
     This allows us to know when it is safe for the command buffer to be reused, thus we want to give it inFlightFence.
     Now on the next frame, the CPU will wait for this command buffer to finish executing before it records new commands into it.
    */

    //////////////////////////////////////////////////////////////////////////
    // Subpass dependencies
    /*
     the subpasses in a render pass automatically take care of image layout transitions.
     These transitions are controlled by subpass dependencies,
     which specify memory and execution dependencies between subpasses. 

     There are two built-in dependencies that take care of the transition at the start of the render pass
     and at the end of the render pass, but the former does not occur at the right time.
     It assumes that the transition occurs at the start of the pipeline, but we haven't acquired the image yet at that point!
     There are two ways to deal with this problem. We could change the waitStages for the
     imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes don't begin until the image is available,
     or we can make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage.
    */

    //////////////////////////////////////////////////////////////////////////
    // Presentation
    /*
    The last step of drawing a frame is submitting the result back to the swap chain to have it eventually show up on the screen. 
    */

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    /*
    The first two parameters specify which semaphores to wait on before presentation can happen, just like VkSubmitInfo.
    Since we want to wait on the command buffer to finish execution, thus our triangle being drawn, we take the semaphores which will be
    signaled and wait on them.
    */

    VkSwapchainKHR swapChains[] = { m_SwapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    /*
    The next two parameters specify the swap chains to present images to and the index of the image for each swap chain.
    */
    presentInfo.pResults = nullptr; // Optional
    /*
    There is one last optional parameter called pResults. It allows you to specify an array of VkResult values
    to check for every individual swap chain if presentation was successful.
    It's not necessary if you're only using a single swap chain, because you can simply use the return value of the present function.
    */

    result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_IsFamebufferResized)
    {
        m_IsFamebufferResized = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
    /*
    The vkQueuePresentKHR function submits the request to present an image to the swap chain.
    */

    m_CurrentFrameIdx = (m_CurrentFrameIdx + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkImageView VulkanApplication::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) const
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    // viewType parameter allows you to treat images as 1D textures, 2D textures, 3D textures and cube maps
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    /*
        The components field allows you to swizzle the color channels around.
        For example, you can map all of the channels to the red channel for a monochrome texture.
        You can also map constant values of 0 and 1 to a channel
    */
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    /*
        subresourceRange field describes what the image's purpose is and which part of the image should be accessed.
        Our images will be used as color targets without any mipmapping levels or multiple layers
    */
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

VkShaderModule VulkanApplication::CreateShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void VulkanApplication::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
}

void VulkanApplication::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);
}

void VulkanApplication::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    /*
    The usage field has the same semantics as the one during buffer creation.
    The image is going to be used as destination for the buffer copy, so it should be set up as a transfer destination.
    We also want to be able to access the image from the shader to color our mesh, so the usage should include VK_IMAGE_USAGE_SAMPLED_BIT
    */
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    /*
    The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
    */

    imageInfo.samples = numSamples;
    imageInfo.flags = 0; // Optional

    if (vkCreateImage(m_Device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(m_Device, image, imageMemory, 0);
}

void VulkanApplication::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    /*
    Just like with buffer copies, you need to specify which part of the buffer is going to be copied to which part of the image.
    This happens through VkBufferImageCopy structs.
    */
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
    /*
    Most of these fields are self-explanatory. The bufferOffset specifies the byte offset in the buffer at which the pixel values start.
    The bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory.
    For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates
    that the pixels are simply tightly packed like they are in our case.
    The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels.
    */

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
    /*
    The fourth parameter indicates which layout the image is currently using.
    I'm assuming here that the image has already been transitioned to the layout that is optimal for copying pixels to.
    Right now we're only copying one chunk of pixels to the whole image, but it's possible to specify an array of
    VkBufferImageCopy to perform many different copies from this buffer to the image in one operation.
    */
    EndSingleTimeCommands(commandBuffer);
}

void VulkanApplication::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
    const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    /*
    A pipeline barrier like that is generally used to synchronize access to resources,
    like ensuring that a write to a buffer completes before reading from it,
    but it can also be used to transition image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used.
    */
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    /*
    The first two fields specify layout transition. It is possible to use VK_IMAGE_LAYOUT_UNDEFINED as oldLayout
    if you don't care about the existing contents of the image.
    */
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    /*
    If you are using the barrier to transfer queue family ownership, then these two fields should be the indices of the queue families.
    They must be set to VK_QUEUE_FAMILY_IGNORED if you don't want to do this (not the default value!).
    */
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    /*
    The image and subresourceRange specify the image that is affected and the specific part of the image.
    Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.
    */

    /*
    You must specify which types of operations that involve the resource must happen before the barrier,
    and which operations that involve the resource must wait on the barrier.
    */

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
    /*
    The first parameter after the command buffer specifies in which pipeline stage the operations occur that should happen before the barrier.
    The second parameter specifies the pipeline stage in which operations will wait on the barrier.
    The third parameter is either 0 or VK_DEPENDENCY_BY_REGION_BIT. The latter turns the barrier into a per-region condition. 
    That means that the implementation is allowed to already begin reading from the parts of a resource that were written so far, for example.
    The last three pairs of parameters reference arrays of pipeline barriers of the three available types: memory barriers, buffer memory barriers,
    and image memory barriers like the one we're using here.
    */

    EndSingleTimeCommands(commandBuffer);
}

void VulkanApplication::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, imageFormat, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("texture image format does not support linear blitting!");
        /*
        There are two alternatives in this case. You could implement a function that searches common texture image formats
        for one that does support linear blitting, or you could implement the mipmap generation in software with a library like stb_image_resize.
        Each mip level can then be loaded into the image in the same way that you loaded the original image.
        */
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
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        /*
        First, we transition level i - 1 to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
        This transition will wait for level i - 1 to be filled, either from the previous blit command,
        or from vkCmdCopyBufferToImage.
        */

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
        /*
        Next, we specify the regions that will be used in the blit operation.
        The source mip level is i - 1 and the destination mip level is i.
        The two elements of the srcOffsets array determine the 3D region that data will be blitted from.
        dstOffsets determines the region that data will be blitted to.
        The X and Y dimensions of the dstOffsets[1] are divided by two since each mip level is half the size of the previous level.
        The Z dimension of srcOffsets[1] and dstOffsets[1] must be 1, since a 2D image has a depth of 1.
        */

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);
        /*
        Record the blit command

        Note that textureImage is used for both the srcImage and dstImage parameter.
        This is because we're blitting between different levels of the same image.
        The source mip level was just transitioned to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        and the destination level is still in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        */

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        /*
        This barrier transitions mip level i - 1 to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        */

        if (mipWidth > 1)
        {
            mipWidth /= 2;
        }

        if (mipHeight > 1)
        {
            mipHeight /= 2;
        }
        /*
         handles cases where the image is not square,
         since one of the mip dimensions would reach 1 before the other dimension
         When this happens, that dimension should remain 1 for all remaining levels.
        */
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
    /*
    Before we end the command buffer, we insert one more pipeline barrier.
    This barrier transitions the last mip level from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    This wasn't handled by the loop, since the last mip level is never blitted from.
    */
    EndSingleTimeCommands(commandBuffer);
}

bool VulkanApplication::CheckValidationLayerSupport() const
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

bool VulkanApplication::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
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

SwapChainSupportDetails VulkanApplication::QuerySwapChainSupport(VkPhysicalDevice device) const
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

std::vector<const char*> VulkanApplication::GetRequiredExtensions() const
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

VkSurfaceFormatKHR VulkanApplication::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanApplication::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
{
    // VK_PRESENT_MODE_MAILBOX_KHR is a very nice trade-off if energy usage is not a concern
    /*
        It allows us to avoid tearing while still maintaining a fairly low latency by rendering new images
        that are as up-to-date as possible right until the vertical blank
    */
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanApplication::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        // Get the resolution of the surface in pixels
        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);

        VkExtent2D actualExtent =
        {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

QueueFamilyIndices VulkanApplication::FindQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;
    // Assign index to queue families that could be found
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            indices.graphicsAndComputeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.IsComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

uint32_t VulkanApplication::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    /*
    The memoryTypes array consists of VkMemoryType structs that specify the heap and properties of each type of memory.
    The properties define special features of the memory, like being able to map it so we can write to it from the CPU.
    This property is indicated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    but we also need to use the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property.
    */

    throw std::runtime_error("failed to find suitable memory type!");
}

VkFormat VulkanApplication::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        /*
        The VkFormatProperties struct contains three fields:
            linearTilingFeatures: Use cases that are supported with linear tiling
            optimalTilingFeatures: Use cases that are supported with optimal tiling
            bufferFeatures: Use cases that are supported for buffers
        */

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

VkFormat VulkanApplication::FindDepthFormat() const
{
    return FindSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    // Make sure to use the VK_FORMAT_FEATURE_
    // All of these candidate formats contain a depth component, but the latter two also contain a stencil component
}

int VulkanApplication::RateDeviceSuitability(VkPhysicalDevice device) const
{
    int score = 0;
    // Checks the necessary queue family
    const QueueFamilyIndices indices = FindQueueFamilies(device);

    if (indices.IsComplete() == false)
    {
        return 0;
    }

    const bool extensionsSupported = CheckDeviceExtensionSupport(device);

    if (extensionsSupported == false)
    {
        return 0;
    }

    bool swapChainAdequate = false;
    const SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    if (swapChainAdequate == false)
    {
        return 0;
    }

    // Basic device properties like the name, type and supported Vulkan version
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // The support for optional features like texture compression,
    // 64 bit floats and multi viewport rendering (useful for VR)
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        score += 1000;
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

void VulkanApplication::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    app->m_IsFamebufferResized = true;
}
