#include "VKPlayground_PCH.h"

#include "example_app.h"

#include "vulkan/vkutils.h"
#include "vulkan/vkcontext.h"
#include "ImGUIVulkan.h"
#include "ImGuizmo.h"

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

bool VulkanImGUI::Initialize()
{
    if (VulkanExample::HasInstance() == false)
    {
        return false;
    }
    VulkanExample& app = *VulkanExample::GetInstance();
    void* window = app.GetWindowHandle();
    if (window == nullptr)
    {
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    // Fonts
    {

    }

    ImGui::StyleColorsDark();

    // Create Descriptor Pool
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 100 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDevice device = app.GetVkDevice();
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool));

    // Setup Platform/Renderer bindings
    // BOOKMARK: ImGui
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = app.GetVkInstance();
    init_info.PhysicalDevice = app.GetVkPhysicalDevice();
    init_info.Device = device;
    init_info.QueueFamily = app.GetQueueFamilyIndices().graphicsFamily.value();
    init_info.Queue = app.GetGraphicsQueue();
    init_info.PipelineCache = nullptr;
    init_info.DescriptorPool = descriptorPool;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = 2;
    init_info.ImageCount = app.GetSwapChainImageCount();
    init_info.CheckVkResultFn = VkUtils::CheckVkResult;
    ImGui_ImplVulkan_Init(&init_info, app.GetRenderPass());



    return true;
}

void VulkanImGUI::Shutdown()
{

}

void VulkanImGUI::BeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void VulkanImGUI::EndFrame()
{

}
