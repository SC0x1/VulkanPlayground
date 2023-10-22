#pragma once

#include <common/imgui/ImGuiConfig.h>

struct PushConstantsBlock
{
    glm::vec2 scale;
    glm::vec2 translate;
};

struct ImGuiVulkanInitInfo
{
    VkInstance instance{ VK_NULL_HANDLE };
    VkDevice device{ VK_NULL_HANDLE };
    VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
    VkQueue graphicsQueue{ VK_NULL_HANDLE };
    uint32_t queueFamily = (uint32_t)-1;
    uint32_t swapChainImageCount = (uint32_t)-1;
    VkFormat swapchainImageFormat{ VK_FORMAT_UNDEFINED };
    VkRenderPass renderPassMain{ VK_NULL_HANDLE };
    VkPipelineCache pipelineCache{ VK_NULL_HANDLE };
    VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
    VkExtent2D swapChainExtend{ 0,0 };
    std::vector<VkImageView> swapChainImageViews;
    void* windowHandle = nullptr;
};

struct ImGuiVulkanWindowRenderBuffers
{
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    Vk::Buffer indexBuffer;
    Vk::Buffer vertexBuffer;
};

struct ImGuiVulkanFontData
{
    VkDeviceMemory fontMemory{ VK_NULL_HANDLE };
    VkImage fontImage{ VK_NULL_HANDLE };
    VkImageView fontView{ VK_NULL_HANDLE };
    VkSampler fontSampler{ VK_NULL_HANDLE };
};

struct ImGuiVulkanBackendData
{
    VkPipelineCache pipelineCache{ VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    VkPipeline pipeline{ VK_NULL_HANDLE };

    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

    VkShaderModule shaderModuleVert{ VK_NULL_HANDLE };
    VkShaderModule shaderModuleFrag{ VK_NULL_HANDLE };

    // Font
    ImGuiVulkanFontData fontData;
};

namespace ImGuiUtils
{
    void UpdateBuffers(const ImGuiVulkanInitInfo& initInfo, ImDrawData* imDrawData,
        ImGuiVulkanWindowRenderBuffers& renderBuffers);

    void CreateRenderPass(VkDevice device, VkFormat format, VkAttachmentLoadOp loadOp,
        VkRenderPass& renderPass);

    void CreatePipeline(const ImGuiVulkanInitInfo& initInfo, VkRenderPass renderPass,
        ImGuiVulkanBackendData& data);

    void CreateFontsTextureCustom(const ImGuiVulkanInitInfo& initInfo, VkCommandPool commandPool,
        ImGuiVulkanFontData& fontData);
}