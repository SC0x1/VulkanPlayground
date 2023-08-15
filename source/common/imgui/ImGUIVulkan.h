#pragma once

#include "vulkan/buffer.h"
#include "vulkan/device.h"
#include "vulkan/commandpool.h"

class VulkanBaseApp;

class VulkanImGUI
{
public:

    bool Initialize(VulkanBaseApp* app);
    void Shutdown();

    void StartFrame();

    void OnRender(uint32_t imageIndex, VkCommandBuffer cmdBuffer = VK_NULL_HANDLE);

    void OnRecreateSwapchain();

    VkCommandBuffer GetCommandBuffer(uint32_t frameID) const;

private:

    void RenderImGuiFrame(uint32_t imageIndex, uint32_t frameIdx, ImDrawData* draw_data);

    void RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, ImDrawData* draw_data);

    void CreateRenderPass();
    void CreateDescriptorPool();
    void CreateFramebuffers();
    void CreateCommandBuffers();

    void CreateCommandPool();
    void DestroyCommandPool();

    void CreateSyncObjects();
    void DestroySyncObjects();

    ImGuiContext* m_ImGuiContext{ nullptr };

    std::vector<VkFramebuffer> m_Framebuffers;

    VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };

    //std::vector<Vk::CommandPool> m_CommandPools;
    Vk::CommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    VkRenderPass m_RenderPass{ VK_NULL_HANDLE };

    std::vector<VkSemaphore> m_RenderFinishedSemaphores;

    VulkanBaseApp* m_App = nullptr;
};

inline VkCommandBuffer VulkanImGUI::GetCommandBuffer(uint32_t frameID) const
{
    return m_CommandBuffers[frameID];
}
 