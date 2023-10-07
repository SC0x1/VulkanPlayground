#include "VKPlayground_PCH.h"
#include "common/imgui/ImGuiConfig.h"

#include "example_app.h"

#include "vulkan/utils.h"
#include "vulkan/framesinflight.h"
#include "vulkan/swapchain.h"
#include "vulkan/surface.h"
#include "vulkan/context.h"

#include "ImGUIVulkan.h"
#include "ImGuizmo.h"

#include "ImGuiShadersSpirV.inl"

// Embedded font
#include "common/imgui/Roboto-Regular.embed"

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

namespace ImGuiInternal
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

        //int32_t Width;                    // already in Vk::SwapChain
        //int32_t Height;                   // already in Vk::SwapChain
        //VkSwapchainKHR Swapchain;         // already in Vk::SwapChain
        //VkSurfaceKHR Surface;             // already in Vk::Surface
        //VkSurfaceFormatKHR SurfaceFormat; // already in Vk::SwapChain
        //VkPresentModeKHR PresentMode;     // already in Vk::SwapChain
        
        VkRenderPass m_RenderPass{};
        VkPipeline m_Pipeline{};            // The window pipeline may uses a different VkRenderPass than the one passed in ImGui_ImplVulkan_InitInfo

        VkClearValue m_ClearValue{};

        uint32_t m_QueueFamily = 0;
        uint32_t m_FrameIndex = 0;
        uint32_t m_ImageCount = 0;
        uint32_t m_ImageIndex = 0;

        bool m_ClearEnable{};
    };

    // For multi-viewport support:
    // Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend data.
    struct VulkanViewportData
    {
        bool WindowOwned= false;
        VulkanWindow Window;             // Used by secondary Viewports only
        //VulkanWindowRenderBuffers RenderBuffers{};      // Used by all Viewports

        Vk::Buffer m_VertexBuffer;
        uint32_t m_VertexCount = 0;

        Vk::Buffer m_IndexBuffer;
        uint32_t m_IndexCount = 0;

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

    static VkPipelineCache pipelineCacheDocking = VK_NULL_HANDLE;
    static VkPipelineLayout pipelineLayoutDocking = VK_NULL_HANDLE;
    static VkPipeline pipelineDocking = VK_NULL_HANDLE;

    static VkDescriptorSet descriptorSetDocking = VK_NULL_HANDLE;

    static VkInstance instance = VK_NULL_HANDLE;
    static VkQueue graphicsQueue = VK_NULL_HANDLE;
    static VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    static VkDevice device = VK_NULL_HANDLE;
};

ImGuiRenderer* ImGuiRenderer::m_This = nullptr;

ImGuiRenderer::ImGuiRenderer()
{
    m_This = this;
}

bool ImGuiRenderer::Initialize(VulkanBaseApp* app)
{
    if (app == nullptr)
    {
        return false;
    }

    m_App = app;

    void* window = m_App->GetWindowHandle();

    if (window == nullptr)
    {
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();

    m_ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_ImGuiContext);

    // Config ImGui
    //SRS - Set ImGui font and style scale factors to handle retina and other HiDPI displays
    ImGuiIO& io = ImGui::GetIO(); (void)io;

#if defined VP_IMGUI_VIEWPORTS_ENABLED
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)
#endif

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls

#if defined VP_IMGUI_DOCKING_ENABLED
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
#endif // VP_IMGUI_DOCKING_ENABLED

#if defined VP_IMGUI_VIEWPORTS_ENABLED
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
#endif // VP_IMGUI_VIEWPORTS_ENABLED

    io.FontGlobalScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);

    ImGui::StyleColorsDark();

    ImGuiInternal::instance = m_App->GetVkInstance();
    ImGuiInternal::device = m_App->GetVkDevice();
    ImGuiInternal::physicalDevice = m_App->GetVkPhysicalDevice();
    ImGuiInternal::graphicsQueue = m_App->GetGraphicsQueue();

    const uint32_t queueFamily = m_App->GetQueueFamilyIndices().graphicsFamily.value();
    const uint32_t imageCount = m_App->GetSwapChainImageCount();

    CreateDescriptorPool();

    CreateRenderPass(m_RenderPassMain, m_App->GetSwapchainImageFormat(), VK_ATTACHMENT_LOAD_OP_LOAD);
    CreateRenderPass(m_RenderPassDocking, VK_FORMAT_B8G8R8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR);

    CreateFramebuffers();

    CreateSyncObjects();

    //this initializes ImGui for GLFW
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window, true);

    if (m_UseDefaultRenderer)
    {
        //this initializes ImGui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = ImGuiInternal::instance;
        init_info.PhysicalDevice = ImGuiInternal::physicalDevice;
        init_info.Device = ImGuiInternal::device;
        init_info.QueueFamily = queueFamily;
        init_info.Queue = ImGuiInternal::graphicsQueue;
        init_info.PipelineCache = nullptr; // TODO: PipelineCache
        init_info.DescriptorPool = m_DescriptorPool;
        init_info.Subpass = 0;
        init_info.Allocator = nullptr; // TODO: Allocator
        init_info.MinImageCount = imageCount;
        init_info.ImageCount = imageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn = Vk::Utils::CheckVkResult;
        ImGui_ImplVulkan_Init(&init_info, m_RenderPassMain);
    }
    else if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        InitializeRendererCallbacks();
    }

    CreateFontsTexture();

    CreateCommandPool();
    CreateCommandBuffers();

    // Need to create ImGui pipeline only in a case of Multi-Viewport feature enabled
    CreatePipeline(m_PipelineMain, m_RenderPassMain);
    CreatePipeline(m_PipelineDocking, m_RenderPassDocking);

    ImGuiInternal::pipelineCacheDocking = m_PipelineCacheDocking;
    ImGuiInternal::pipelineLayoutDocking = m_PipelineLayoutDocking;
    ImGuiInternal::pipelineDocking = m_PipelineDocking;
    ImGuiInternal::descriptorSetDocking = m_DescriptorSetDocking;

    ImGui::GetIO().BackendRendererUserData = app;

    m_VertexBuffer.m_Device = ImGuiInternal::device;
    m_IndexBuffer.m_Device = ImGuiInternal::device;
    return true;
}

void ImGuiRenderer::Shutdown()
{
    assert(m_App);

    const VkDevice device = ImGuiInternal::device;

    // Cleanup DearImGui
    if (m_UseDefaultRenderer)
    {
        ImGui_ImplVulkan_Shutdown();
    }

    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    {
        if (m_ShaderModuleVertDocking)
        {
            vkDestroyShaderModule(device, m_ShaderModuleVertDocking, nullptr);
            m_ShaderModuleVertDocking = VK_NULL_HANDLE;
        }

        if (m_ShaderModuleFragDocking)
        {
            vkDestroyShaderModule(device, m_ShaderModuleFragDocking, nullptr);
            m_ShaderModuleFragDocking = VK_NULL_HANDLE;
        }

        // Destroy Font resources
        vkDestroyImage(device, m_FontImage, nullptr);
        vkDestroyImageView(device, m_FontView, nullptr);
        vkFreeMemory(device, m_FontMemory, nullptr);
        vkDestroySampler(device, m_Sampler, nullptr);

        // Pipeline
        vkDestroyPipelineCache(device, m_PipelineCacheDocking, nullptr);

        vkDestroyPipeline(device, m_PipelineMain, nullptr);
        vkDestroyPipeline(device, m_PipelineDocking, nullptr);

        vkDestroyPipelineLayout(device, m_PipelineLayoutDocking, nullptr);
        vkDestroyDescriptorPool(device, m_DescriptorPoolDocking, nullptr);
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayoutDocking, nullptr);

        m_VertexBuffer.Destroy();
        m_IndexBuffer.Destroy();

        vkDestroyFence(device, m_FenceQueueSync, nullptr);
    }

    vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
    vkFreeCommandBuffers(device, m_CommandPool.GetVkCommandPool(), static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

    DestroyCommandPool();

    // Render Pass
    vkDestroyRenderPass(device, m_RenderPassMain, nullptr);
    vkDestroyRenderPass(device, m_RenderPassDocking, nullptr);
    
    for (auto framebuffer : m_Framebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
}

void ImGuiRenderer::StartFrame()
{
    if (IsSkippingFrame())
    {
        return;
    }

    ImGui::SetCurrentContext(m_ImGuiContext);

    // Start the Dear ImGui frame
    if (m_UseDefaultRenderer)
    {
        ImGui_ImplVulkan_NewFrame();
    }

    ImGui_ImplGlfw_NewFrame();

    // Call NewFrame(), after this point you can use ImGui::* functions anytime.
    // You should calling NewFrame() as early as you can to be able to use
    // ImGui everywhere.
    ImGui::NewFrame();

    // SRS - Set initial position of default Debug window (note: Debug window sets its own initial size, use ImGuiSetCond_Always to override)
    ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_Always);

    ImGui::ShowDemoWindow();
    //ImGuizmo::BeginFrame(); // TODO: ImGuizmo
}

void ImGuiRenderer::EndFrame(uint32_t frameIndex, uint32_t imageIndex)
{
    // You want to try calling EndFrame/Render as late as you can.
    ImGui::EndFrame();
    // Render to generate draw buffers
    ImGui::Render();

    ImDrawData* main_draw_data = ImGui::GetDrawData();

    UpdateBuffers(main_draw_data, m_VertexBuffer, m_VertexCount, m_IndexBuffer, m_IndexCount);

    RecordCommandBuffer(m_CommandBuffers[frameIndex], imageIndex, main_draw_data);

    SubmitCommandBuffer(m_CommandBuffers[frameIndex]);

#if defined VP_IMGUI_VIEWPORTS_ENABLED
    ImGui::SetCurrentContext(m_ImGuiContext);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        if (m_UseDefaultRenderer)
        {
            //TODO: RenderPlatformWindowsDefault -> custom implementation
            ImGui::RenderPlatformWindowsDefault();
        }
        else
        {
            //RenderViewports();
        }
    }

#endif // VP_IMGUI_VIEWPORTS_ENABLED
}

void ImGuiRenderer::OnRecreateSwapchain()
{
    assert(m_App);

    const VkDevice device = ImGuiInternal::device;
    const uint32_t imageCount = m_App->GetSwapChainImageCount();
    // Destroy Framebuffers
    for (auto framebuffer : m_Framebuffers)
    {
        vkDestroyFramebuffer(m_App->GetVkDevice(), framebuffer, nullptr);
    }

    vkFreeCommandBuffers(device, m_CommandPool.GetVkCommandPool(),
        static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

    DestroyCommandPool();

    vkDestroyRenderPass(device, m_RenderPassMain, nullptr);

    CreateRenderPass(m_RenderPassMain, m_App->GetSwapchainImageFormat(), VK_ATTACHMENT_LOAD_OP_LOAD);

    if (m_UseDefaultRenderer)
    {
        ImGui_ImplVulkan_SetMinImageCount(imageCount);
    }

    CreateCommandPool();
    CreateCommandBuffers();
    CreateFramebuffers();
}

bool ImGuiRenderer::IsSkippingFrame() const
{
    return m_IsSkippingFrame;
}

VulkanBaseApp* ImGuiRenderer::GetVulkanBaseApp() const
{
    return m_App;
}

void ImGuiRenderer::InitializePlatformCallbacks()
{
    //ImGuiPlatformIO& io = ImGui::GetPlatformIO();

    //io.Platform_CreateWindow = OnCreateWindow;
    //io.Platform_DestroyWindow = OnDestroyWindow;
    //io.Platform_ShowWindow =   OnShowWindow;
    //io.Platform_SetWindowPos = OnSetWindowPos;
    //io.Platform_GetWindowPos = OnGetWindowPos;
    //io.Platform_SetWindowSize = OnSetWindowSize;
    //io.Platform_GetWindowSize = OnGetWindowSize;
    //io.Platform_SetWindowFocus = OnSetWindowFocus;
    //io.Platform_GetWindowFocus = OnGetWindowFocus;
    //io.Platform_SetWindowTitle = OnSetWindowTitle;
}

void ImGuiRenderer::InitializeRendererCallbacks()
{
    //TOOD: Implement: OnCreateWindow / OnDestroyWindow / OnSetWindowSize
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    platform_io.Renderer_CreateWindow = ImGuiInternal::Renderer_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGuiInternal::Renderer_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGuiInternal::Renderer_SetWindowSize;

    if (m_UseDefaultRenderer == false)
    {
        platform_io.Renderer_RenderWindow = ImGuiInternal::Renderer_RenderWindow;
        platform_io.Renderer_SwapBuffers = ImGuiInternal::Renderer_SwapBuffers;
    }

    /*
    platform_io.Renderer_CreateWindow  = __ImGUIRendererCreateWindowCallback;
    platform_io.Renderer_DestroyWindow = __ImGUIPlatformDestroyWindowCallback;
    platform_io.Renderer_RenderWindow  = __ImGUIPlatformRenderWindowCallback;
    platform_io.Renderer_SetWindowSize = __ImGUIPlatformSetWindowSizeCallback;
    platform_io.Renderer_SwapBuffers   = __ImGUIPlatformSwapBuffersCallback;
    */
}

void ImGuiRenderer::CreateFontsTexture()
{
    assert(m_App);

    const VkDevice device = ImGuiInternal::device;
    const VkPhysicalDevice physicalDevice = ImGuiInternal::physicalDevice;

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
    io.FontDefault = robotoFont;
    
    // Load default font
    if (m_UseDefaultRenderer)
    {
        // Upload Fonts
        VkCommandBuffer command_buffer = m_App->BeginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
        m_App->EndSingleTimeCommands(command_buffer);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    // Create font texture
    unsigned char* fontData;
    int32_t texWidth, texHeight;
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    const VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

    // Create target image for copy
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = texWidth;
    imageInfo.extent.height = texHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &m_FontImage));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_FontImage, &memReqs);
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = Vk::Utils::FindMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(device, &memAllocInfo, nullptr, &m_FontMemory));
    VK_CHECK(vkBindImageMemory(device, m_FontImage, m_FontMemory, 0));

    // Image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_FontImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_FontView));

    // Staging buffers for font data upload
    Vk::Buffer stagingBuffer;
    stagingBuffer.m_Device = device;

    Vk::Utils::CreateBuffer(device, physicalDevice, uploadSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer.m_Buffer, &stagingBuffer.m_Memory);

    stagingBuffer.Map();
    memcpy(stagingBuffer.m_Mapped, fontData, uploadSize);
    stagingBuffer.UnMap();

    // Copy buffer data to font image
    VkCommandBuffer command_buffer = m_App->BeginSingleTimeCommands();
    {
        // Prepare for transfer
        Vk::Utils::SetImageLayout(
            command_buffer,
            m_FontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Copy
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = texWidth;
        bufferCopyRegion.imageExtent.height = texHeight;
        bufferCopyRegion.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(
            command_buffer,
            stagingBuffer.m_Buffer,
            m_FontImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
        );

        // Prepare for shader read
        Vk::Utils::SetImageLayout(
            command_buffer,
            m_FontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
    m_App->EndSingleTimeCommands(command_buffer);

    stagingBuffer.Destroy();

    // Font texture Sampler
    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler));
}

void ImGuiRenderer::RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, ImDrawData* draw_data)
{
    assert(m_App);

    const VkDevice device = ImGuiInternal::device;
    const VkExtent2D extend = m_App->GetSwapChainExtend();

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
    renderPassBeginInfo.renderArea.extent = extend;
    renderPassBeginInfo.renderArea.offset = {0,0};
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Grab and record the draw data for Dear ImGui
    if (m_UseDefaultRenderer)
    {
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmdBuffer);
    }

    // Draw Main Frame
    {
        ImDrawData* imDrawData = ImGui::GetDrawData();
        DrawFrameMain(imDrawData, cmdBuffer);
    }

    vkCmdEndRenderPass(cmdBuffer);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));
}

void ImGuiRenderer::SubmitCommandBuffer(VkCommandBuffer cmdBuffer)
{
    const VkDevice device = ImGuiInternal::device;
    const VkQueue graphicsQueue = m_App->GetGraphicsQueue();

    vkResetFences(device, 1, &m_FenceQueueSync);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_FenceQueueSync));

    VK_CHECK(vkWaitForFences(device, 1, &m_FenceQueueSync, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
}

void ImGuiRenderer::CreateRenderPass(VkRenderPass& renderPass, VkFormat format, VkAttachmentLoadOp loadOp)
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

    VK_CHECK(vkCreateRenderPass(ImGuiInternal::device, &renderPassInfo, nullptr, &renderPass));
}

void ImGuiRenderer::CreateDescriptorPool()
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

    VK_CHECK(vkCreateDescriptorPool(ImGuiInternal::device, &pool_info, nullptr, &m_DescriptorPool));
}

void ImGuiRenderer::CreateFramebuffers()
{
    assert(m_App);
    const VkDevice device = ImGuiInternal::device;
    const VkExtent2D extend = m_App->GetSwapChainExtend();
    const std::vector<VkImageView>& swapChainImageViews = m_App->GetSwapChainImageViews();
    const std::vector<VkImage>& swapChainImages = m_App->GetSwapChainImages();

    m_Framebuffers.resize(swapChainImages.size());

    VkImageView attachment[1];
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext = nullptr,
    info.flags = 0,
    info.renderPass = m_RenderPassMain;
    info.attachmentCount = 1;
    info.pAttachments = attachment;
    info.width = extend.width;
    info.height = extend.height;
    info.layers = 1;

    for (uint32_t i = 0; i < swapChainImages.size(); ++i)
    {
        attachment[0] = swapChainImageViews[i];
        if (vkCreateFramebuffer(device, &info, nullptr, &m_Framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Unable to create UI framebuffers!");
        }
    }
}

void ImGuiRenderer::CreateCommandBuffers()
{
    const uint32_t swapchainImages = static_cast<uint32_t>(m_App->GetSwapChainImages().size());

    m_CommandBuffers.resize(swapchainImages);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_CommandPool.GetVkCommandPool();
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = swapchainImages;

    VK_CHECK(vkAllocateCommandBuffers(ImGuiInternal::device, &commandBufferAllocateInfo, m_CommandBuffers.data()));
}

void ImGuiRenderer::CreateSyncObjects()
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FLAGS_NONE;
    VK_CHECK(vkCreateFence(ImGuiInternal::device, &fenceInfo, nullptr, &m_FenceQueueSync));
}

void ImGuiRenderer::CreateCommandPool()
{
    const uint32_t queueFamily = m_App->GetQueueFamilyIndices().graphicsFamily.value();

    m_CommandPool.Create(ImGuiInternal::device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

void ImGuiRenderer::DestroyCommandPool()
{
    m_CommandPool.Destroy(ImGuiInternal::device);
}

void ImGuiRenderer::CreatePipeline(VkPipeline& pipeline, const VkRenderPass renderPass)
{
    const VkDevice device = ImGuiInternal::device;

    // Descriptor pool

    if (m_DescriptorPoolDocking == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorPoolSize> poolSizes =
        {
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
            }
        };

        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        descriptorPoolInfo.maxSets = 2;

        VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_DescriptorPoolDocking));
    }

    // Descriptor set layout
    if (m_DescriptorSetLayoutDocking == VK_NULL_HANDLE)
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            Vk::Init::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        };

        VkDescriptorSetLayoutCreateInfo descriptorLayout = Vk::Init::DescriptorSetLayoutCreateInfo(&setLayoutBindings[0], 1);
        VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_DescriptorSetLayoutDocking));
    }

    // Descriptor set
    if (m_DescriptorSetDocking == VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo allocInfo = Vk::Init::DescriptorSetAllocateInfo(m_DescriptorPoolDocking, &m_DescriptorSetLayoutDocking, 1);

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSetDocking));

        VkDescriptorImageInfo fontDescriptor{};
        fontDescriptor.sampler = m_Sampler;
        fontDescriptor.imageView = m_FontView;
        fontDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets =
        {
            Vk::Init::WriteDescriptorSet(m_DescriptorSetDocking, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
        };
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }

    // Pipeline cache
    if (m_PipelineCacheDocking == VK_NULL_HANDLE)
    {
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

        VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCacheDocking));

    }

    // Pipeline layout
    if (m_PipelineLayoutDocking == VK_NULL_HANDLE)
    {
        // Push constants for UI rendering parameters
        VkPushConstantRange pushConstantRange = Vk::Init::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstantsBlock), 0);
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Vk::Init::PipelineLayoutCreateInfo(&m_DescriptorSetLayoutDocking, 1);
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayoutDocking));
    }

    // PRIMITIVE TOPOLOGY
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Vk::Init::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Vk::Init::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    // Blending
    VkPipelineColorBlendAttachmentState blendAttachmentState{};
    blendAttachmentState.blendEnable = VK_TRUE;
    blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        Vk::Init::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Vk::Init::PipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState =
        Vk::Init::PipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        Vk::Init::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

    const std::vector<VkDynamicState> dynamicStateEnables =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState =
        Vk::Init::PipelineDynamicStateCreateInfo(&dynamicStateEnables[0], 2);

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};

    if (m_ShaderModuleVertDocking == VK_NULL_HANDLE)
    {
        VkShaderModuleCreateInfo vert_info = {};
        vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vert_info.codeSize = sizeof(__glsl_shader_vert_spv__);
        vert_info.pCode = (uint32_t*)__glsl_shader_vert_spv__;
        VK_CHECK(vkCreateShaderModule(device, &vert_info, nullptr, &m_ShaderModuleVertDocking));
    }

    if (m_ShaderModuleFragDocking == VK_NULL_HANDLE)
    {
        VkShaderModuleCreateInfo frag_info = {};
        frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        frag_info.codeSize = sizeof(__glsl_shader_frag_spv__);
        frag_info.pCode = (uint32_t*)__glsl_shader_frag_spv__;
        VK_CHECK(vkCreateShaderModule(device, &frag_info, nullptr, &m_ShaderModuleFragDocking));
    }

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = m_ShaderModuleVertDocking;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = m_ShaderModuleFragDocking;
    shaderStages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vk::Init::PipelineCreateInfo(m_PipelineLayoutDocking, renderPass);

    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;

    // Vertex bindings an attributes based on ImGui vertex definition
    std::vector<VkVertexInputBindingDescription> vertexInputBindings =
    {
        Vk::Init::VertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes =
    {
        Vk::Init::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),  // Location 0: Position
        Vk::Init::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),   // Location 1: UV
        Vk::Init::VertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)), // Location 0: Color
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState = Vk::Init::PipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    pipelineCreateInfo.pVertexInputState = &vertexInputState;

    VK_CHECK(vkCreateGraphicsPipelines(device, m_PipelineCacheDocking, 1, &pipelineCreateInfo, nullptr, &pipeline));
}

void ImGuiRenderer::UpdateBuffers(ImDrawData* imDrawData, Vk::Buffer& vertexBuffer,
    uint32_t& vertexCount, Vk::Buffer& indexBuffer, uint32_t& indexCount)
{
    const VkDevice device = ImGuiInternal::device;
    const VkPhysicalDevice physicalDevice = ImGuiInternal::physicalDevice;

    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Note: Alignment is done inside buffer creation
    const VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    const VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    if ((vertexBufferSize == 0) || (indexBufferSize == 0))
    {
        return;
    }

    // Update buffers only if vertex or index count has been changed compared to current buffer size

    size_t vertex_size = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    size_t index_size = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    // Vertex buffer
    if ((vertexBuffer.m_Buffer == VK_NULL_HANDLE) || (vertexCount < (uint32_t)imDrawData->TotalVtxCount))
    {
        vertexBuffer.m_Device = device;

        vertexBuffer.UnMap();
        vertexBuffer.Destroy();

        Vk::Utils::CreateBuffer(device, physicalDevice, vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &vertexBuffer.m_Buffer, &vertexBuffer.m_Memory);

        vertexCount = imDrawData->TotalVtxCount;
        vertexBuffer.Map();
    }

    // Index buffer
    if ((indexBuffer.m_Buffer == VK_NULL_HANDLE) ||
        (indexCount < (uint32_t)imDrawData->TotalIdxCount))
    {
        indexBuffer.m_Device = device;

        indexBuffer.UnMap();
        indexBuffer.Destroy();

        Vk::Utils::CreateBuffer(device, physicalDevice, indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &indexBuffer.m_Buffer, &indexBuffer.m_Memory);

        indexCount = imDrawData->TotalIdxCount;
        indexBuffer.Map();
    }

    // Upload data
    ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer.m_Mapped;
    ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer.m_Mapped;

    for (int n = 0; n < imDrawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush to make writes visible to GPU
    vertexBuffer.Flush();
    indexBuffer.Flush();
}

void ImGuiRenderer::DrawFrameMain(ImDrawData* imDrawData, VkCommandBuffer cmdBuffer)
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGuiIO& io = ImGui::GetIO();

    float width = ImGui::GetIO().DisplaySize.x;
    float height = ImGui::GetIO().DisplaySize.y;

    if (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports)
    {
        ImGuiViewport* vp = platform_io.Viewports[0];
        imDrawData = vp->DrawData;

        float fb_width = (imDrawData->DisplaySize.x * imDrawData->FramebufferScale.x);
        float fb_height = (imDrawData->DisplaySize.y * imDrawData->FramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0)
        {
            return;
        }

        width = fb_width;
        height = fb_height;
    }

    DrawFrame(imDrawData, cmdBuffer, m_VertexBuffer, m_IndexBuffer, width, height);
}

void ImGuiRenderer::DrawFrameViewport(ImGuiViewport* vp, VkCommandBuffer cmdBuffer)
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports && vp)
    {
        ImDrawData* imDrawData = vp->DrawData;

        const float fb_width = (imDrawData->DisplaySize.x * imDrawData->FramebufferScale.x);
        const float fb_height = (imDrawData->DisplaySize.y * imDrawData->FramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0)
        {
            return;
        }

        auto vd = (ImGuiInternal::VulkanViewportData*)vp->RendererUserData;
        auto wd = &vd->Window;

        DrawFrame(imDrawData, cmdBuffer, vd->m_VertexBuffer, vd->m_IndexBuffer,
            fb_width, fb_height);
    }
}

void ImGuiRenderer::DrawFrame(ImDrawData* imDrawData, VkCommandBuffer cmdBuffer,
    Vk::Buffer& vertexBuffer, Vk::Buffer& indexBuffer, float width, float height)
{
    ImGuiIO& io = ImGui::GetIO();

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayoutDocking, 0, 1, &m_DescriptorSetDocking, 0, nullptr);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineMain);

    VkViewport viewport{};
    viewport.width = width;
    viewport.height = height;
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

            vkCmdPushConstants(cmdBuffer, m_PipelineLayoutDocking, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantsBlock), &m_PushConstBlock);
        }
    }
    else
    {
        m_PushConstBlock.scale = glm::vec2(2.0f / width, 2.0f / height);
        m_PushConstBlock.translate = glm::vec2(-1.0f);
        vkCmdPushConstants(cmdBuffer, m_PipelineLayoutDocking, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantsBlock), &m_PushConstBlock);
    }

    // Render commands
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    if (imDrawData->CmdListsCount > 0)
    {
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer.m_Buffer, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, indexBuffer.m_Buffer, 0, VK_INDEX_TYPE_UINT16);

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
                    if (clip_max.x > width) { clip_max.x = (float)width; }
                    if (clip_max.y > height) { clip_max.y = (float)height; }
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

            VkRect2D scissor = { { 0, 0 }, { (uint32_t)width, (uint32_t)height } };
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        }
        else
        {
            for (uint32_t i = 0; i < (uint32_t)imDrawData->CmdListsCount; i++)
            {
                const ImDrawList* cmd_list = imDrawData->CmdLists[i];
                for (uint32_t j = 0; j < (uint32_t)cmd_list->CmdBuffer.Size; j++)
                {
                    const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
                    VkRect2D scissorRect;
                    scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
                    scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
                    scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                    scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                    vkCmdSetScissor(cmdBuffer, 0, 1, &scissorRect);
                    vkCmdDrawIndexed(cmdBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                    indexOffset += pcmd->ElemCount;
                }
                vertexOffset += cmd_list->VtxBuffer.Size;
            }
        }
    }
}

void ImGuiRenderer::RenderViewports()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGuiIO& io = ImGui::GetIO();

    if (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports)
    {
        for (int i = 1; i < platform_io.Viewports.Size; i++)
        {
            if ((platform_io.Viewports[i]->Flags & ImGuiViewportFlags_IsMinimized) == 0)
            {
                ImGuiViewport* vp = platform_io.Viewports[i];
                ImDrawData* drawData = vp->DrawData;

                auto* vd = (ImGuiInternal::VulkanViewportData*)vp->RendererUserData;
                if (vd != nullptr)
                {
                    UpdateBuffers(drawData, vd->m_VertexBuffer, vd->m_VertexCount, vd->m_IndexBuffer, vd->m_IndexCount);

                    if (drawData->CmdListsCount > 0)
                    {
                        drawData->CmdLists;
                        VkDeviceSize offsets[1] = { 0 };
                        //vkCmdBindVertexBuffers(m_CommandBuffers[frameIndex], 0, 1, &vertexBuffer.buffer, offsets);
                        //vkCmdBindIndexBuffer(m_CommandBuffers[frameIndex], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
                    }

                    // MyRenderFunction(platform_io.Viewports[i], my_args);
                }
            }
        }

        for (int i = 1; i < platform_io.Viewports.Size; i++)
        {
            if ((platform_io.Viewports[i]->Flags & ImGuiViewportFlags_IsMinimized) == 0)
            {
                // MySwapBufferFunction(platform_io.Viewports[i], my_args);
            }
        }
    }
}

namespace ImGuiInternal
{
    //void RecreateSwapchain(VulkanWindow* wd, const VkAllocationCallbacks* allocator, uint32_t width, uint32_t height)
    //{
    //    const VkDevice device = ImGuiInternal::device;

    //    VkResult err;
    //    VkSwapchainKHR old_swapchain = wd->m_Swapchain.GetVkSwapChain();
    //    err = vkDeviceWaitIdle(device);
    //    VK_CHECK(err);

    //    uint32_t windowSize[2] = { width, height };
    //    wd->m_Swapchain.Rebuild(windowSize);

    //    wd->m_Width = static_cast<uint32_t>(width);
    //    wd->m_Width = static_cast<uint32_t>(height);

    //    //CreateColorResources();

    //    //CreateDepthResources();

    //    // The framebuffers directly depend on the swap chain images,
    //    // and thus must be recreated as well.
    //    //CreateFramebuffers();
    //}

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
        VulkanBaseApp* app = (VulkanBaseApp*)ImGui::GetIO().BackendRendererUserData;
        assert(app);

        const VkDevice device = ImGuiInternal::device;
        const VkPhysicalDevice physicalDevice = ImGuiInternal::physicalDevice;

        //////////////////////////////////////////////////////////////////////////
        // Recreate Swapchain
        VkResult result;
        result = vkDeviceWaitIdle(device);
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
        CreateWindowResources(device, wd, nullptr);
        CreateWindowCommandBuffers(physicalDevice, device, wd);
    }

    static void Renderer_CreateWindow(ImGuiViewport* viewport)
    {
        VulkanBaseApp* app = (VulkanBaseApp*)ImGui::GetIO().BackendRendererUserData;
        assert(app);

        const VkInstance instance = ImGuiInternal::instance;
        const VkDevice device = ImGuiInternal::device;
        const VkPhysicalDevice physicalDevice = ImGuiInternal::physicalDevice;
        const VkQueue graphicsQueue = ImGuiInternal::graphicsQueue;
        const uint32_t queueFamily = app->GetQueueFamilyIndices().graphicsFamily.value();
        const uint32_t imageCount = app->GetSwapChainImageCount();

        VulkanViewportData* vd = IM_NEW(VulkanViewportData)();
        viewport->RendererUserData = vd;
        VulkanWindow* wd = &vd->Window;
        wd->m_QueueFamily = queueFamily;

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
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, wd->m_Surface, &res);
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
        const VkDevice device = ImGuiInternal::device;
        vkDeviceWaitIdle(device);

        VulkanViewportData* vd = (VulkanViewportData*)viewport->RendererUserData;
        if (vd)
        {
            vd->m_VertexBuffer.Destroy();
            vd->m_IndexBuffer.Destroy();

            VulkanWindow* wd = &vd->Window;

            vkFreeCommandBuffers(device, wd->m_CommandPool.GetVkCommandPool(),
                static_cast<uint32_t>(wd->m_CommandBuffers.size()), wd->m_CommandBuffers.data());

            wd->m_CommandPool.Destroy(device);

            wd->m_pFramesInFlight->Destroy();
            wd->m_pFramesInFlight.reset(0);

            wd->m_pSwapchain->Destroy();
            wd->m_pSwapchain.reset(0);

            vkDestroySurfaceKHR(instance, wd->m_Surface, nullptr);

            if (wd->m_RenderPass)
            {
                vkDestroyRenderPass(device, wd->m_RenderPass, nullptr);
            }

            for (auto frameBuffer : wd->m_Framebuffers)
            {
                vkDestroyFramebuffer(device, frameBuffer, nullptr);
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
        VulkanBaseApp* app = (VulkanBaseApp*)ImGui::GetIO().BackendRendererUserData;
        assert(app);

        VulkanViewportData* vd = (VulkanViewportData*)viewport->RendererUserData;
        VulkanWindow* wd = &vd->Window;

        const VkDevice device = ImGuiInternal::device;

        vkWaitForFences(device, 1, &syncObject.fence, VK_TRUE, UINT64_MAX);

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
        VulkanBaseApp* app = (VulkanBaseApp*)ImGui::GetIO().BackendRendererUserData;
        assert(app);

        const VkDevice device = app->GetVkDevice();

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
            result = vkResetCommandPool(device, wd->m_CommandPool.GetVkCommandPool(), 0);
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
        ImGuiRenderer::UpdateBuffers(viewport->DrawData, vd->m_VertexBuffer, vd->m_VertexCount,
            vd->m_IndexBuffer, vd->m_IndexCount);

        //////////////////////////////////////////////////////////////////////////
        //SetupRenderState
        // Bind pipeline:
        {
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ImGuiInternal::pipelineLayoutDocking,
                0, 1, &ImGuiInternal::descriptorSetDocking, 0, nullptr);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ImGuiInternal::pipelineDocking);
        }
        // Bind Vertex And Index Buffer:
        if (viewport->DrawData->TotalVtxCount > 0)
        {
            VkBuffer vertex_buffers[1] = { vd->m_VertexBuffer.m_Buffer };
            VkDeviceSize vertex_offset[1] = { 0 };
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertex_buffers, vertex_offset);
            vkCmdBindIndexBuffer(cmdBuffer, vd->m_IndexBuffer.m_Buffer, 0,
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

            vkCmdPushConstants(cmdBuffer, ImGuiInternal::pipelineLayoutDocking,
                VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
            vkCmdPushConstants(cmdBuffer, ImGuiInternal::pipelineLayoutDocking,
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

        result = vkQueueSubmit(ImGuiInternal::graphicsQueue, 1, &info, syncObject.fence);
        VK_CHECK(result);
    }

    static void Renderer_SwapBuffers(ImGuiViewport* viewport, void* renderArg)
    {
        VulkanBaseApp* app = (VulkanBaseApp*)ImGui::GetIO().BackendRendererUserData;
        assert(app);

        const VkDevice device = app->GetVkDevice();
        const VkQueue presentQueue = app->GetPresentQueue();

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

} // ImGuiInternal