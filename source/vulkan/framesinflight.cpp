#include "VKPlayground_PCH.h"

#include <vulkan/framesinflight.h>

vkBEGIN_NAMESPACE

void FramesInFlight::Initialize(VkDevice device, uint32_t maxFramesInFlight)
{
    assert(maxFramesInFlight > 0);

    m_MaxFramesInFlight = maxFramesInFlight;
    m_Device = device;

    CreateSyncObjects();

    m_IsInitialized = true;
}

void FramesInFlight::Destroy()
{
    for (size_t i = 0; i < m_MaxFramesInFlight; ++i)
    {
        vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
    }
    m_IsInitialized = false;
}

void FramesInFlight::CreateSyncObjects()
{
    assert(m_MaxFramesInFlight > 0);

    m_ImageAvailableSemaphores.resize(m_MaxFramesInFlight);
    m_RenderFinishedSemaphores.resize(m_MaxFramesInFlight);
    m_InFlightFences.resize(m_MaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    /*
    Create the fence in the signaled state, so that the first call to vkWaitForFences() returns immediately
    since the fence is already signaled. To do this, we add the VK_FENCE_CREATE_SIGNALED_BIT
    */

    for (size_t i = 0; i < m_MaxFramesInFlight; ++i)
    {
        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]));

        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]));

        VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]));
    }
}

SyncObject FramesInFlight::NextSyncObject()
{
    m_CurrentFrameIdx = m_NextFrameIdx;

    const VkSemaphore imageAvailableSemaphore = m_ImageAvailableSemaphores[m_CurrentFrameIdx];
    const VkSemaphore renderFinishedSemaphore = m_RenderFinishedSemaphores[m_CurrentFrameIdx];
    const VkFence fence = m_InFlightFences[m_CurrentFrameIdx];

    m_NextFrameIdx = (m_CurrentFrameIdx + 1) % m_MaxFramesInFlight;

    return SyncObject {
            imageAvailableSemaphore,
            renderFinishedSemaphore,
            fence,
    };
}

void FramesInFlight::ResetFence(uint32_t imageIndex)
{
    vkResetFences(m_Device, 1, &m_InFlightFences[imageIndex]);
}

vkEND_NAMESPACE