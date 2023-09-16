#pragma once

#include "vulkan/buffer.h"
#include "vulkan/device.h"
#include "vulkan/commandpool.h"

class VulkanBaseApp;

struct ImGuiDrawData
{
public:
    struct CmdList
    {
        std::vector<ImDrawVert> m_VertexBuffer;
        std::vector<ImDrawIdx> m_IndexBuffer;
        std::vector<ImDrawCmd> m_DrawCommands;
    };

    std::vector<CmdList> m_DrawList;
    ImVec2 m_Pos = ImVec2(0.0f, 0.0f);
    ImVec2 m_Size = ImVec2(0.0f, 0.0f);
    ImGuiViewport* m_Viewport = nullptr;
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount = 0;
};

class ImGuiRenderer
{
    // UI params
    struct PushConstantsBlock
    {
        glm::vec2 scale;
        glm::vec2 translate;
    };

public:

    bool Initialize(VulkanBaseApp* app);
    void Shutdown();

    void StartFrame();
    void EndFrame(uint32_t frameIndex, uint32_t imageIndex);

    void OnRecreateSwapchain();

    VkCommandBuffer GetCommandBuffer(uint32_t frameID) const;

    bool IsSkippingFrame() const;

private:
    void InitializePlatformCallbacks();
    void InitializeRendererCallbacks();

    void CreateFontsTexture();

    void RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, ImDrawData* draw_data);
    void SubmitCommandBuffer(VkCommandBuffer cmdBuffer);

    void CreateRenderPass(VkRenderPass& renderPass, bool mainSwapchainFormat) const;
    void CreateDescriptorPool();
    void CreateFramebuffers();
    void CreateCommandBuffers();

    void CreateSyncObjects();

    void CreateCommandPool();
    void DestroyCommandPool();

    void CreatePipeline();

    void UpdateBuffers();
    void DrawFrameMain(VkCommandBuffer cmdBuffer);

    PushConstantsBlock m_PushConstants;

    ImGuiContext* m_ImGuiContext{ nullptr };

    std::vector<VkFramebuffer> m_Framebuffers;

    VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };

    //std::vector<Vk::CommandPool> m_CommandPools;
    Vk::CommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    VkRenderPass m_RenderPassMain{ VK_NULL_HANDLE };
    VkRenderPass m_RenderPassDocking{ VK_NULL_HANDLE };

    // Pipeline - we need this only for ImGui Docking
    // to use our custom renderPass.
    VkDeviceMemory m_FontMemory = VK_NULL_HANDLE;
    VkImage m_FontImage = VK_NULL_HANDLE;
    VkImageView m_FontView = VK_NULL_HANDLE;
    VkSampler m_Sampler;

    VkPipelineCache m_PipelineCacheDocking{ VK_NULL_HANDLE };
    VkPipelineLayout m_PipelineLayoutDocking{ VK_NULL_HANDLE };
    VkPipeline m_PipelineDocking{ VK_NULL_HANDLE };

    VkDescriptorPool m_DescriptorPoolDocking{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_DescriptorSetLayoutDocking{ VK_NULL_HANDLE };

    VkDescriptorSet m_DescriptorSetDocking{ VK_NULL_HANDLE };

    VkShaderModule m_ShaderModuleVertDocking{ VK_NULL_HANDLE };
    VkShaderModule m_ShaderModuleFragDocking{ VK_NULL_HANDLE };

    Vk::Buffer m_VertexBuffer;
    Vk::Buffer m_IndexBuffer;
    int32_t m_VertexCount = 0;
    int32_t m_IndexCount = 0;

    //std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    //Vk::FramesInFlight m_FramesInFlight;
    VkFence m_FenceQueueSync;

    // UI params are set via push constants
    PushConstantsBlock m_PushConstBlock;

    VulkanBaseApp* m_App = nullptr;

    bool m_IsSkippingFrame = false;
    bool m_UseDefaultRenderer = false;
};

inline VkCommandBuffer ImGuiRenderer::GetCommandBuffer(uint32_t frameID) const
{
    return m_CommandBuffers[frameID];
}
 