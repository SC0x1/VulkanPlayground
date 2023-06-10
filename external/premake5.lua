
outputlibs_Extern = "../" .. outputlibs
outputint_Extern = "../" .. outputint

project "ImGui"
    kind "StaticLib"
    language "C++"
    staticruntime "off"

    targetdir (outputlibs_Extern)
    objdir (outputint_Extern)

    files
    {
        "imgui/imconfig.h",
        "imgui/imgui.h",
        "imgui/imgui.cpp",
        "imgui/imgui_draw.cpp",
        "imgui/imgui_internal.h",
        "imgui/imgui_tables.cpp",
        "imgui/imgui_widgets.cpp",
        "imgui/imstb_rectpack.h",
        "imgui/imstb_textedit.h",
        "imgui/imstb_truetype.h",
        "imgui/imgui_demo.cpp",
    }

    filter "system:windows"
        systemversion "latest"
        cppdialect "C++17"

    filter "system:linux"
        pic "On"
        systemversion "latest"
        cppdialect "C++17"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
        targetsuffix ("_d")

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Final"
        runtime "Release"
        optimize "on"
        symbols "off"

-- GLFW --
project "glfw3"
    location "glfw"
    kind "StaticLib"
    language "C"
    staticruntime "off"

    --TODO: review the output location
    targetdir (outputlibs_Extern)
    --objdir ("bin/temp/%{prj.name}")

    objdir (outputint_Extern)

    files
    {
    "glfw/include/GLFW/glfw3.h",
    "glfw/include/GLFW/glfw3native.h",
    "glfw/src/internal.h",
    "glfw/src/platform.h",
    "glfw/src/mappings.h",
    "glfw/src/context.c",
    "glfw/src/init.c",
    "glfw/src/input.c",
    "glfw/src/monitor.c",
    "glfw/src/platform.c",
    "glfw/src/vulkan.c",
    "glfw/src/window.c",
    "glfw/src/egl_context.c",
    "glfw/src/osmesa_context.c",
    "glfw/src/null_platform.h",
    "glfw/src/null_joystick.h",
    "glfw/src/null_init.c",

    "glfw/src/null_monitor.c",
    "glfw/src/null_window.c",
    "glfw/src/null_joystick.c",
    }

    filter "system:linux"
        pic "On"

        systemversion "latest"

        files
        {
            "glfw/src/x11_init.c",
            "glfw/src/x11_monitor.c",
            "glfw/src/x11_window.c",
            "glfw/src/xkb_unicode.c",
            "glfw/src/posix_time.c",
            "glfw/src/posix_thread.c",
            "glfw/src/glx_context.c",
            "glfw/src/egl_context.c",
            "glfw/src/osmesa_context.c",
            "glfw/src/linux_joystick.c",
        }

        defines
        {
            "_GLFW_X11"
        }

    filter "system:windows"
        systemversion "latest"

        files
        {
            "glfw/src/win32_init.c",
            "glfw/src/win32_module.c",
            "glfw/src/win32_joystick.c",
            "glfw/src/win32_monitor.c",
            "glfw/src/win32_time.h",
            "glfw/src/win32_time.c",
            "glfw/src/win32_thread.h",
            "glfw/src/win32_thread.c",
            "glfw/src/win32_window.c",
            "glfw/src/wgl_context.c",
            "glfw/src/egl_context.c",
            "glfw/src/osmesa_context.c",
        }

        defines
        {
            "_GLFW_WIN32",
            "_CRT_SECURE_NO_WARNINGS"
        }

        links
        {
            "Dwmapi.lib"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Final"
        runtime "Release"
        optimize "on"
        symbols "off"
