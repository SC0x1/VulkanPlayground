#include "VKPlayground_PCH.h"

#include "vulkan/utils.h"
#include "vulkan/framesinflight.h"
#include "vulkan/swapchain.h"
#include "vulkan/surface.h"
#include "vulkan/context.h"

// Embedded font
#include "common/imgui/Roboto-Regular.embed"

#include "common/imgui/shadowimpl/custom_impl.h"
#include "common/imgui/ImGuiShadersSpirV.inl"

ImGuiShadowVulkanCustomImpl::ImGuiShadowVulkanCustomImpl()
    : ImGuiShadowVulkan(false, true)
{

}

ImGuiShadowVulkanCustomImpl::~ImGuiShadowVulkanCustomImpl()
{

}

void ImGuiShadowVulkanCustomImpl::Initialize(const ImGuiVulkanInitInfo& initInfo)
{
    ImGuiShadowVulkan::Initialize(initInfo);

    ImGuiUtils::CreateFontsTextureCustom(m_InitInfo,
        m_CommandPool.GetVkCommandPool(),
        m_BackendDataMain.fontData);

    ImGuiUtils::CreatePipeline(m_InitInfo, m_InitInfo.renderPassMain, m_BackendDataMain);

    m_Buffers.vertexBuffer.m_Device = m_InitInfo.device;
    m_Buffers.indexBuffer.m_Device = m_InitInfo.device;
}

void ImGuiShadowVulkanCustomImpl::Shutdown()
{
    ImGuiShadowVulkan::Shutdown();

    // Destroy Font resources
    const ImGuiVulkanFontData& fontData = m_BackendDataMain.fontData;
    vkDestroyImage(m_InitInfo.device, fontData.fontImage, nullptr);
    vkDestroyImageView(m_InitInfo.device, fontData.fontView, nullptr);
    vkFreeMemory(m_InitInfo.device, fontData.fontMemory, nullptr);
    vkDestroySampler(m_InitInfo.device, fontData.fontSampler, nullptr);

    // Pipeline
    vkDestroyPipelineCache(m_InitInfo.device, m_BackendDataMain.pipelineCache, nullptr);

    vkDestroyPipeline(m_InitInfo.device, m_BackendDataMain.pipeline, nullptr);

    vkDestroyPipelineLayout(m_InitInfo.device, m_BackendDataMain.pipelineLayout, nullptr);
    //vkDestroyDescriptorPool(m_InitInfo.device, m_MainWindowData.descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_InitInfo.device, m_BackendDataMain.descriptorSetLayout, nullptr);

    m_Buffers.vertexBuffer.Destroy();
    m_Buffers.indexBuffer.Destroy();
}

void ImGuiShadowVulkanCustomImpl::StartFrame()
{
    ImGuiShadowVulkan::StartFrame();
}

void ImGuiShadowVulkanCustomImpl::EndFrame(uint32_t frameIndex, uint32_t imageIndex)
{
    ImGuiShadowVulkan::EndFrame(frameIndex, imageIndex);
}

void ImGuiShadowVulkanCustomImpl::DrawFrame(ImDrawData* imDrawData, VkCommandBuffer cmdBuffer)
{
    VkPipelineLayout& pipelineLayout = m_BackendDataMain.pipelineLayout;
    VkPipeline& pipelineMain = m_BackendDataMain.pipeline;
    VkDescriptorSet& descriptorSet = m_BackendDataMain.descriptorSet;

    ImGuiIO& io = ImGui::GetIO();

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineMain);

    VkViewport viewport{};
    viewport.width = (float)m_InitInfo.swapChainExtend.width;
    viewport.height = (float)m_InitInfo.swapChainExtend.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    m_PushConstBlock.scale = glm::vec2(2.0f / viewport.width, 2.0f / viewport.height);
    m_PushConstBlock.translate = glm::vec2(-1.0f);

    vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(PushConstantsBlock), &m_PushConstBlock);

    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    const VkBuffer vkIndexBuffer = m_Buffers.indexBuffer.m_Buffer;
    const VkBuffer vkVertexBuffer = m_Buffers.vertexBuffer.m_Buffer;

    if (imDrawData->CmdListsCount > 0)
    {
        VkDeviceSize offsets[1] = { 0 };
        
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vkVertexBuffer, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, vkIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

        for (uint32_t i = 0; i < (uint32_t)imDrawData->CmdListsCount; i++)
        {
            const ImDrawList* cmd_list = imDrawData->CmdLists[i];

            for (uint32_t j = 0; j < (uint32_t)cmd_list->CmdBuffer.Size; j++)
            {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
                VkRect2D scissorRect;
                scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
                scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
                scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissorRect);
                vkCmdDrawIndexed(cmdBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                
                indexOffset += pcmd->ElemCount;
            }

            vertexOffset += cmd_list->VtxBuffer.Size;
        }
    }
}

void ImGuiShadowVulkanCustomImpl::UpdateBuffers(ImDrawData* imDrawData)
{
    ImGuiUtils::UpdateBuffers(m_InitInfo, imDrawData, m_Buffers);
}