#include "common/imgui/shadowimpl/imgui_shadow.h"

class ImGuiShadowVulkanDockingImpl : public ImGuiShadowVulkan 
{
public:
    ImGuiShadowVulkanDockingImpl(bool useCustomRenderer);

    virtual ~ImGuiShadowVulkanDockingImpl();

    void Initialize(const ImGuiVulkanInitInfo& initInfo) override;
    void Shutdown() override;

    void StartFrame() override;
    void EndFrame(uint32_t frameIndex, uint32_t imageIndex) override;
};