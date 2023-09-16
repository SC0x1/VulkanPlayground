#pragma once

#define IMGUI_INCLUDE_IMCONFIG_H

#if defined IMGUI_HAS_VIEWPORT
    #define VP_IMGUI_VIEWPORTS_ENABLED
#endif // IMGUI_HAS_VIEWPORT

#if defined IMGUI_HAS_DOCK
    #define VP_IMGUI_DOCKING_ENABLED
#endif // IMGUI_HAS_DOCK

// define pool size for pools
enum ImGuiPoolSize : uint32_t
{
    ImGuiPoolSize_Default = 1024,
};