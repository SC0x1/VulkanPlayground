#include "VKPlayground_PCH.h"

#include "vulkan/utils.h"
#include "vulkan/framesinflight.h"
#include "vulkan/swapchain.h"
#include "vulkan/surface.h"
#include "vulkan/context.h"

// Embedded font
#include "common/imgui/Roboto-Regular.embed"

#include "common/imgui/shadowimpl/imgui_shadow.h"
#include "common/imgui/shadowimpl/imgui_shadow_docking_impl.h"
#include "common/imgui/shadowimpl/imgui_shadow_impl.h"

ImGuiShadowVulkan* ImGuiShadowVulkan::Create(bool useDockingRenderer, bool useCustomRenderer)
{
    if (useDockingRenderer)
    {
        return new ImGuiShadowVulkanDockingImpl(useCustomRenderer);
    }
    else
    {
        return new ImGuiShadowVulkanImpl(useCustomRenderer);
    }

    return nullptr;
}

ImGuiShadowVulkan::~ImGuiShadowVulkan()
{
    Shutdown();
}

ImGuiShadowVulkan::ImGuiShadowVulkan(bool dockingRenderer, bool customRenderer)
    : m_UseCustomRenderer(customRenderer)
    , m_UseDockingRenderer(dockingRenderer)
{
}

void ImGuiShadowVulkan::CreateFontsTexture()
{
    if (m_IsUseDefaultFont == false)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
        io.FontDefault = robotoFont;
    }

    if (m_UseCustomRenderer == false)
    {
        VkCommandBuffer command_buffer = Vk::Utils::BeginSingleTimeCommands(
            m_InitInfo.device,
            m_CommandPool.GetVkCommandPool());

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        Vk::Utils::EndSingleTimeCommands(
            m_InitInfo.device,
            m_InitInfo.graphicsQueue,
            m_CommandPool.GetVkCommandPool(),
            command_buffer);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
}

void ImGuiShadowVulkan::Initialize(const ImGuiVulkanInitInfo& initInfo)
{
    m_InitInfo = initInfo;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();

    m_ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_ImGuiContext);

    // Config ImGui
    //SRS - Set ImGui font and style scale factors to handle retina and other HiDPI displays
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.FontGlobalScale = 1.0f;

    if (m_UseDockingRenderer)
    {
#if defined VP_IMGUI_VIEWPORTS_ENABLED
        // We can create multi-viewports on the Renderer side (optional)
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
        // Enable Multi-Viewport / Platform Windows
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif // VP_IMGUI_VIEWPORTS_ENABLED

#if defined VP_IMGUI_DOCKING_ENABLED
        // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif // VP_IMGUI_DOCKING_ENABLED
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);

    ImGui::StyleColorsDark();

    CreateCommandPool();
    CreateCommandBuffers(m_InitInfo.swapChainImageCount);

    if (m_UseCustomRenderer)
    {
        //CreateCommandBuffers();
    }
    else // ImGui Default Renderer API
    {
        CreateDescriptorPool();

        CreateRenderPass(m_RenderPassMain, m_InitInfo.swapchainImageFormat,
            VK_ATTACHMENT_LOAD_OP_LOAD);

        // Initializes ImGui for GLFW
        ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)initInfo.windowHandle, true);

        // Initializes ImGui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_InitInfo.instance;
        init_info.PhysicalDevice = m_InitInfo.physicalDevice;
        init_info.Device = m_InitInfo.device;
        init_info.QueueFamily = m_InitInfo.queueFamily;
        init_info.Queue = m_InitInfo.graphicsQueue;
        init_info.PipelineCache = nullptr; // TODO: PipelineCache
        init_info.DescriptorPool = m_DescriptorPool;
        init_info.Subpass = 0;
        init_info.Allocator = nullptr; // TODO: Allocator
        init_info.MinImageCount = m_InitInfo.swapChainImageCount;
        init_info.ImageCount = m_InitInfo.swapChainImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn = Vk::Utils::CheckVkResult;

        ImGui_ImplVulkan_Init(&init_info, m_RenderPassMain);
    }

    CreateFontsTexture();

    CreateFramebuffers();

    CreateSyncObjects();

    m_IsInitialized = true;
}

void ImGuiShadowVulkan::Shutdown()
{
    if (m_IsInitialized)
    {
        // Release resources for the custom renderer
        vkFreeCommandBuffers(m_InitInfo.device, m_CommandPool.GetVkCommandPool(),
            static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

        DestroyCommandPool();

        vkDestroyFence(m_InitInfo.device, m_FenceQueueSync, nullptr);

        if (m_RenderPassMain != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_InitInfo.device, m_RenderPassMain, nullptr);
        }

        if (m_UseCustomRenderer)
        {
            // De initialization of Custom Renderer Resources
        }
        else // Release Vulkan Resources for ImGui Default Renderer
        {
            ImGui_ImplVulkan_Shutdown();
        }

        for (auto framebuffer : m_Framebuffers)
        {
            vkDestroyFramebuffer(m_InitInfo.device, framebuffer, nullptr);
        }

        ImGui_ImplGlfw_Shutdown();

        ImGui::DestroyContext();

        m_IsInitialized = false;
    }
}

bool ImGuiShadowVulkan::IsInitialized() const
{
    return m_IsInitialized;
}

void ImGuiShadowVulkan::SetUseCustomRenderer(bool isCustomRenderer)
{
    m_UseCustomRenderer = isCustomRenderer;
}

void ImGuiShadowVulkan::SetUseDefaultFont(bool useDefaultFont)
{
    m_IsUseDefaultFont = useDefaultFont;
}

bool ImGuiShadowVulkan::IsCustomRenderer() const
{
    return m_UseCustomRenderer;
}

bool ImGuiShadowVulkan::IsDockingRenderer() const
{
    return m_UseDockingRenderer;
}

void ImGuiShadowVulkan::StartFrame()
{
    if (IsSkippingFrame())
    {
        return;
    }

    ImGui::SetCurrentContext(m_ImGuiContext);

    if (m_UseCustomRenderer)
    {
        // Setup frame for Custom Renderer
    }
    else
    {
        ImGui_ImplVulkan_NewFrame();
    }

    ImGui_ImplGlfw_NewFrame();

    // Call NewFrame(), after this point you can use ImGui::* functions anytime.
    // You should calling NewFrame() as early as you can
    // to be able to use ImGui everywhere.
    ImGui::NewFrame();
}

void ImGuiShadowVulkan::EndFrame(uint32_t frameIndex, uint32_t imageIndex)
{
    // You want to try calling EndFrame/Render as late as you can.
    ImGui::EndFrame();
    // Render to generate draw buffers
    ImGui::Render();

    ImDrawData* main_draw_data = ImGui::GetDrawData();

    RecordCommandBuffer(m_CommandBuffers[frameIndex], imageIndex, main_draw_data);

    SubmitCommandBuffer(m_CommandBuffers[frameIndex]);
}

void ImGuiShadowVulkan::OnRecreateSwapchain(const Vk::SwapChain& swapChain)
{
    if (m_UseCustomRenderer == false)
    {
        ImGui_ImplVulkan_SetMinImageCount(m_InitInfo.swapChainImageCount);
    }

    m_InitInfo.swapChainExtend = swapChain.GetExtent();
    m_InitInfo.swapChainImageViews = swapChain.GetVkImageViews();

    // Destroy Framebuffers
    for (auto framebuffer : m_Framebuffers)
    {
        vkDestroyFramebuffer(m_InitInfo.device, framebuffer, nullptr);
    }

    vkFreeCommandBuffers(m_InitInfo.device, m_CommandPool.GetVkCommandPool(),
        static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

    DestroyCommandPool();

    vkDestroyRenderPass(m_InitInfo.device, m_RenderPassMain, nullptr);

    CreateRenderPass(m_RenderPassMain, m_InitInfo.swapchainImageFormat, VK_ATTACHMENT_LOAD_OP_LOAD);

    CreateCommandPool();
    CreateCommandBuffers(m_InitInfo.swapChainImageCount);

    CreateFramebuffers();
}

bool ImGuiShadowVulkan::IsSkippingFrame() const
{
    return m_IsSkippingFrame;
}

void ImGuiShadowVulkan::CreateDescriptorPool()
{
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, ImGuiPoolSize_Default },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       ImGuiPoolSize_Default }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = ImGuiPoolSize_Default * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(m_InitInfo.device, &pool_info, nullptr, &m_DescriptorPool));
}

void ImGuiShadowVulkan::DestroyDescriptorPool()
{
    vkDestroyDescriptorPool(m_InitInfo.device, m_DescriptorPool, nullptr);
}

void ImGuiShadowVulkan::CreateSyncObjects()
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FLAGS_NONE;
    VK_CHECK(vkCreateFence(m_InitInfo.device, &fenceInfo, nullptr, &m_FenceQueueSync));
}

void ImGuiShadowVulkan::CreateCommandPool()
{
    m_CommandPool.Create(m_InitInfo.device, m_InitInfo.queueFamily,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

void ImGuiShadowVulkan::DestroyCommandPool()
{
    m_CommandPool.Destroy(m_InitInfo.device);
}

void ImGuiShadowVulkan::CreateFramebuffers()
{
    m_Framebuffers.resize(m_InitInfo.swapChainImageCount);

    VkImageView attachment[1];
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.renderPass = m_RenderPassMain;
    info.attachmentCount = 1;
    info.pAttachments = attachment;
    info.width = m_InitInfo.swapChainExtend.width;
    info.height = m_InitInfo.swapChainExtend.height;
    info.layers = 1;

    for (uint32_t i = 0; i < m_InitInfo.swapChainImageCount; ++i)
    {
        attachment[0] = m_InitInfo.swapChainImageViews[i];
        if (vkCreateFramebuffer(m_InitInfo.device, &info, nullptr, &m_Framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Unable to create UI framebuffers!");
        }
    }
}

void ImGuiShadowVulkan::CreateCommandBuffers(uint32_t numCommandBuffers)
{
    m_CommandBuffers.resize(numCommandBuffers);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_CommandPool.GetVkCommandPool();
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = numCommandBuffers;

    VK_CHECK(vkAllocateCommandBuffers(m_InitInfo.device, &commandBufferAllocateInfo, m_CommandBuffers.data()));
}

void ImGuiShadowVulkan::CreateRenderPass(VkRenderPass& renderPass, VkFormat format, VkAttachmentLoadOp loadOp)
{
    VkAttachmentDescription attachment = {};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // Need UI to be drawn on top of main
    attachment.loadOp = loadOp;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // Because this render pass is the last one, we want finalLayout to be set to
    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkSubpassDependency subpassDependency = {};
    // VK_SUBPASS_EXTERNAL to create a dependency outside the current render pass
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT - since we want the pixels to be already
    // written to the framebuffer. We can also set our dstStageMask to this same value because
    // our GUI will also be drawn to the same target.
    // We're basically waiting for pixels to be written before we can write pixels ourselves.
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // Wait on writes
    subpassDependency.srcAccessMask = 0;// VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &subpassDependency;

    VK_CHECK(vkCreateRenderPass(m_InitInfo.device, &renderPassInfo, nullptr, &renderPass));
}

void ImGuiShadowVulkan::RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, ImDrawData* draw_data)
{
    VK_CHECK(vkResetCommandBuffer(cmdBuffer, 0));

    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &commandBufferBeginInfo));

    VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_RenderPassMain;
    renderPassBeginInfo.framebuffer = m_Framebuffers[imageIndex];
    renderPassBeginInfo.renderArea.extent = m_InitInfo.swapChainExtend;
    renderPassBeginInfo.renderArea.offset = { 0,0 };
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Grab and record the draw data for Dear ImGui
    if (m_UseCustomRenderer == false)
    {
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmdBuffer);
    }
    else
    {
        // Draw Custom ImGui Frame
    }

    vkCmdEndRenderPass(cmdBuffer);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));
}

void ImGuiShadowVulkan::SubmitCommandBuffer(VkCommandBuffer cmdBuffer)
{
    vkResetFences(m_InitInfo.device, 1, &m_FenceQueueSync);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VK_CHECK(vkQueueSubmit(m_InitInfo.graphicsQueue, 1, &submitInfo, m_FenceQueueSync));

    VK_CHECK(vkWaitForFences(m_InitInfo.device, 1, &m_FenceQueueSync, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
}