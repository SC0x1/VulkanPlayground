#pragma once

#include "common/imgui/shadowimpl/imgui_shadow.h"

class ImGuiShadowVulkanCustomDockingImpl : public ImGuiShadowVulkan
{
public:
    ImGuiShadowVulkanCustomDockingImpl();

    virtual ~ImGuiShadowVulkanCustomDockingImpl();

    void Initialize(const ImGuiVulkanInitInfo& initInfo) override;
    void Shutdown() override;

    void StartFrame() override;
    void EndFrame(uint32_t frameIndex, uint32_t imageIndex) override;

    void DrawFrame(ImDrawData* imDrawData, VkCommandBuffer cmdBuffer) override;
    void UpdateBuffers(ImDrawData* imDrawData) override;

private:

    ImGuiVulkanBackendData m_BackendDataMain;
    ImGuiVulkanWindowRenderBuffers m_MainWindowBuffers;

    ImGuiVulkanBackendData m_BackendDataDocking;
    VkRenderPass m_RenderPassDocking{VK_NULL_HANDLE};
};