#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

struct InstanceInitInfo
{
    bool validationLayersEnable = true;
    std::vector<const char*> validationLayersRequested;
    char applicationName[256] = "QbApp";

    const uint32_t applicationVersionMajor = 1;
    const uint32_t applicationVersionMinor = 0;
    const uint32_t applicationVersionPatch = 0;
};

class Instance
{
public:
    Instance();
    ~Instance();

    void Initialize(const InstanceInitInfo& initInfo);
    void Destroy();

    VkInstance GetVkInstance() const { return m_Instance; }

    bool IsEnableValidationLayers() const { return m_IsValidationLayersEnabled; }

    uint32_t GetNumLayersNames() const { return uint32_t(m_ValidationLayersName.size()); }
    const char* const* GetLayersNames() const { return &(*m_ValidationLayersName.data()); }

    bool IsDebugUtilsExtensionAvailable() const { return m_IsDebugUtilsExtensionAvailable; }

    static const VkAllocationCallbacks& GetVkAllocator() { return ms_AllocatorCallbacks; }

private:
    bool SetupValidationLayers(std::vector<const char*> validationLayersNamesRequested);
    void SetupExtensions();

    void SetupDebugMessenger();

    uint32_t GetNumInstanceExtensionsNames() const { return uint32_t(m_InstanceExtensionsNames.size()); }
    const char* const* GetInstanceExtensionsNames() const { return &(*m_InstanceExtensionsNames.data()); }

    VkInstance m_Instance = VK_NULL_HANDLE;
    static VkAllocationCallbacks ms_AllocatorCallbacks;

    std::vector<VkLayerProperties> m_AvailableLayers;
    std::vector<const char*> m_ValidationLayersName;

    // A VkDebugUtilsMessengerEXT is a messenger object which handles passing along debug
    // messages to a provided debug callback
    VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;
    VkDebugUtilsMessengerCreateInfoEXT m_DebugUtilsCreateInfo;

    VkDebugReportCallbackEXT m_DebugReportCallback = VK_NULL_HANDLE;

    std::vector<VkExtensionProperties> m_InstanceExtensions;
    std::vector<const char*> m_InstanceExtensionsNames;

    bool m_IsValidationLayersEnabled = false;

    bool m_IsDebugUtilsExtensionAvailable = false;
    bool m_IsDebugReportExtensionAvailable = false;
};

vkEND_NAMESPACE