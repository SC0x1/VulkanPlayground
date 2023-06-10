#pragma once

#include "base/vkbuffer.h"
#include "base/vkdevice.h"

class VulkanBaseApp;

class VulkanImGUI
{
public:

    void Initialize();
    void Shutdown();

    void BeginFrame();
    void EndFrame();

private:
    // Vulkan resources for rendering the UI
    VkSampler sampler;
    vk::Buffer vertexBuffer;
    vk::Buffer indexBuffer;
    int32_t vertexCount = 0;
    int32_t indexCount = 0;
    VkDeviceMemory fontMemory = VK_NULL_HANDLE;
    VkImage fontImage = VK_NULL_HANDLE;
    VkImageView fontView = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    vk::VulkanDevice* device;
    VkPhysicalDeviceDriverProperties driverProperties = {};
    VulkanBaseApp* example;

};

