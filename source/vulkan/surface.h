#pragma once

#include <vector>

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

class Surface
{
public:

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    void InitSurface(VkInstance instance, void* platformHandle, void* platformWindow);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    void InitSurface(VkInstance instance, ANativeWindow* window);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    void InitSurface(VkInstance instance, IDirectFB* dfb, IDirectFBSurface* window);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    void InitSurface(VkInstance instance, wl_display* display, wl_surface* window);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    void InitSurface(VkInstance instance, xcb_connection_t* connection, xcb_window_t window);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
    void InitSurface(VkInstance instance, void* view);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
    void InitSurface(VkInstance instance, uint32_t width, uint32_t height);
#else
    void Surface::InitSurface(VkInstance instance, GLFWwindow* glfwWindow)
#endif

    void Destroy(VkInstance instance);

    VkSurfaceKHR GetVkSurface() const;

private:
    VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };
};

inline VkSurfaceKHR Surface::GetVkSurface() const
{
    return m_Surface;
}

vkEND_NAMESPACE