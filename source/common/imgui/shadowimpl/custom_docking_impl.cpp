#include "VKPlayground_PCH.h"

#include "vulkan/utils.h"
#include "vulkan/framesinflight.h"
#include "vulkan/swapchain.h"
#include "vulkan/surface.h"
#include "vulkan/context.h"

#include "common/imgui/shadowimpl/custom_docking_impl.h"

namespace ImGuiDockingInternal
{
    // Helper structure to hold the data needed by one rendering context into one OS window
    // (Used by example's main.cpp. Used by multi-viewport features. Probably NOT used by your own engine/app.)
    struct VulkanWindow
    {
        VulkanWindow()
        {
            memset((void*)this, 0, sizeof(*this));

            m_pSwapchain = std::make_unique<Vk::SwapChain>();
            m_pFramesInFlight = std::make_unique<Vk::FramesInFlight>();

            //PresentMode = (VkPresentModeKHR)~0;     // Ensure we get an error if user doesn't set this.
            m_ClearEnable = true;
        }

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        //Vk::Surface m_Surface;
        VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };

        std::unique_ptr<Vk::SwapChain> m_pSwapchain;
        std::unique_ptr<Vk::FramesInFlight> m_pFramesInFlight;

        std::vector<VkFramebuffer> m_Framebuffers{};

        Vk::CommandPool m_CommandPool;
        std::vector<VkCommandBuffer> m_CommandBuffers;

        VkRenderPass m_RenderPass{VK_NULL_HANDLE};
        VkPipeline m_Pipeline{ VK_NULL_HANDLE }; // The window pipeline may uses a different VkRenderPass than the one passed in ImGui_ImplVulkan_InitInfo

        VkClearValue m_ClearValue{};

        uint32_t m_QueueFamily = 0;
        uint32_t m_FrameIndex = 0;
        uint32_t m_ImageCount = 0;
        uint32_t m_ImageIndex = 0;

        bool m_ClearEnable{false};
    };

    // For multi-viewport support:
    // Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend data.
    struct VulkanViewportData
    {
        bool WindowOwned = false;
        VulkanWindow Window;             // Used by secondary Viewports only
        //VulkanWindowRenderBuffers RenderBuffers{};      // Used by all Viewports

        ImGuiVulkanWindowRenderBuffers renderBuffers;
        //Vk::Buffer m_VertexBuffer;
        //uint32_t m_VertexCount = 0;

        //Vk::Buffer m_IndexBuffer;
        //uint32_t m_IndexCount = 0;

        VulkanViewportData()
        {
            //memset(&RenderBuffers, 0, sizeof(RenderBuffers)); 
        }
        ~VulkanViewportData() { }
    };

    static void Renderer_CreateWindow(ImGuiViewport* viewport);
    static void Renderer_DestroyWindow(ImGuiViewport* viewport);
    static void Renderer_SetWindowSize(ImGuiViewport* viewport, ImVec2 size);
    static void Renderer_RenderWindow(ImGuiViewport* viewport, void* renderArg);
    static void Renderer_SwapBuffers(ImGuiViewport* viewport, void* renderArg);

    static ImGuiVulkanInitInfo* initInfo = nullptr;
    static ImGuiVulkanBackendData* bdDocking = nullptr;
};

ImGuiShadowVulkanCustomDockingImpl::ImGuiShadowVulkanCustomDockingImpl()
    : ImGuiShadowVulkan(true, true)
{
}

ImGuiShadowVulkanCustomDockingImpl::~ImGuiShadowVulkanCustomDockingImpl()
{

}

void ImGuiShadowVulkanCustomDockingImpl::Initialize(const ImGuiVulkanInitInfo& vulkanInitInfo)
{
    ImGuiShadowVulkan::Initialize(vulkanInitInfo);

    ImGuiDockingInternal::initInfo = &m_InitInfo;

    ImGuiUtils::CreateFontsTextureCustom(m_InitInfo,
        m_CommandPool.GetVkCommandPool(),
        m_BackendDataMain.fontData);

    ImGuiUtils::CreatePipeline(m_InitInfo, m_InitInfo.renderPassMain, m_BackendDataMain);

    m_MainWindowBuffers.vertexBuffer.m_Device = m_InitInfo.device;
    m_MainWindowBuffers.indexBuffer.m_Device = m_InitInfo.device;

    ImGuiUtils::CreateRenderPass(m_InitInfo.device, VK_FORMAT_B8G8R8A8_UNORM,
        VK_ATTACHMENT_LOAD_OP_LOAD, m_RenderPassDocking);

    ImGuiUtils::CreateFontsTextureCustom(m_InitInfo,
        m_CommandPool.GetVkCommandPool(),
        m_BackendDataDocking.fontData);

    ImGuiUtils::CreatePipeline(m_InitInfo, m_RenderPassDocking, m_BackendDataDocking);
    ImGuiDockingInternal::bdDocking = &m_BackendDataDocking;

    // ImGui callbacks registration
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    {
        platform_io.Renderer_CreateWindow = ImGuiDockingInternal::Renderer_CreateWindow;
        platform_io.Renderer_DestroyWindow = ImGuiDockingInternal::Renderer_DestroyWindow;
        platform_io.Renderer_SetWindowSize = ImGuiDockingInternal::Renderer_SetWindowSize;
        platform_io.Renderer_RenderWindow = ImGuiDockingInternal::Renderer_RenderWindow;
        platform_io.Renderer_SwapBuffers = ImGuiDockingInternal::Renderer_SwapBuffers;
    }
}

void ImGuiShadowVulkanCustomDockingImpl::Shutdown()
{
    // Destroy Font resources
    const ImGuiVulkanFontData& fontDataMain = m_BackendDataMain.fontData;
    vkDestroyImage(m_InitInfo.device, fontDataMain.fontImage, nullptr);
    vkDestroyImageView(m_InitInfo.device, fontDataMain.fontView, nullptr);
    vkFreeMemory(m_InitInfo.device, fontDataMain.fontMemory, nullptr);
    vkDestroySampler(m_InitInfo.device, fontDataMain.fontSampler, nullptr);

    const ImGuiVulkanFontData& fontDataDocking = m_BackendDataDocking.fontData;
    vkDestroyImage(m_InitInfo.device, fontDataDocking.fontImage, nullptr);
    vkDestroyImageView(m_InitInfo.device, fontDataDocking.fontView, nullptr);
    vkFreeMemory(m_InitInfo.device, fontDataDocking.fontMemory, nullptr);
    vkDestroySampler(m_InitInfo.device, fontDataDocking.fontSampler, nullptr);

    // Pipeline Main
    vkDestroyPipelineCache(m_InitInfo.device, m_BackendDataMain.pipelineCache, nullptr);
    vkDestroyPipeline(m_InitInfo.device, m_BackendDataMain.pipeline, nullptr);
    vkDestroyPipelineLayout(m_InitInfo.device, m_BackendDataMain.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_InitInfo.device, m_BackendDataMain.descriptorSetLayout, nullptr);

    // Pipeline Docking
    vkDestroyPipelineCache(m_InitInfo.device, m_BackendDataDocking.pipelineCache, nullptr);
    vkDestroyPipeline(m_InitInfo.device, m_BackendDataDocking.pipeline, nullptr);
    vkDestroyPipelineLayout(m_InitInfo.device, m_BackendDataDocking.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_InitInfo.device, m_BackendDataDocking.descriptorSetLayout, nullptr);

    m_MainWindowBuffers.vertexBuffer.Destroy();
    m_MainWindowBuffers.indexBuffer.Destroy();

    // ImGui callbacks unregister
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    {
        platform_io.Renderer_CreateWindow = nullptr;
        platform_io.Renderer_DestroyWindow = nullptr;
        platform_io.Renderer_SetWindowSize = nullptr;
        platform_io.Renderer_RenderWindow = nullptr;
        platform_io.Renderer_SwapBuffers = nullptr;
    }

    ImGuiShadowVulkan::Shutdown();
}

void ImGuiShadowVulkanCustomDockingImpl::StartFrame()
{
    ImGuiShadowVulkan::StartFrame();
}

void ImGuiShadowVulkanCustomDockingImpl::EndFrame(uint32_t frameIndex, uint32_t imageIndex)
{
    ImGuiShadowVulkan::EndFrame(frameIndex, imageIndex);

#if defined VP_IMGUI_VIEWPORTS_ENABLED
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
#endif // VP_IMGUI_VIEWPORTS_ENABLED
}

void ImGuiShadowVulkanCustomDockingImpl::DrawFrame(ImDrawData* imDrawData, VkCommandBuffer cmdBuffer)
{
    VkPipelineLayout& pipelineLayout = m_BackendDataMain.pipelineLayout;
    VkPipeline& pipelineMain = m_BackendDataMain.pipeline;
    VkDescriptorSet& descriptorSet = m_BackendDataMain.descriptorSet;

    ImGuiIO& io = ImGui::GetIO();

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineMain);

    VkViewport viewport{};
    viewport.width = (float)m_InitInfo.swapChainExtend.width;
    viewport.height = (float)m_InitInfo.swapChainExtend.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    // UI scale and translate via push constants
    if (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports)
    {
        // Setup scale and translation:
        // Our visible penguin space lies from draw_data->DisplayPps (top left) to
        // draw_data->DisplayPos+data_data->DisplaySize (bottom right).
        // DisplayPos is (0,0) for single viewport apps.
        {
            m_PushConstBlock.scale = glm::vec2(2.0f / imDrawData->DisplaySize.x, 2.0f / imDrawData->DisplaySize.y);

            float translate[2];
            translate[0] = -1.0f - imDrawData->DisplayPos.x * m_PushConstBlock.scale[0];
            translate[1] = -1.0f - imDrawData->DisplayPos.y * m_PushConstBlock.scale[1];
            m_PushConstBlock.translate = glm::vec2(translate[0], translate[1]);

            vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantsBlock), &m_PushConstBlock);
        }
    }

    // Render commands
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    const VkBuffer vkIndexBuffer = m_MainWindowBuffers.indexBuffer.m_Buffer;
    const VkBuffer vkVertexBuffer = m_MainWindowBuffers.vertexBuffer.m_Buffer;

    if (imDrawData->CmdListsCount > 0)
    {
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vkVertexBuffer, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, vkIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

        if (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports)
        {
            // Will project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_off = imDrawData->DisplayPos;         // (0,0) unless using multi-viewports
            ImVec2 clip_scale = imDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

            int32_t vtxOffset = 0;
            int32_t idxOffset = 0;

            for (uint32_t i = 0; i < (uint32_t)imDrawData->CmdListsCount; i++)
            {
                const ImDrawList* cmd_list = imDrawData->CmdLists[i];
                for (uint32_t cmd_i = 0; cmd_i < (uint32_t)cmd_list->CmdBuffer.Size; cmd_i++)
                {
                    const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                    ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                    // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                    if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                    if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                    if (clip_max.x > viewport.width) { clip_max.x = viewport.width; }
                    if (clip_max.y > viewport.height) { clip_max.y = viewport.height; }
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        continue;

                    // Apply scissor/clipping rectangle
                    VkRect2D scissor;
                    scissor.offset.x = (int32_t)(clip_min.x);
                    scissor.offset.y = (int32_t)(clip_min.y);
                    scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                    scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
                    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                    // Draw
                    vkCmdDrawIndexed(cmdBuffer, pcmd->ElemCount, 1, pcmd->IdxOffset + idxOffset, pcmd->VtxOffset + vtxOffset, 0);
                }
                idxOffset += cmd_list->IdxBuffer.Size;
                vtxOffset += cmd_list->VtxBuffer.Size;
            }

            VkRect2D scissor = { { 0, 0 }, { m_InitInfo.swapChainExtend.width, m_InitInfo.swapChainExtend.height } };
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        }
    }
}

void ImGuiShadowVulkanCustomDockingImpl::UpdateBuffers(ImDrawData* imDrawData)
{
    ImGuiUtils::UpdateBuffers(m_InitInfo, imDrawData, m_MainWindowBuffers);
}

namespace ImGuiDockingInternal
{
    //////////////////////////////////////////////////////////////////////////
    void CreateWindowResources(VkDevice device, VulkanWindow* wd,
        const VkAllocationCallbacks* allocator)
    {
        VkResult err;
        VkSwapchainKHR old_swapchain = wd->m_pSwapchain->GetVkSwapChain();
        err = vkDeviceWaitIdle(device);
        VK_CHECK(err);

        const uint32_t imageCount = wd->m_pSwapchain->GetImageCount();

        //wd->m_FramesInFlight.Destroy();

        if (wd->m_RenderPass)
            vkDestroyRenderPass(device, wd->m_RenderPass, allocator);
        if (wd->m_Pipeline)
            vkDestroyPipeline(device, wd->m_Pipeline, allocator);

        // Create the Render Pass
        {
            //ImGuiRenderer::CreateRenderPass(wd->m_RenderPass, wd->m_pSwapchain->GetSurfaceFormat().format,
            //    wd->m_ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
            VkAttachmentDescription attachment = {};
            attachment.format = wd->m_pSwapchain->GetSurfaceFormat().format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = wd->m_ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            VkAttachmentReference color_attachment = {};
            color_attachment.attachment = 0;
            color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment;
            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            VkRenderPassCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            info.attachmentCount = 1;
            info.pAttachments = &attachment;
            info.subpassCount = 1;
            info.pSubpasses = &subpass;
            info.dependencyCount = 1;
            info.pDependencies = &dependency;
            err = vkCreateRenderPass(device, &info, allocator, &wd->m_RenderPass);
            VK_CHECK(err);
        }

        for (size_t i = 0; i < wd->m_Framebuffers.size(); i++)
        {
            vkDestroyFramebuffer(device, wd->m_Framebuffers[i], nullptr);
        }

        // Create Framebuffer
        wd->m_Framebuffers = wd->m_pSwapchain->CreateFramebuffers(wd->m_RenderPass);
    }

    void CreateWindowCommandBuffers(VkPhysicalDevice physicalDevice, VkDevice device, VulkanWindow* wd)
    {
        const uint32_t imageCount = (uint32_t)wd->m_Framebuffers.size();

        if (wd->m_CommandBuffers.size() > 0)
        {
            vkFreeCommandBuffers(device, wd->m_CommandPool.GetVkCommandPool(),
                (uint32_t)wd->m_Framebuffers.size(), wd->m_CommandBuffers.data());
        }
        wd->m_CommandPool.Destroy(device);

        wd->m_CommandPool.Create(device, wd->m_QueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        wd->m_CommandBuffers.resize(imageCount);

        // Create Command Buffers
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = wd->m_CommandPool.GetVkCommandPool();
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = imageCount;

        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, wd->m_CommandBuffers.data()));
    }

    void CreateOrResizeWindow(VulkanWindow* wd, uint32_t w, uint32_t h)
    {
        //////////////////////////////////////////////////////////////////////////
        // Recreate Swapchain
        VkResult result;
        result = vkDeviceWaitIdle(initInfo->device);
        VK_CHECK(result);

        wd->m_ImageCount = 0;
        //if (wd->m_RenderPass)
        //{
        //    vkDestroyRenderPass(device, wd->m_RenderPass, nullptr);
        //}

        uint32_t windowSize[2] = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
        wd->m_pSwapchain->Rebuild(windowSize);

        // Destroy Framebuffers
        //for (auto framebuffer : wd->m_Framebuffers)
        //{
        //    vkDestroyFramebuffer(device, framebuffer, nullptr);
        //}

        //RecreateSwapchain(wd, nullptr, w, h);
        CreateWindowResources(initInfo->device, wd, nullptr);
        CreateWindowCommandBuffers(initInfo->physicalDevice, initInfo->device, wd);
    }

    static void Renderer_CreateWindow(ImGuiViewport* viewport)
    {
        const VkInstance instance = initInfo->instance;
        const VkDevice device = initInfo->device;
        const VkPhysicalDevice physicalDevice = initInfo->physicalDevice;
        const VkQueue graphicsQueue = initInfo->graphicsQueue;

        VulkanViewportData* vd = IM_NEW(VulkanViewportData)();
        viewport->RendererUserData = vd;
        VulkanWindow* wd = &vd->Window;
        wd->m_QueueFamily = initInfo->queueFamily;

        // Create surface
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        VkResult err = (VkResult)platform_io.Platform_CreateVkSurface(viewport, (ImU64)instance, nullptr, (ImU64*)&wd->m_Surface);
        VK_CHECK(err);

        //#if defined(VK_USE_PLATFORM_WIN32_KHR)
        //        wd->m_Surface.InitSurface(instance, (void*)::GetModuleHandle(nullptr), glfwGetWin32Window(windowHandle));
        //#else
        //        m_Surface.InitSurface(m_Instance, m_Window);
        //#endif

                // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, initInfo->queueFamily, wd->m_Surface, &res);
        if (res != VK_TRUE)
        {
            assert(0); // Error: no WSI support on physical device
            return;
        }

        // Select Surface Format
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

        const VkSurfaceKHR surface = wd->m_Surface;
        uint32_t windowSize[2] = { static_cast<uint32_t>(viewport->Size.x), static_cast<uint32_t>(viewport->Size.y) };
        Vk::SurfaceFormatsRequest sfRequest
        {
            .formats = requestSurfaceImageFormat,
            .formatsCount = (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
            .colorSpace = requestSurfaceColorSpace,
        };

        //int width = 0, height = 0;
        //while (width == 0 || height == 0)
        //{
        //    GLFWwindow* window = (GLFWwindow*)viewport->PlatformHandle;
        //    // Window minimization a special case
        //    glfwGetFramebufferSize(window, &width, &height);
        //    glfwWaitEvents();
        //}

        wd->m_Width = static_cast<uint32_t>(viewport->Size.x);
        wd->m_Height = static_cast<uint32_t>(viewport->Size.y);

        wd->m_pSwapchain->Create(device, physicalDevice, surface, windowSize, sfRequest);
        wd->m_ClearEnable = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? false : true;

        wd->m_pFramesInFlight->Initialize(device, wd->m_pSwapchain->GetImageCount());

        CreateWindowResources(device, wd, nullptr);
        CreateWindowCommandBuffers(physicalDevice, device, wd);
    }

    static void Renderer_DestroyWindow(ImGuiViewport* viewport)
    {
        vkDeviceWaitIdle(initInfo->device);

        VulkanViewportData* vd = (VulkanViewportData*)viewport->RendererUserData;
        if (vd)
        {
            vd->renderBuffers.vertexBuffer.Destroy();
            vd->renderBuffers.indexBuffer.Destroy();

            VulkanWindow* wd = &vd->Window;

            vkFreeCommandBuffers(initInfo->device, wd->m_CommandPool.GetVkCommandPool(),
                static_cast<uint32_t>(wd->m_CommandBuffers.size()), wd->m_CommandBuffers.data());

            wd->m_CommandPool.Destroy(initInfo->device);

            wd->m_pFramesInFlight->Destroy();
            wd->m_pFramesInFlight.reset(0);

            wd->m_pSwapchain->Destroy();
            wd->m_pSwapchain.reset(0);

            vkDestroySurfaceKHR(initInfo->instance, wd->m_Surface, nullptr);

            if (wd->m_RenderPass)
            {
                vkDestroyRenderPass(initInfo->device, wd->m_RenderPass, nullptr);
            }

            for (auto frameBuffer : wd->m_Framebuffers)
            {
                vkDestroyFramebuffer(initInfo->device, frameBuffer, nullptr);
            }

            IM_DELETE(vd);
            viewport->RendererUserData = nullptr;
        }
    }

    static void Renderer_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
    {
        //int width = 0, height = 0;
        //while (width == 0 || height == 0)
        //{
        //    GLFWwindow* window = (GLFWwindow*)viewport->PlatformHandle;
        //    // Window minimization a special case
        //    glfwGetFramebufferSize(window, &width, &height);
        //    glfwWaitEvents();
        //}

        VulkanViewportData* vd = (VulkanViewportData*)viewport->RendererUserData;
        VulkanWindow* wd = &vd->Window;
        CreateOrResizeWindow(wd,
            static_cast<uint32_t>(size.x),
            static_cast<uint32_t>(size.y));
    }

    std::optional<uint32_t> AcquireNextImage(Vk::SyncObject syncObject, ImGuiViewport* viewport)
    {
        VulkanViewportData* vd = (VulkanViewportData*)viewport->RendererUserData;
        VulkanWindow* wd = &vd->Window;

        vkWaitForFences(initInfo->device, 1, &syncObject.fence, VK_TRUE, UINT64_MAX);

        uint32_t frameBufferImageIndex;
        const VkResult result = wd->m_pSwapchain->AcquireNextImage(
            syncObject.imageAvailableSemaphore, &frameBufferImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // *VK_ERROR_OUT_OF_DATE_KHR* The swap chain has become incompatible with the surface
            // and can no longer be used for rendering. Usually happens after a window resize.
            // It can be returned by The vkAcquireNextImageKHR and vkQueuePresentKHR functions.
            //OnRecreateSwapchain();

            CreateOrResizeWindow(wd,
                static_cast<uint32_t>(viewport->Size.x),
                static_cast<uint32_t>(viewport->Size.y));

            return std::nullopt;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            VK_CHECK(result);

            // *VK_SUBOPTIMAL_KHR* The swap chain can still be used to successfully
            // present to the surface, but the surface properties are no longer matched exactly.
            // It can be returned by The vkAcquireNextImageKHR and vkQueuePresentKHR functions.

            throw std::runtime_error("failed to acquire swap chain image!");
        }

        return frameBufferImageIndex;
    }

    static void Renderer_RenderWindow(ImGuiViewport* viewport, void* renderArg)
    {
        VulkanViewportData* vd = (VulkanViewportData*)viewport->RendererUserData;
        VulkanWindow* wd = &vd->Window;

        ImDrawData* imDrawData = viewport->DrawData;
        const float fb_width = (imDrawData->DisplaySize.x * imDrawData->FramebufferScale.x);
        const float fb_height = (imDrawData->DisplaySize.y * imDrawData->FramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0)
        {
            return;
        }

        auto syncObject = wd->m_pFramesInFlight->NextSyncObject();
        const std::optional<uint32_t> imageIndexOpt = AcquireNextImage(syncObject, viewport);
        const uint32_t imageIndex = imageIndexOpt.value();
        wd->m_ImageIndex = imageIndex;

        const uint32_t frameIndex = wd->m_pFramesInFlight->GetCurrentFrameIndex();
        wd->m_pFramesInFlight->ResetFence(frameIndex);

        VkCommandBuffer cmdBuffer = wd->m_CommandBuffers[frameIndex];

        VkResult result;
        {
            result = vkResetCommandPool(initInfo->device, wd->m_CommandPool.GetVkCommandPool(), 0);
            VK_CHECK(result);

            VkCommandBufferBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = vkBeginCommandBuffer(cmdBuffer, &info);
            VK_CHECK(result);
        }
        {
            ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            memcpy(&wd->m_ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));
        }
        {
            VkRenderPassBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.renderPass = wd->m_RenderPass;
            info.framebuffer = wd->m_Framebuffers[imageIndex];
            info.renderArea.extent.width = (uint32_t)fb_width;
            info.renderArea.extent.height = (uint32_t)fb_height;
            info.clearValueCount = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? 0 : 1;
            info.pClearValues = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? nullptr : &wd->m_ClearValue;
            vkCmdBeginRenderPass(cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        }

        //////////////////////////////////////////////////////////////////////////
        // RenderDrawData - START
        // UpdateBuffers
        ImGuiUtils::UpdateBuffers(*initInfo, viewport->DrawData, vd->renderBuffers);

        //////////////////////////////////////////////////////////////////////////
        //SetupRenderState
        // Bind pipeline:
        const VkPipelineLayout pipelineLayout = bdDocking->pipelineLayout;
        const VkDescriptorSet descriptorSet = bdDocking->descriptorSet;
        const VkPipeline pipeline = bdDocking->pipeline;
        {
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout, 0, 1,
                &descriptorSet, 0, nullptr);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }
        // Bind Vertex And Index Buffer:
        if (viewport->DrawData->TotalVtxCount > 0)
        {
            VkBuffer vertex_buffers[1] = { vd->renderBuffers.vertexBuffer.m_Buffer };
            VkDeviceSize vertex_offset[1] = { 0 };

            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertex_buffers, vertex_offset);
            vkCmdBindIndexBuffer(cmdBuffer, vd->renderBuffers.indexBuffer.m_Buffer, 0,
                sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        }

        // Setup viewport:
        {
            VkViewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = (float)fb_width;
            viewport.height = (float)fb_height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        }

        // Setup scale and translation:
        // Our visible imgui space lies from imDrawData->DisplayPps (top left)
        // to imDrawData->DisplayPos+data_data->DisplaySize (bottom right).
        // DisplayPos is (0,0) for single viewport apps.
        {
            float scale[2];
            scale[0] = 2.0f / imDrawData->DisplaySize.x;
            scale[1] = 2.0f / imDrawData->DisplaySize.y;
            float translate[2];
            translate[0] = -1.0f - imDrawData->DisplayPos.x * scale[0];
            translate[1] = -1.0f - imDrawData->DisplayPos.y * scale[1];

            vkCmdPushConstants(cmdBuffer, pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
            
            vkCmdPushConstants(cmdBuffer, pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);

            // Setup scale and translation:
            // Our visible penguin space lies from draw_data->DisplayPps (top left) to
            // draw_data->DisplayPos+data_data->DisplaySize (bottom right).
            // DisplayPos is (0,0) for single viewport apps.
            //{
            //    m_PushConstBlock.scale = glm::vec2(2.0f / imDrawData->DisplaySize.x, 2.0f / imDrawData->DisplaySize.y);

            //    float translate[2];
            //    translate[0] = -1.0f - imDrawData->DisplayPos.x * m_PushConstBlock.scale[0];
            //    translate[1] = -1.0f - imDrawData->DisplayPos.y * m_PushConstBlock.scale[1];
            //    m_PushConstBlock.translate = glm::vec2(translate[0], translate[1]);

            //    vkCmdPushConstants(cmdBuffer, m_PipelineLayoutDocking, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantsBlock), &m_PushConstBlock);
            //}
        }

        //////////////////////////////////////////////////////////////////////////
        // Render command lists
        int32_t vertexOffset = 0;
        int32_t indexOffset = 0;

        // Will project scissor/clipping rectangles into framebuffer space
        ImVec2 clip_off = imDrawData->DisplayPos;         // (0,0) unless using multi-viewports
        ImVec2 clip_scale = imDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

        int32_t vtxOffset = 0;
        int32_t idxOffset = 0;

        for (uint32_t i = 0; i < (uint32_t)imDrawData->CmdListsCount; i++)
        {
            const ImDrawList* cmd_list = imDrawData->CmdLists[i];
            for (uint32_t cmd_i = 0; cmd_i < (uint32_t)cmd_list->CmdBuffer.Size; cmd_i++)
            {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                VkRect2D scissor;
                scissor.offset.x = (int32_t)(clip_min.x);
                scissor.offset.y = (int32_t)(clip_min.y);
                scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                // Draw
                vkCmdDrawIndexed(cmdBuffer, pcmd->ElemCount, 1, pcmd->IdxOffset + idxOffset, pcmd->VtxOffset + vtxOffset, 0);
            }
            idxOffset += cmd_list->IdxBuffer.Size;
            vtxOffset += cmd_list->VtxBuffer.Size;
        }

        VkRect2D scissor = { { 0, 0 }, { (uint32_t)fb_width, (uint32_t)fb_height } };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        // RenderDrawData - END
        //////////////////////////////////////////////////////////////////////////

        {
            vkCmdEndRenderPass(cmdBuffer);
        }
        {
            result = vkEndCommandBuffer(cmdBuffer);
            VK_CHECK(result);
        }

        //////////////////////////////////////////////////////////////////////////
        // Submit Queue
        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &syncObject.imageAvailableSemaphore;
        info.pWaitDstStageMask = &waitStages;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &cmdBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &syncObject.renderFinishedSemaphore;

        result = vkQueueSubmit(initInfo->graphicsQueue, 1, &info, syncObject.fence);
        VK_CHECK(result);
    }

    static void Renderer_SwapBuffers(ImGuiViewport* viewport, void* renderArg)
    {
        const VkQueue presentQueue = initInfo->graphicsQueue;

        VulkanViewportData* vd = (VulkanViewportData*)viewport->RendererUserData;
        VulkanWindow* wd = &vd->Window;

        VkResult result;
        uint32_t present_index = wd->m_pFramesInFlight->GetCurrentFrameIndex();
        Vk::SyncObject syncObject = wd->m_pFramesInFlight->LatestSyncObject();
        result = wd->m_pSwapchain->QueuePresent(presentQueue, wd->m_ImageIndex,
            syncObject.renderFinishedSemaphore);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR /*|| m_IsFramebufferResized*/)
        {
            //m_IsFramebufferResized = false;
            CreateOrResizeWindow(wd,
                static_cast<uint32_t>(viewport->Size.x),
                static_cast<uint32_t>(viewport->Size.y));
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }
}