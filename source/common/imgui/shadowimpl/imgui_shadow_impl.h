#pragma once

#include "common/imgui/shadowimpl/imgui_shadow.h"

class ImGuiShadowVulkanImpl : public ImGuiShadowVulkan
{
public:
    ImGuiShadowVulkanImpl(bool useCustomRenderer);

    virtual ~ImGuiShadowVulkanImpl();

    void Initialize(const ImGuiVulkanInitInfo& initInfo) override;
    void Shutdown() override;

    void StartFrame() override;
    void EndFrame(uint32_t frameIndex, uint32_t imageIndex) override;

private:

};