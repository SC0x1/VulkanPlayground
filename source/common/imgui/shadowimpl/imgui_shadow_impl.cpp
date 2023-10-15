#include "VKPlayground_PCH.h"

#include "vulkan/utils.h"
#include "vulkan/framesinflight.h"
#include "vulkan/swapchain.h"
#include "vulkan/surface.h"
#include "vulkan/context.h"

#include "common/imgui/shadowimpl/imgui_shadow_impl.h"

ImGuiShadowVulkanImpl::ImGuiShadowVulkanImpl(bool useCustomRenderer)
    : ImGuiShadowVulkan(false, useCustomRenderer)
{

}

ImGuiShadowVulkanImpl::~ImGuiShadowVulkanImpl()
{

}

void ImGuiShadowVulkanImpl::Initialize(const ImGuiVulkanInitInfo& initInfo)
{
    ImGuiShadowVulkan::Initialize(initInfo);

    //CreatePipeline(m_PipelineMain, m_RenderPassMain);
}

void ImGuiShadowVulkanImpl::Shutdown()
{
    ImGuiShadowVulkan::Shutdown();
}

void ImGuiShadowVulkanImpl::StartFrame()
{
    ImGuiShadowVulkan::StartFrame();
}

void ImGuiShadowVulkanImpl::EndFrame(uint32_t frameIndex, uint32_t imageIndex)
{
    ImGuiShadowVulkan::EndFrame(frameIndex, imageIndex);
}

