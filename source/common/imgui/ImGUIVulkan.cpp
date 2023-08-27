#include "VKPlayground_PCH.h"

#include "example_app.h"

#include "vulkan/utils.h"
#include "vulkan/context.h"
#include "ImGUIVulkan.h"
#include "ImGuizmo.h"

// Emedded font
#include "common/imgui/Roboto-Regular.embed"

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

bool VulkanImGUI::Initialize(VulkanBaseApp* app)
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

#if defined IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
#endif // IMGUI_HAS_DOCK

#if defined IMGUI_HAS_VIEWPORT
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
#endif // IMGUI_HAS_VIEWPORT

    io.FontGlobalScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);

    ImGui::StyleColorsDark();

    CreateDescriptorPool();

    CreateRenderPass();
    CreateFramebuffers();

    const VkInstance instance = m_App->GetVkInstance();
    const VkDevice device = m_App->GetVkDevice();
    const VkPhysicalDevice physicalDevice = m_App->GetVkPhysicalDevice();
    const VkQueue graphicsQueue = m_App->GetGraphicsQueue();
    const uint32_t queueFamily = m_App->GetQueueFamilyIndices().graphicsFamily.value();
    const uint32_t imageCount = m_App->GetSwapChainImageCount();

    //this initializes ImGui for GLFW
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window, true);

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
    ImGui_ImplVulkan_Init(&init_info, m_RenderPass);

    // Load default font
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
    io.FontDefault = robotoFont;

    // Upload Fonts
    {
        VkCommandBuffer command_buffer = m_App->BeginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
        m_App->EndSingleTimeCommands(command_buffer);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    CreateCommandPool();
    CreateCommandBuffers();

    return true;
}

void VulkanImGUI::Shutdown()
{
    assert(m_App);

    // Cleanup DearImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    const VkDevice device = m_App->GetVkDevice();

    vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
    vkFreeCommandBuffers(device, m_CommandPool.GetVkCommandPool(), static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

    DestroyCommandPool();

    vkDestroyRenderPass(device, m_RenderPass, nullptr);

    for (auto framebuffer : m_Framebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
}

void VulkanImGUI::StartNewFrame()
{
    ImGui::SetCurrentContext(m_ImGuiContext);

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
    //ImGuizmo::BeginFrame(); // TODO: ImGuizmo

    // Render to generate draw buffers
    ImGui::Render();

#if defined IMGUI_HAS_VIEWPORT
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
#endif // IMGUI_HAS_VIEWPORT
}

void VulkanImGUI::OnRecreateSwapchain()
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

    CreateRenderPass();

    ImGui_ImplVulkan_SetMinImageCount(imageCount);

    CreateCommandPool();
    CreateCommandBuffers();
    CreateFramebuffers();
}

void VulkanImGUI::OnRender(uint32_t frameIndex, uint32_t imageIndex)
{
    ImDrawData* main_draw_data = ImGui::GetDrawData();

    RecordCommandBuffer(m_CommandBuffers[frameIndex], imageIndex, main_draw_data);
}

void VulkanImGUI::RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, ImDrawData* draw_data)
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
    renderPassBeginInfo.renderPass = m_RenderPass;
    renderPassBeginInfo.framebuffer = m_Framebuffers[imageIndex];
    renderPassBeginInfo.renderArea.extent = extend;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Grab and record the draw data for Dear ImGui
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmdBuffer);

    vkCmdEndRenderPass(cmdBuffer);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));
}

void VulkanImGUI::CreateRenderPass()
{ 
    assert(m_App);

    const VkDevice device = m_App->GetVkDevice();
    const VkFormat format = m_App->GetSwapchainImageFormat();

    vkDestroyRenderPass(device, m_RenderPass, nullptr);

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

    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass));
}

void VulkanImGUI::CreateDescriptorPool()
{
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    const VkDevice device = m_App->GetVkDevice();

    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &m_DescriptorPool));
}

void VulkanImGUI::CreateFramebuffers()
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
    info.renderPass = m_RenderPass;
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

void VulkanImGUI::CreateCommandBuffers()
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

void VulkanImGUI::CreateCommandPool()
{
    const uint32_t queueFamily = m_App->GetQueueFamilyIndices().graphicsFamily.value();
    const VkDevice device = m_App->GetVkDevice();

    m_CommandPool.Create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

void VulkanImGUI::DestroyCommandPool()
{
    const VkDevice device = m_App->GetVkDevice();

    m_CommandPool.Destroy(device);
}
