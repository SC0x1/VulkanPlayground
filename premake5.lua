include "dependencies.lua"

workspace "VulkanPlayground"
    location "Compiler"
    architecture "x64"
    configurations { "Debug", "Release", }

group "Dependencies"
    include "external"
group ""

project "VulkanPlayground"
    language "C++"
    kind "ConsoleApp"

    targetdir (outputbuild)
	objdir (outputint)

    files
    {
        "source/**.h", "source/**.cpp",
        "shaders/**.copm",
        "shaders/**.vert",
        "shaders/**.frag",

        "external/imgui/backends/imgui_impl_glfw.h",
        "external/imgui/backends/imgui_impl_glfw.cpp",
        "external/imgui/backends/imgui_impl_vulkan.h",
        "external/imgui/backends/imgui_impl_vulkan.cpp",
    }

    removefiles
    {
    --    "external/**",
    }

    includedirs
    {
        "%VULKAN_SDK%/include/",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.ImGui}/backends/",
        "external/stb/",
        "external/glm/",
        "external/glfw/include/",
        "external/tinyobjloader/",
        "external/",
        "source/"
    }

    libdirs { "%VULKAN_SDK%/Lib", "%{LibraryDir.ImGui}", "%LibraryDir.GLFW}" }

    links
    {
        "glfw3",
        "ImGui" .. "%{cfg.targetsuffix}",
        "vulkan-1"
    }

    filter "system:windows"
        cppdialect "C++17"
        --staticruntime "On"
        systemversion "latest"

        defines
        {
            "PLATFORM_WINDOWS"
        }

    filter "configurations:Debug"
        defines "BUILD_DEBUG"
        symbols "On"
        targetsuffix ("_d")

    filter "configurations:Release"
        defines "BUILD_RELEASE"
        optimize "On"
        targetsuffix ("")

    filter "configurations:Final"
        runtime "Release"
        optimize "on"
        symbols "off"
        targetsuffix ("")
