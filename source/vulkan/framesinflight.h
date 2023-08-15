#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

class FramesInFlight
{
public:

    void Initialize(VkDevice device, uint32_t maxFramesInFlight);

    void Destroy();

    SyncObject NextSyncObject();

   void ResetFence(uint32_t imageIndex);

    uint32_t GetCurrentFrameIndex() const;

    uint32_t GetMaxFrmesInFlight() const;

private:

    void CreateSyncObjects();

    // Shared data
    VkDevice m_Device;

    uint32_t m_MaxFramesInFlight = 0;

    uint32_t m_NextFrameIdx = 0;
    uint32_t m_CurrentFrameIdx = 0;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    bool m_IsInitialized = false;
};

inline uint32_t FramesInFlight::GetCurrentFrameIndex() const
{
    return m_CurrentFrameIdx;
}

inline uint32_t FramesInFlight::GetMaxFrmesInFlight() const
{
    return m_MaxFramesInFlight;
}

vkEND_NAMESPACE