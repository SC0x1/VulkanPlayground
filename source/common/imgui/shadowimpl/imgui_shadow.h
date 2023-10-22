#pragma once

#include "common/imgui/shadowimpl/imgui_common.h"

#include "vulkan/buffer.h"
#include "vulkan/device.h"
#include "vulkan/commandpool.h"

class ImGuiShadowVulkan
{
public:

    virtual ~ImGuiShadowVulkan();

    virtual void Initialize(const ImGuiVulkanInitInfo& initInfo);
    virtual void Shutdown();

    bool IsInitialized() const;

    void SetUseCustomRenderer(bool isCustomRenderer);
    void SetUseDefaultFont(bool useDefaultFont);

    bool IsCustomRenderer() const;
    bool IsDockingRenderer() const;

    virtual void StartFrame();
    virtual void EndFrame(uint32_t frameIndex, uint32_t imageIndex);

    virtual void OnRecreateSwapchain(const Vk::SwapChain& swapChain);

    bool IsSkippingFrame() const;

    // Backend specific methods
    virtual void DrawFrame(ImDrawData* imDrawData, VkCommandBuffer cmdBuffer);
    virtual void UpdateBuffers(ImDrawData* imDrawData);

    static ImGuiShadowVulkan* Create(bool dockingRenderer, bool useCustomRenderer);

protected:

    ImGuiShadowVulkan(bool dockingRenderer, bool customRenderer);

    // Font
    void CreateFontsTexture();

    void CreateDescriptorPool();
    void DestroyDescriptorPool();

    void CreateSyncObjects();

    // CommandPool
    void CreateCommandPool();
    void DestroyCommandPool();

    void CreateFramebuffers();
    void CreateCommandBuffers(uint32_t swapchainImagesID);

    void RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, ImDrawData* draw_data);
    void SubmitCommandBuffer(VkCommandBuffer cmdBuffer);

    // Vulkan Data
    ImGuiVulkanInitInfo m_InitInfo;

    VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };
    //VkRenderPass m_RenderPassMain{ VK_NULL_HANDLE };
    //VkPipeline m_PipelineMain{ VK_NULL_HANDLE };

    Vk::CommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkFramebuffer> m_Framebuffers;

    //Vk::FramesInFlight m_FramesInFlight;
    VkFence m_FenceQueueSync{ VK_NULL_HANDLE };

    //VkExtent2D m_SwapChainExtend{ 0,0 };

    // ImGui Data
    ImGuiContext* m_ImGuiContext{ nullptr };

    // Custom Renderer
    // UI params are set via push constants
    PushConstantsBlock m_PushConstBlock;

    bool m_IsRenderPassMainCreated = false;
    bool m_IsDescriptorPoolCreated = false;

    bool m_UseDockingRenderer = false;
    bool m_UseCustomRenderer = false;

    bool m_IsSkippingFrame = false;

    bool m_IsUseDefaultFont = false;

    bool m_IsInitialized = false;
};