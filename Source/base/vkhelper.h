#pragma once

#include "vkconfig.h"

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <stdexcept>

#define vkSTART_NAMESPACE namespace vk {
#define vkEND_NAMESPACE }

#define VK_FLAGS_NONE 0

#define DEFAULT_FENCE_TIMEOUT 100000000000

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(f)			\
{									\
	VkResult res = (f);				\
	if (res != VK_SUCCESS)			\
	{								\
		assert(res == VK_SUCCESS);	\
	}								\
}

vkSTART_NAMESPACE

namespace Helper
{
    inline bool HasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
};

vkEND_NAMESPACE