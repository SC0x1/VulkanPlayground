workspace "VulkanPlayground"
    location "Compiler"

    architecture "x64"

    configurations { "Debug", "Release", }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
VULKAN_SDK = os.getenv("VULKAN_SDK")

project "VulkanPlayground"
    language "C++"
    kind "ConsoleApp"
    targetdir("build/")

    objdir("temp/" .. outputdir .. "/%{prj.name}")

    files
    {
        "source/**.h", "source/**.cpp",
        "shaders/**.copm",
        "shaders/**.vert",
        "shaders/**.frag",
    }

    removefiles
    {
        "externals/**",
    }

    includedirs
    {
        "%VULKAN_SDK%/include/",
        "externals/stb/",
        "externals/glm/",
        "externals/glfw/include/",
        "externals/tinyobjloader/",
        "externals/",
        "source/"
    }

    libdirs { "%VULKAN_SDK%/Lib", "binaries/Lib" }

    links { "glfw3", "vulkan-1"}

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
