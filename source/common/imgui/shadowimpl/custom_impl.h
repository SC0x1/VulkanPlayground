#pragma once

#include "common/imgui/shadowimpl/imgui_shadow.h"

class ImGuiShadowVulkanCustomImpl : public ImGuiShadowVulkan
{
public:
    ImGuiShadowVulkanCustomImpl();

    virtual ~ImGuiShadowVulkanCustomImpl();

    void Initialize(const ImGuiVulkanInitInfo& initInfo) override;
    void Shutdown() override;

    void StartFrame() override;
    void EndFrame(uint32_t frameIndex, uint32_t imageIndex) override;

protected:
    void DrawFrame(ImDrawData* imDrawData, VkCommandBuffer cmdBuffer) override;
    virtual void UpdateBuffers(ImDrawData* imDrawData) override;

    void CreateFontsTextureCustom();

    //VkPipelineCache m_PipelineCache{ VK_NULL_HANDLE };
    //VkPipelineLayout m_PipelineLayout{ VK_NULL_HANDLE };
    //VkPipeline m_Pipeline{ VK_NULL_HANDLE };

    //VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };
    //VkDescriptorSetLayout m_DescriptorSetLayout{ VK_NULL_HANDLE };

    //VkDescriptorSet m_DescriptorSet{ VK_NULL_HANDLE };

    //VkShaderModule m_ShaderModuleVert{ VK_NULL_HANDLE };
    //VkShaderModule m_ShaderModuleFrag{ VK_NULL_HANDLE };

    // Font
    //VkDeviceMemory m_FontMemory = VK_NULL_HANDLE;
    //VkImage m_FontImage = VK_NULL_HANDLE;
    //VkImageView m_FontView = VK_NULL_HANDLE;
    //VkSampler m_Sampler;
    // Font
    
    ImGuiVulkanBackendData m_BackendDataMain;
    ImGuiVulkanWindowRenderBuffers m_Buffers;
};