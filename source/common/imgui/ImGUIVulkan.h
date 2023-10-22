#pragma once

#include "vulkan/buffer.h"
#include "vulkan/device.h"
#include "vulkan/commandpool.h"

class VulkanBaseApp;
class ImGuiShadowVulkan;

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
    ImGuiRenderer();
    ~ImGuiRenderer();

    bool Initialize(VulkanBaseApp* app);
    void Shutdown();

    void StartFrame();
    void EndFrame(uint32_t frameIndex, uint32_t imageIndex);

    bool IsSkippingFrame() const;

    void OnRecreateSwapchain(const Vk::SwapChain& swapChain);

    ImGuiContext* m_ImGuiContext{ nullptr };

    std::unique_ptr<ImGuiShadowVulkan> m_ShadowBackendImGui;

    bool m_IsSkippingFrame = false;
};