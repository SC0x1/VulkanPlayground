#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

class FramesInFlight
{
public:

    void Initialize(VkDevice device, uint32_t maxFramesInFlight);

    bool IsInitialized() const;

    void Destroy();

    SyncObject NextSyncObject();
    SyncObject LatestSyncObject() const;

    void ResetFence(uint32_t imageIndex);

    uint32_t GetCurrentFrameIndex() const;

    uint32_t GetMaxFrmesInFlight() const;

private:

    void CreateSyncObjects();

    // Shared data
    VkDevice m_Device{};

    uint32_t m_MaxFramesInFlight = 0;

    uint32_t m_NextFrameIdx = 0;
    uint32_t m_CurrentFrameIdx = 0;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
    std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
    std::vector<VkFence> m_InFlightFences{};

    SyncObject m_LatestSyncObject{};

    bool m_IsInitialized = false;
};

inline bool FramesInFlight::IsInitialized() const
{
    return m_IsInitialized;
}

inline uint32_t FramesInFlight::GetCurrentFrameIndex() const
{
    return m_CurrentFrameIdx;
}

inline uint32_t FramesInFlight::GetMaxFrmesInFlight() const
{
    return m_MaxFramesInFlight;
}

inline Vk::SyncObject FramesInFlight::LatestSyncObject() const
{
    return m_LatestSyncObject;
}

vkEND_NAMESPACE