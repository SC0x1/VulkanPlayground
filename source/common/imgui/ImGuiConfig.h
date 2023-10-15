#pragma once

#define IMGUI_INCLUDE_IMCONFIG_H

#define VP_IMGUI_VIEWPORTS_ENABLED 0
#define VP_IMGUI_DOCKING_ENABLED 0

// define pool size for pools
enum ImGuiPoolSize : uint32_t
{
    ImGuiPoolSize_Default = 1024,
};