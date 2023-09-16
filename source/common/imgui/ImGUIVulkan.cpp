#include "VKPlayground_PCH.h"
#include "common/imgui/ImGuiConfig.h"

#include "example_app.h"

#include "vulkan/utils.h"
#include "vulkan/context.h"
#include "ImGUIVulkan.h"
#include "ImGuizmo.h"

#include "ImGuiShadersSpirV.inl"

// Emedded font
#include "common/imgui/Roboto-Regular.embed"

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

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

    //SRS - Set ImGui font and style scale factors to handle retina and other HiDPI displays
    ImGuiIO& io = ImGui::GetIO(); (void)io;
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

    CreateDescriptorPool();

    CreateRenderPass(m_RenderPassMain, true);
    CreateRenderPass(m_RenderPassDocking, false);

    CreateFramebuffers();

    CreateSyncObjects();

    const VkInstance instance = m_App->GetVkInstance();
    const VkDevice device = m_App->GetVkDevice();
    const VkPhysicalDevice physicalDevice = m_App->GetVkPhysicalDevice();
    const VkQueue graphicsQueue = m_App->GetGraphicsQueue();
    const uint32_t queueFamily = m_App->GetQueueFamilyIndices().graphicsFamily.value();
    const uint32_t imageCount = m_App->GetSwapChainImageCount();

    //m_FramesInFlight.Initialize(device, imageCount);

    //this initializes ImGui for GLFW
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window, true);

    if (m_UseDefaultRenderer)
    {
        //this initializes ImGui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance;
        init_info.PhysicalDevice = physicalDevice;
        init_info.Device = device;
        init_info.QueueFamily = queueFamily;
        init_info.Queue = graphicsQueue;
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

    CreateFontsTexture();

    CreateCommandPool();
    CreateCommandBuffers();

    // Need to create ImGui pipeline only in a case of Multi-Viewport feature enabled
    CreatePipeline();

    m_VertexBuffer.m_Device = device;
    m_IndexBuffer.m_Device = device;
    return true;
}

void ImGuiRenderer::Shutdown()
{
    assert(m_App);

    const VkDevice device = m_App->GetVkDevice();

    // Cleanup DearImGui
    if (m_UseDefaultRenderer)
    {
        ImGui_ImplVulkan_Shutdown();
    }

    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    // Docking Specific
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

    UpdateBuffers();

    RecordCommandBuffer(m_CommandBuffers[frameIndex], imageIndex, main_draw_data);

    SubmitCommandBuffer(m_CommandBuffers[frameIndex]);

#if defined VP_IMGUI_VIEWPORTS_ENABLED
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        if (m_UseDefaultRenderer)
        {
            ImGui::UpdatePlatformWindows();

            //TODO: RenderPlatformWindowsDefault -> custom implementation
            ImGui::RenderPlatformWindowsDefault();
        }
    }
#endif // VP_IMGUI_VIEWPORTS_ENABLED
}

void ImGuiRenderer::OnRecreateSwapchain()
{
    assert(m_App);

    const VkDevice device = m_App->GetVkDevice();
    const uint32_t imageCount = m_App->GetSwapChainImageCount();
    // Destroy Framebuffers
    for (auto framebuffer : m_Framebuffers)
    {
        vkDestroyFramebuffer(m_App->GetVkDevice(), framebuffer, nullptr);
    }

    vkFreeCommandBuffers(device, m_CommandPool.GetVkCommandPool(), static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());
    DestroyCommandPool();

    vkDestroyRenderPass(device, m_RenderPassMain, nullptr);

    CreateRenderPass(m_RenderPassMain, true);

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
    ImGuiPlatformIO& io = ImGui::GetPlatformIO();

    //io.Renderer_CreateWindow = ImGuiRendererInternal::OnCreateWindow;
    //io.Renderer_DestroyWindow = ImGuiRendererInternal::OnDestroyWindow;
    //io.Renderer_SetWindowSize = ImGuiRendererInternal::OnSetWindowSize;
}

void ImGuiRenderer::CreateFontsTexture()
{
    assert(m_App);

    const VkDevice device = m_App->GetVkDevice();
    const VkPhysicalDevice physicalDevice = m_App->GetVkPhysicalDevice();

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

    const VkDevice device = m_App->GetVkDevice();
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
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Grab and record the draw data for Dear ImGui
    if (m_UseDefaultRenderer)
    {
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmdBuffer);
    }

    DrawFrameMain(cmdBuffer);

    vkCmdEndRenderPass(cmdBuffer);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));
}

void ImGuiRenderer::SubmitCommandBuffer(VkCommandBuffer cmdBuffer)
{
    const VkDevice device = m_App->GetVkDevice();
    const VkQueue graphicsQueue = m_App->GetGraphicsQueue();

    vkResetFences(device, 1, &m_FenceQueueSync);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_FenceQueueSync));

    VK_CHECK(vkWaitForFences(device, 1, &m_FenceQueueSync, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
}

void ImGuiRenderer::CreateRenderPass(VkRenderPass& renderPass, bool mainSwapchainFormat) const
{
    assert(m_App);

    const VkDevice device = m_App->GetVkDevice();
    const VkFormat format = mainSwapchainFormat ? m_App->GetSwapchainImageFormat() : VK_FORMAT_B8G8R8A8_UNORM;

    VkAttachmentDescription attachment = {};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // Need UI to be drawn on top of main
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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

    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
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

    const VkDevice device = m_App->GetVkDevice();

    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &m_DescriptorPool));
}

void ImGuiRenderer::CreateFramebuffers()
{
    assert(m_App);
    const VkDevice device = m_App->GetVkDevice();
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
    const VkDevice device = m_App->GetVkDevice();

    m_CommandBuffers.resize(swapchainImages);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_CommandPool.GetVkCommandPool();
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = swapchainImages;

    VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, m_CommandBuffers.data()));
}

void ImGuiRenderer::CreateSyncObjects()
{
    const VkDevice device = m_App->GetVkDevice();

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FLAGS_NONE;
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_FenceQueueSync));
}

void ImGuiRenderer::CreateCommandPool()
{
    const uint32_t queueFamily = m_App->GetQueueFamilyIndices().graphicsFamily.value();
    const VkDevice device = m_App->GetVkDevice();

    m_CommandPool.Create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

void ImGuiRenderer::DestroyCommandPool()
{
    const VkDevice device = m_App->GetVkDevice();

    m_CommandPool.Destroy(device);
}

void ImGuiRenderer::CreatePipeline()
{
    const VkDevice device = m_App->GetVkDevice();

    // Descriptor pool
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

    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        Vk::Init::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout = Vk::Init::DescriptorSetLayoutCreateInfo(&setLayoutBindings[0], 1);
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_DescriptorSetLayoutDocking));

    // Descriptor set
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

    // Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCacheDocking));

    // Pipeline layout
    // Push constants for UI rendering parameters
    VkPushConstantRange pushConstantRange = Vk::Init::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstantsBlock), 0);
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Vk::Init::PipelineLayoutCreateInfo(&m_DescriptorSetLayoutDocking, 1);
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayoutDocking));

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

    VkShaderModuleCreateInfo vert_info = {};
    vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_info.codeSize = sizeof(__glsl_shader_vert_spv__);
    vert_info.pCode = (uint32_t*)__glsl_shader_vert_spv__;
    VK_CHECK(vkCreateShaderModule(device, &vert_info, nullptr, &m_ShaderModuleVertDocking));

    VkShaderModuleCreateInfo frag_info = {};
    frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_info.codeSize = sizeof(__glsl_shader_frag_spv__);
    frag_info.pCode = (uint32_t*)__glsl_shader_frag_spv__;
    VK_CHECK(vkCreateShaderModule(device, &frag_info, nullptr, &m_ShaderModuleFragDocking));

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = m_ShaderModuleVertDocking;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = m_ShaderModuleFragDocking;
    shaderStages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vk::Init::PipelineCreateInfo(m_PipelineLayoutDocking, m_RenderPassMain);

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

    VK_CHECK(vkCreateGraphicsPipelines(device, m_PipelineCacheDocking, 1, &pipelineCreateInfo, nullptr, &m_PipelineDocking));
}

void ImGuiRenderer::UpdateBuffers()
{
    const VkDevice device = m_App->GetVkDevice();
    const VkPhysicalDevice physicalDevice = m_App->GetVkPhysicalDevice();

    ImDrawData* imDrawData = ImGui::GetDrawData();

    // Note: Alignment is done inside buffer creation
    const VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    const VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    if ((vertexBufferSize == 0) || (indexBufferSize == 0))
    {
        return;
    }

    // Update buffers only if vertex or index count has been changed compared to current buffer size

    // Vertex buffer
    if ((m_VertexBuffer.m_Buffer == VK_NULL_HANDLE) || (m_VertexCount != imDrawData->TotalVtxCount))
    {
        m_VertexBuffer.UnMap();
        m_VertexBuffer.Destroy();

        Vk::Utils::CreateBuffer(device, physicalDevice, vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &m_VertexBuffer.m_Buffer, &m_VertexBuffer.m_Memory);

        m_VertexCount = imDrawData->TotalVtxCount;
        m_VertexBuffer.Map();
    }

    // Index buffer
    if ((m_IndexBuffer.m_Buffer == VK_NULL_HANDLE) || (m_IndexCount < imDrawData->TotalIdxCount))
    {
        m_IndexBuffer.UnMap();
        m_IndexBuffer.Destroy();

        Vk::Utils::CreateBuffer(device, physicalDevice, indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &m_IndexBuffer.m_Buffer, &m_IndexBuffer.m_Memory);

        m_IndexCount = imDrawData->TotalIdxCount;
        m_IndexBuffer.Map();
    }

    // Upload data
    ImDrawVert* vtxDst = (ImDrawVert*)m_VertexBuffer.m_Mapped;
    ImDrawIdx* idxDst = (ImDrawIdx*)m_IndexBuffer.m_Mapped;

    for (int n = 0; n < imDrawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush to make writes visible to GPU
    m_VertexBuffer.Flush();
    m_IndexBuffer.Flush();
}

void ImGuiRenderer::DrawFrameMain(VkCommandBuffer cmdBuffer)
{
    {
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

        // TODO: m_CommandBuffers[frameIndex] -> replace to dedicated for Docking
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayoutDocking, 0, 1, &m_DescriptorSetDocking, 0, nullptr);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineDocking);

        VkViewport viewport{};
        viewport.width = ImGui::GetIO().DisplaySize.x;
        viewport.height = ImGui::GetIO().DisplaySize.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        // UI scale and translate via push constants
        m_PushConstBlock.scale = glm::vec2(2.0f / ImGui::GetIO().DisplaySize.x, 2.0f / ImGui::GetIO().DisplaySize.y);
        m_PushConstBlock.translate = glm::vec2(-1.0f);
        vkCmdPushConstants(cmdBuffer, m_PipelineLayoutDocking, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantsBlock), &m_PushConstBlock);

        // Render commands
        ImDrawData* imDrawData = ImGui::GetDrawData();
        int32_t vertexOffset = 0;
        int32_t indexOffset = 0;

        if (imDrawData->CmdListsCount > 0)
        {
            VkDeviceSize offsets[1] = { 0 };
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_VertexBuffer.m_Buffer, offsets);
            vkCmdBindIndexBuffer(cmdBuffer, m_IndexBuffer.m_Buffer, 0, VK_INDEX_TYPE_UINT16);

            for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
            {
                const ImDrawList* cmd_list = imDrawData->CmdLists[i];
                for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
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

        //for (int i = 1; i < platform_io.Viewports.Size; i++)
        //{
        //    if ((platform_io.Viewports[i]->Flags & ImGuiViewportFlags_IsMinimized) == 0)
        //    {
        //        ImGuiViewport* vp = platform_io.Viewports[i];
        //        ImDrawData* drawData = vp->DrawData;

        //        const VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        //        const VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

        //        if ((vertexBufferSize == 0) || (indexBufferSize == 0))
        //        {
        //            return;
        //        }

        //        if (drawData->CmdListsCount > 0)
        //        {
        //            drawData->CmdLists;
        //            VkDeviceSize offsets[1] = { 0 };
        //            vkCmdBindVertexBuffers(m_CommandBuffers[frameIndex], 0, 1, &vertexBuffer.buffer, offsets);
        //            vkCmdBindIndexBuffer(m_CommandBuffers[frameIndex], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
        //        }


        //        // MyRenderFunction(platform_io.Viewports[i], my_args);
        //    }
        //}

        //for (int i = 1; i < platform_io.Viewports.Size; i++)
        //{
        //    if ((platform_io.Viewports[i]->Flags & ImGuiViewportFlags_IsMinimized) == 0)
        //    {
        //        // MySwapBufferFunction(platform_io.Viewports[i], my_args);
        //    }
        //}
    }
}
