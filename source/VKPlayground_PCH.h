#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif // _WIN32

// to include vulkan_win32.h
//#ifndef VK_USE_PLATFORM_WIN32_KHR
//    #define VK_USE_PLATFORM_WIN32_KHR
//#endif

#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <windows.h>
#include "vulkan/vulkan_win32.h"
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //  OpenGL depth range of -1.0 to 1.0 by default. Vulkan range of 0.0 to 1.0
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cstdint>   // Necessary for uint32_t
#include <limits>    // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

#include <cstring>
#include <iostream>
#include <vector>
#include <array>
#include <optional>
#include <map>
#include <set>

#include <chrono>
#include <thread>
#include <random>
#include <fstream>
#include <unordered_map>
#include <map>

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
