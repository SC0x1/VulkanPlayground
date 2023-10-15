#include "VKPlayground_PCH.h"

#include "vulkan/utils.h"
#include "vulkan/framesinflight.h"
#include "vulkan/swapchain.h"
#include "vulkan/surface.h"
#include "vulkan/context.h"

#include "common/imgui/shadowimpl/imgui_shadow_docking_impl.h"

ImGuiShadowVulkanDockingImpl::ImGuiShadowVulkanDockingImpl(bool useCustomRenderer)
    : ImGuiShadowVulkan(true, useCustomRenderer)
{
}

ImGuiShadowVulkanDockingImpl::~ImGuiShadowVulkanDockingImpl()
{

}

void ImGuiShadowVulkanDockingImpl::Initialize(const ImGuiVulkanInitInfo& initInfo)
{
    ImGuiShadowVulkan::Initialize(initInfo);

}

void ImGuiShadowVulkanDockingImpl::Shutdown()
{
    ImGuiShadowVulkan::Shutdown();

}

void ImGuiShadowVulkanDockingImpl::StartFrame()
{
    ImGuiShadowVulkan::StartFrame();
}

void ImGuiShadowVulkanDockingImpl::EndFrame(uint32_t frameIndex, uint32_t imageIndex)
{
    ImGuiShadowVulkan::EndFrame(frameIndex, imageIndex);

#if defined VP_IMGUI_VIEWPORTS_ENABLED
    ImGui::SetCurrentContext(m_ImGuiContext);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
#endif // VP_IMGUI_VIEWPORTS_ENABLED
}
