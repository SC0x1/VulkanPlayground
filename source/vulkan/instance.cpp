#include "VKPlayground_PCH.h"

#include <vulkan/instance.h>

vkBEGIN_NAMESPACE

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

void DestroyDebugReportCallbackEXT(VkInstance instance,
    VkDebugReportCallbackEXT debugReportCallback, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr)
    {
        func(instance, debugReportCallback, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        std::cout << "INFO: validation layer: " << pCallbackData->pMessage << std::endl;
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cout << "WARNING: validation layer: " << pCallbackData->pMessage << std::endl;
    }
    else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ||
        (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT))
    {
        std::cout << "ERROR: validation layer: " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode,
    const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        std::cout << "ERROR: debug report: " << messageCode << pMessage << std::endl;
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        std::cout << "WARNING: debug report: " << messageCode << pMessage << std::endl;
    }
    else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        std::cout << "INFO: debug report:" << messageCode << pMessage << std::endl;
    }

    return VK_FALSE;
}

void* AllocationFunction(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    return nullptr;
}

void* ReallocationFunction(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    return nullptr;
}

void FreeFunction(void* pUserData, void* pMemory)
{
}

void InternalAllocationNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
}

void InternalFreeNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
}

// TODO: Implement Allocator
VkAllocationCallbacks Instance::ms_AllocatorCallbacks = {
    VK_NULL_HANDLE,
    VK_NULL_HANDLE/*AllocationFunction*/,
    VK_NULL_HANDLE/*ReallocationFunction*/,
    VK_NULL_HANDLE/*FreeFunction*/,
    VK_NULL_HANDLE/*InternalAllocationNotification*/,
    VK_NULL_HANDLE/*InternalFreeNotification*/
};

Instance::Instance()
{
}

Instance::~Instance()
{
    Destroy();
}

void Instance::Initialize(const InstanceInitInfo& initInfo)
{
    m_IsValidationLayersEnabled = initInfo.validationLayersEnable;

    if (m_IsValidationLayersEnabled)
    {
        if (!SetupValidationLayers(initInfo.validationLayersRequested))
        {
            // "Validation layers requested, but not available!"
            assert(0);
        }
    }

    SetupExtensions();

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = initInfo.applicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(initInfo.applicationVersionMajor, initInfo.applicationVersionMinor, initInfo.applicationVersionPatch);
    appInfo.pEngineName = "QbEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = GetNumLayersNames();
    createInfo.ppEnabledLayerNames = GetLayersNames();
    createInfo.enabledExtensionCount = GetNumInstanceExtensionsNames();
    createInfo.ppEnabledExtensionNames = GetInstanceExtensionsNames();

    // Debug layer
    if (m_IsDebugUtilsExtensionAvailable)
    {
        m_DebugUtilsCreateInfo = {};
        m_DebugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        m_DebugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        // INFO / VERBOSE messages
        //m_DebugUtilsCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        //m_DebugUtilsCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

        m_DebugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        m_DebugUtilsCreateInfo.pfnUserCallback = &VulkanDebugUtilsCallback;
        m_DebugUtilsCreateInfo.pUserData = this;

        createInfo.pNext = &m_DebugUtilsCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    const VkResult result = vkCreateInstance(&createInfo, nullptr/*&ms_AllocatorCallbacks*/, &m_Instance);
    VK_CHECK(result);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }

    SetupDebugMessenger();
}

void Instance::Destroy()
{
    if (m_DebugUtilsMessenger)
    {
        DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugUtilsMessenger, nullptr/*&ms_AllocatorCallbacks*/);
        m_DebugUtilsMessenger = VK_NULL_HANDLE;
    }
    else if (m_DebugReportCallback)
    {
        DestroyDebugReportCallbackEXT(m_Instance, m_DebugReportCallback, nullptr);
        m_DebugReportCallback = VK_NULL_HANDLE;
    }

    vkDestroyInstance(m_Instance, nullptr/*&ms_AllocatorCallbacks*/);

    m_Instance = VK_NULL_HANDLE;
}

bool Instance::SetupValidationLayers(std::vector<const char*> validationLayersNamesRequested)
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    m_AvailableLayers.resize(layerCount);

    vkEnumerateInstanceLayerProperties(&layerCount, m_AvailableLayers.data());

    for (const char* layerName : validationLayersNamesRequested)
    {
        bool isLayerFound = false;

        for (const auto& layerProperties : m_AvailableLayers)
        {
            if (::strcmp(layerName, layerProperties.layerName) == 0)
            {
                isLayerFound = true;
                m_ValidationLayersName.push_back(layerName);
            }
        }

        if (!isLayerFound)
        {
            return false;
        }
    }

    return true;
}

void Instance::SetupExtensions()
{
    bool isSurfaceExtensionFound = false;
    bool isPlatformSurfaceExtensionFound = false;

    bool hasDebugUtilsExtension = false;
    bool hasDebugReportExtension = false;

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    m_InstanceExtensions.resize(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, m_InstanceExtensions.data());

    for (const auto& extension : m_InstanceExtensions)
    {
        // Surface Extension
        if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension.extensionName))
        {
            m_InstanceExtensionsNames.push_back(extension.extensionName);
            isSurfaceExtensionFound = true;
        }

        // Platform surface extension
        if (!strcmp(Vk::Utils::GetSurfaceKHRExtensionName(), extension.extensionName))
        {
            isPlatformSurfaceExtensionFound = true;
            m_InstanceExtensionsNames.push_back(extension.extensionName);
        }

        if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extension.extensionName))
        {
            hasDebugUtilsExtension = true;
        }

        if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, extension.extensionName))
        {
            hasDebugReportExtension = true;
        }
    }

    if (m_IsValidationLayersEnabled)
    {
        if (hasDebugUtilsExtension)
        {
            m_InstanceExtensionsNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            m_IsDebugUtilsExtensionAvailable = true;
        }
        else if (hasDebugReportExtension)
        {
            m_InstanceExtensionsNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

            m_IsDebugReportExtensionAvailable = true;
        }
    }

    if (isSurfaceExtensionFound == false)
    {
        assert(0);
    }

    if (isPlatformSurfaceExtensionFound == false)
    {
        assert(0);
    }
}

void Instance::SetupDebugMessenger()
{
    if (!m_IsValidationLayersEnabled)
    {
        return;
    }

    if (m_IsDebugUtilsExtensionAvailable)
    {
        if (CreateDebugUtilsMessengerEXT(m_Instance, &m_DebugUtilsCreateInfo, nullptr, &m_DebugUtilsMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
    else if (m_IsDebugReportExtensionAvailable)
    {
        VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
        callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_DEBUG_BIT_EXT;

        // Uncomment to REPORT all vulkan api call
        //callbackCreateInfo.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;

        callbackCreateInfo.pUserData = this;

        callbackCreateInfo.pfnCallback = &VulkanDebugReportCallback;

        auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugReportCallbackEXT");
        if (func != nullptr)
        {
            func(m_Instance, &callbackCreateInfo, nullptr/*&ms_AllocatorCallbacks*/, &m_DebugReportCallback);
        }
        else
        {
            assert(0);
        }
    }
}

vkEND_NAMESPACE