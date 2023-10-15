#pragma once

#include <common/imgui/ImGuiConfig.h>

#include "vulkan/buffer.h"
#include "vulkan/device.h"
#include "vulkan/commandpool.h"

struct ImGuiVulkanInitInfo
{
    VkInstance instance{ VK_NULL_HANDLE };
    VkDevice device{ VK_NULL_HANDLE };
    VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
    VkQueue graphicsQueue{ VK_NULL_HANDLE };
    uint32_t queueFamily = (uint32_t)-1;
    uint32_t swapChainImageCount = (uint32_t)-1;
    VkFormat swapchainImageFormat{ VK_FORMAT_UNDEFINED };
    VkRenderPass renderPass{ VK_NULL_HANDLE };
    VkExtent2D swapChainExtend{ 0,0 };
    std::vector<VkImageView> swapChainImageViews;
    void* windowHandle = nullptr;
};

class ImGuiShadowVulkan
{
public:

    static ImGuiShadowVulkan* Create(bool dockingRenderer, bool useCustomRenderer);

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

#if VP_IMGUI_VIEWPORTS_ENABLED
    //virtual IImGuiRendererData* CreateData() = 0;
    //virtual void DestroyData(IImGuiRendererData* data) = 0;

    //virtual void OnCreateWindow(ImGuiViewport* vp) = 0;
    //virtual void OnDestroyWindow(ImGuiViewport* vp) = 0;
    //virtual void OnSetWindowSize(ImGuiViewport* vp, ImVec2 size) = 0;
#endif // VP_IMGUI_VIEWPORTS_ENABLED

protected:
    ImGuiShadowVulkan(bool dockingRenderer, bool customRenderer);

    void CreateFontsTexture();

    void CreateDescriptorPool();
    void DestroyDescriptorPool();

    void CreateSyncObjects();

    // CommandPool
    void CreateCommandPool();
    void DestroyCommandPool();

    void CreateFramebuffers();
    void CreateCommandBuffers(uint32_t swapchainImagesID);

    void CreateRenderPass(VkRenderPass& renderPass, VkFormat format,
        VkAttachmentLoadOp loadOp);

    void RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, ImDrawData* draw_data);
    void SubmitCommandBuffer(VkCommandBuffer cmdBuffer);

    // Vulkan Data
    ImGuiVulkanInitInfo m_InitInfo;

    VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };
    VkRenderPass m_RenderPassMain{ VK_NULL_HANDLE };
    VkPipeline m_PipelineMain{ VK_NULL_HANDLE };

    Vk::CommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkFramebuffer> m_Framebuffers;

    //Vk::FramesInFlight m_FramesInFlight;
    VkFence m_FenceQueueSync{ VK_NULL_HANDLE };

    //VkExtent2D m_SwapChainExtend{ 0,0 };

    // ImGui Data
    ImGuiContext* m_ImGuiContext{ nullptr };

    bool m_UseDockingRenderer = false;
    bool m_UseCustomRenderer = false;

    bool m_IsSkippingFrame = false;

    bool m_IsUseDefaultFont = false;

    bool m_IsInitialized = false;
};