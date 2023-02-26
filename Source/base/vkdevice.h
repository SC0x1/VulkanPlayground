#pragma once

#include "VkHelper.h"

#include "VkBuffer.h"

#include <algorithm>
#include <assert.h>
#include <exception>

vkSTART_NAMESPACE

struct VulkanDevice
{
	VkPhysicalDevice m_PhysicalDevice;

	VkDevice m_LogicalDevice;

	VkPhysicalDeviceProperties m_Properties;

	VkPhysicalDeviceFeatures m_Features;

	VkPhysicalDeviceFeatures m_EnabledFeatures;

	VkPhysicalDeviceMemoryProperties m_MemoryProperties;

	std::vector<VkQueueFamilyProperties> m_QueueFamilyProperties;

	std::vector<std::string> m_SupportedExtensions;

	// Default command pool for the graphics queue family index
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;

	// Set to true when the debug marker extension is detected
	bool m_IsEnableDebugMarkers = false;

	// Contains queue family indices
	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;

	operator VkDevice() const
	{
		return m_LogicalDevice;
	};

	explicit VulkanDevice(VkPhysicalDevice physicalDevice);

	~VulkanDevice();

    uint32_t GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
    uint32_t GetQueueFamilyIndex(VkQueueFlags queueFlags) const;
    VkResult CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    VkResult CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data = nullptr);
    VkResult CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size, void* data = nullptr);
	void CopyBuffer(vks::Buffer *src, vks::Buffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr);
    VkCommandPool   CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
    VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool begin = false);
    void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
    void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
    bool ExtensionSupported(std::string extension);
	VkFormat GetSupportedDepthFormat(bool checkSamplingSupport);
};

vkEND_NAMESPACE
