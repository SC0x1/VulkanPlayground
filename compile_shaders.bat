@echo off
cls

CD %~dp0%\Shaders

%VULKAN_SDK%/Bin/glslc.exe shader_main.vert -o main_vert.spv
%VULKAN_SDK%/Bin/glslc.exe shader_main.frag -o main_frag.spv
%VULKAN_SDK%/Bin/glslc.exe shader_compute.comp -o compute_comp.spv
%VULKAN_SDK%/Bin/glslc.exe shader_compute.vert -o compute_vert.spv
%VULKAN_SDK%/Bin/glslc.exe shader_compute.frag -o compute_frag.spv

pause
