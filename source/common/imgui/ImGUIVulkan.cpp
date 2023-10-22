#include "VKPlayground_PCH.h"
#include "common/imgui/ImGuiConfig.h"

#include "example_app.h"

#include "ImGUIVulkan.h"
#include "ImGuizmo.h"

#include "common/imgui/shadowimpl/custom_impl.h"
#include "common/imgui/shadowimpl/custom_docking_impl.h"

ImGuiRenderer::ImGuiRenderer()
{
}

ImGuiRenderer::~ImGuiRenderer()
{
    // To prevent an compilation error with forward declaration of
    // ImGuiShadowVulkan and unique_ptr we implementation of dtor
    m_ShadowBackendImGui.reset(0);
}

bool ImGuiRenderer::Initialize(VulkanBaseApp* app)
{
    void* window = app->GetWindowHandle();

    if (window == nullptr)
    {
        return false;
    }

    m_ShadowBackendImGui.reset(ImGuiShadowVulkan::Create(true, true));

    const VkInstance instance = app->GetVkInstance();
    const VkDevice device = app->GetVkDevice();
    const VkQueue graphicsQueue = app->GetVkGraphicsQueue();
    const VkPhysicalDevice physicalDevice = app->GetVkPhysicalDevice();

    const uint32_t queueFamilyIdx = app->GetQueueFamilyIndices().graphicsFamily.value();;
    const uint32_t swapChainImageCount = app->GetSwapChainImageCount();

    const VkFormat swapchainImageFormat = app->GetVkSwapchainImageFormat();
    const VkExtent2D swapChainExtend = app->GetSwapChainExtend();
    const std::vector<VkImageView>& swapChainImageViews = app->GetSwapChainImageViews();

    ImGuiVulkanInitInfo initInfo
    {
        .instance = instance,
        .device = device,
        .physicalDevice = physicalDevice,
        .graphicsQueue = graphicsQueue,
        .queueFamily = queueFamilyIdx,
        .swapChainImageCount = swapChainImageCount,
        .swapchainImageFormat = swapchainImageFormat,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .renderPassMain = nullptr, // we will create renderPass by Initialization
        .pipelineCache = nullptr,
        .descriptorPool = nullptr, // It will be created by initialization ImGuiBackend
        .swapChainExtend = swapChainExtend,
        .swapChainImageViews = swapChainImageViews,
        .windowHandle = window,
    };

    m_ShadowBackendImGui->Initialize(initInfo);

    return true;
}

void ImGuiRenderer::Shutdown()
{
    if (m_ShadowBackendImGui != nullptr)
    {
        m_ShadowBackendImGui->Shutdown();
    }
}

void ImGuiRenderer::StartFrame()
{
    if (m_ShadowBackendImGui == nullptr)
    {
        return;
    }

    m_ShadowBackendImGui->StartFrame();

    // SRS - Set initial position of default Debug window (note: Debug window sets its own initial size, use ImGuiSetCond_Always to override)
    ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_Always);

    ImGui::ShowDemoWindow();
    {
        bool flag = true;
        //IMGUI_DEMO_MARKER("Examples/Menu");
        ImGui::MenuItem("(demo menu)", NULL, false, false);
        if (ImGui::MenuItem("New")) {}
        if (ImGui::MenuItem("Open", "Ctrl+O")) {}
        if (ImGui::BeginMenu("Open Recent"))
        {
            ImGui::MenuItem("fish_hat.c");
            ImGui::MenuItem("fish_hat.inl");
            ImGui::MenuItem("fish_hat.h");
            if (ImGui::BeginMenu("More.."))
            {
                ImGui::MenuItem("Hello");
                ImGui::MenuItem("Sailor");
                if (ImGui::BeginMenu("Recurse.."))
                {
                    //ShowExampleMenuFile();
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {}
        if (ImGui::MenuItem("Save As..")) {}

        ImGui::Separator();
        //IMGUI_DEMO_MARKER("Examples/Menu/Options");
        if (ImGui::BeginMenu("Options"))
        {
            static bool enabled = true;
            ImGui::MenuItem("Enabled", "", &enabled);
            ImGui::BeginChild("child", ImVec2(0, 60), true);
            for (int i = 0; i < 10; i++)
                ImGui::Text("Scrolling Text %d", i);
            ImGui::EndChild();
            static float f = 0.5f;
            static int n = 0;
            ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
            ImGui::InputFloat("Input", &f, 0.1f);
            ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
            ImGui::EndMenu();
        }

        //IMGUI_DEMO_MARKER("Examples/Menu/Colors");
        if (ImGui::BeginMenu("Colors"))
        {
            float sz = ImGui::GetTextLineHeight();
            for (int i = 0; i < ImGuiCol_COUNT; i++)
            {
                const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
                ImGui::Dummy(ImVec2(sz, sz));
                ImGui::SameLine();
                ImGui::MenuItem(name);
            }
            ImGui::EndMenu();
        }

        // Here we demonstrate appending again to the "Options" menu (which we already created above)
        // Of course in this demo it is a little bit silly that this function calls BeginMenu("Options") twice.
        // In a real code-base using it would make senses to use this feature from very different code locations.
        if (ImGui::BeginMenu("Options")) // <-- Append!
        {
            //IMGUI_DEMO_MARKER("Examples/Menu/Append to an existing menu");
            static bool b = true;
            ImGui::Checkbox("SomeOption", &b);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Disabled", false)) // Disabled
        {
            IM_ASSERT(0);
        }
        if (ImGui::MenuItem("Checked", NULL, true)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) {}
    }
    //ImGuizmo::BeginFrame(); // TODO: ImGuizmo
}

void ImGuiRenderer::EndFrame(uint32_t frameIndex, uint32_t imageIndex)
{
    if (m_ShadowBackendImGui != nullptr)
    {
        m_ShadowBackendImGui->EndFrame(frameIndex, imageIndex);
    }
}

void ImGuiRenderer::OnRecreateSwapchain(const Vk::SwapChain& swapChain)
{
    if (m_ShadowBackendImGui != nullptr)
    {
        m_ShadowBackendImGui->OnRecreateSwapchain(swapChain);
    }
}

bool ImGuiRenderer::IsSkippingFrame() const
{
    return m_IsSkippingFrame;
}