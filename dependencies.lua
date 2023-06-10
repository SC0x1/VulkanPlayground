VULKAN_SDK = os.getenv("VULKAN_SDK")
LIB_DIR = "temp/Lib"

IncludeDir = {}
IncludeDir["GLFW"] = "external/GLFW/include"
IncludeDir["glew"] = "external/glew/include"
IncludeDir["glsw"] = "external/glsw"
IncludeDir["ImGui"] = "external/imgui"
IncludeDir["glm"] = "external/glm"
IncludeDir["fmt"] = "external/fmt/include"
IncludeDir["turf"] = "external/turf"

LibraryDir = {}
LibraryDir["GLFW"] = LIB_DIR
LibraryDir["ImGui"] = LIB_DIR

Library = {}
Library["GLFW3"] = "glfw3.lib"
Library["GLFW"] = "glfw.lib"
Library["glew32"] = "glew32.lib"
Library["glew32s"] = "glew32s.lib"

Binaries = {}

------------
-- Config --
outputtemp = "temp"
outputlibs = outputtemp .. "/Lib"
outputint = outputtemp .. "/int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
outputbuild = "build/"

projectfiles = "Compiler"
