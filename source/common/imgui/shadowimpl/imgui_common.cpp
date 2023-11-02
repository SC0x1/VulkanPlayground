#include "VKPlayground_PCH.h"

#include "vulkan/buffer.h"
#include "vulkan/device.h"
#include "vulkan/commandpool.h"

#include "common/imgui/ImGuiShadersSpirV.inl"

#include "common/imgui/shadowimpl/imgui_common.h"

namespace ImGuiUtils
{
    void UpdateBuffers(const ImGuiVulkanInitInfo& initInfo, ImDrawData* imDrawData,
        ImGuiVulkanWindowRenderBuffers& renderBuffers)
    {
        const VkDevice device = initInfo.device;
        const VkPhysicalDevice physicalDevice = initInfo.physicalDevice;

        Vk::Buffer& indexBuffer = renderBuffers.indexBuffer;
        Vk::Buffer& vertexBuffer = renderBuffers.vertexBuffer;
        uint32_t& indexCount = renderBuffers.indexCount;
        uint32_t& vertexCount = renderBuffers.vertexCount;

        ImGuiIO& io = ImGui::GetIO(); (void)io;

        // Note: Alignment is done inside buffer creation
        const VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
        const VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

        if ((vertexBufferSize == 0) || (indexBufferSize == 0))
        {
            return;
        }

        // Update buffers only if vertex or index count has been changed compared to current buffer size

        size_t vertex_size = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
        size_t index_size = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

        // Vertex buffer
        if ((vertexBuffer.m_Buffer == VK_NULL_HANDLE) || (vertexCount < (uint32_t)imDrawData->TotalVtxCount))
        {
            vertexBuffer.m_Device = device;

            vertexBuffer.UnMap();
            vertexBuffer.Destroy();
            // TODO: refactor: proper create a buffer wrapper
            Vk::Utils::CreateBuffer(device, physicalDevice, vertexBufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                &vertexBuffer.m_Buffer, &vertexBuffer.m_Memory);

            vertexCount = imDrawData->TotalVtxCount;
            vertexBuffer.Map();
        }

        // Index buffer
        if ((indexBuffer.m_Buffer == VK_NULL_HANDLE) ||
            (indexCount < (uint32_t)imDrawData->TotalIdxCount))
        {
            indexBuffer.m_Device = device;

            indexBuffer.UnMap();
            indexBuffer.Destroy();

            Vk::Utils::CreateBuffer(device, physicalDevice, indexBufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                &indexBuffer.m_Buffer, &indexBuffer.m_Memory);

            indexCount = imDrawData->TotalIdxCount;
            indexBuffer.Map();
        }

        // Upload data
        ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer.m_Mapped;
        ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer.m_Mapped;

        for (int n = 0; n < imDrawData->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = imDrawData->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }

        // Flush to make writes visible to GPU
        vertexBuffer.Flush();
        indexBuffer.Flush();
    }

    void CreateRenderPass(VkDevice device, VkFormat format, VkSampleCountFlagBits MSAASamples,
        VkAttachmentLoadOp loadOp, const VkAllocationCallbacks* allocator, VkRenderPass& renderPass)
    {
        VkAttachmentDescription attachment = {};
        attachment.format = format;
        attachment.samples = MSAASamples;
        // Need UI to be drawn on top of main
        attachment.loadOp = loadOp;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // Because this render pass is the last one, we want finalLayout to be set to
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        CreateRenderPass(device, attachment, allocator, renderPass);
    }

    void CreateRenderPass(VkDevice device, const VkAttachmentDescription& attachment,
         const VkAllocationCallbacks* allocator, VkRenderPass& renderPass)
    {
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;

        VkSubpassDependency subpassDependency = {};
        // VK_SUBPASS_EXTERNAL to create a dependency outside the current render pass
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT - since we want the pixels to be already
        // written to the framebuffer. We can also set our dstStageMask to this same value because
        // our GUI will also be drawn to the same target.
        // We're basically waiting for pixels to be written before we can write pixels ourselves.
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        // Wait on writes
        subpassDependency.srcAccessMask = 0;// VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &subpassDependency;

        VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, allocator, &renderPass));
    }

    void CreatePipeline(const ImGuiVulkanInitInfo& initInfo, VkRenderPass renderPass,
        ImGuiVulkanBackendData& bData)
    {
        VkPipelineCache& pipelineCache = bData.pipelineCache;
        VkPipelineLayout& pipelineLayout = bData.pipelineLayout;
        VkPipeline& pipeline = bData.pipeline;
        VkDescriptorSetLayout& descriptorSetLayout = bData.descriptorSetLayout;
        VkDescriptorSet& descriptorSet = bData.descriptorSet;
        VkShaderModule& shaderModuleVert = bData.shaderModuleVert;
        VkShaderModule& shaderModuleFrag = bData.shaderModuleFrag;

        //if (descriptorPool == VK_NULL_HANDLE)
        //{
        //    std::vector<VkDescriptorPoolSize> poolSizes =
        //    {
        //        {
        //            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        //            .descriptorCount = 1,
        //        }
        //    };

        //    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        //    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        //    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        //    descriptorPoolInfo.pPoolSizes = poolSizes.data();
        //    descriptorPoolInfo.maxSets = 2;

        //    VK_CHECK(vkCreateDescriptorPool(initInfo.device, &descriptorPoolInfo, nullptr, &initInfo.descriptorPool));
        //}

        // Descriptor set layout
        if (descriptorSetLayout == VK_NULL_HANDLE)
        {
            std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
                Vk::Init::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            };

            VkDescriptorSetLayoutCreateInfo descriptorLayout = Vk::Init::DescriptorSetLayoutCreateInfo(&setLayoutBindings[0], 1);
            VK_CHECK(vkCreateDescriptorSetLayout(initInfo.device, &descriptorLayout, nullptr, &descriptorSetLayout));
        }

        // Descriptor set
        if (descriptorSet == VK_NULL_HANDLE)
        {
            VkDescriptorSetAllocateInfo allocInfo = Vk::Init::DescriptorSetAllocateInfo(
                initInfo.descriptorPool, &descriptorSetLayout, 1);

            VK_CHECK(vkAllocateDescriptorSets(initInfo.device, &allocInfo, &descriptorSet));

            VkDescriptorImageInfo fontDescriptor{};
            fontDescriptor.sampler = bData.fontData.fontSampler;
            fontDescriptor.imageView = bData.fontData.fontView;
            fontDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            std::vector<VkWriteDescriptorSet> writeDescriptorSets =
            {
                Vk::Init::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
            };
            vkUpdateDescriptorSets(initInfo.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
        }

        // Pipeline cache
        if (pipelineCache == VK_NULL_HANDLE)
        {
            VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
            pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

            VK_CHECK(vkCreatePipelineCache(initInfo.device,
                &pipelineCacheCreateInfo, nullptr, &pipelineCache));
        }

        // Pipeline layout
        if (pipelineLayout == VK_NULL_HANDLE)
        {
            // Push constants for UI rendering parameters
            VkPushConstantRange pushConstantRange = Vk::Init::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstantsBlock), 0);

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Vk::Init::PipelineLayoutCreateInfo(&descriptorSetLayout, 1);
            pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
            pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

            VK_CHECK(vkCreatePipelineLayout(initInfo.device, &pipelineLayoutCreateInfo,
                nullptr, &pipelineLayout));
        }

        // PRIMITIVE TOPOLOGY
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
            Vk::Init::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

        VkPipelineRasterizationStateCreateInfo rasterizationState =
            Vk::Init::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        // Blending
        VkPipelineColorBlendAttachmentState blendAttachmentState{};
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlendState =
            Vk::Init::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        VkPipelineDepthStencilStateCreateInfo depthStencilState =
            Vk::Init::PipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

        VkPipelineViewportStateCreateInfo viewportState =
            Vk::Init::PipelineViewportStateCreateInfo(1, 1, 0);

        VkPipelineMultisampleStateCreateInfo multisampleState =
            Vk::Init::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

        const std::vector<VkDynamicState> dynamicStateEnables =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState =
            Vk::Init::PipelineDynamicStateCreateInfo(&dynamicStateEnables[0], 2);

        VkPipelineShaderStageCreateInfo shaderStages[2] = {};

        if (shaderModuleVert == VK_NULL_HANDLE)
        {
            VkShaderModuleCreateInfo vert_info = {};
            vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vert_info.codeSize = sizeof(__glsl_shader_vert_spv__);
            vert_info.pCode = (uint32_t*)__glsl_shader_vert_spv__;

            VK_CHECK(vkCreateShaderModule(initInfo.device, &vert_info, nullptr,
                &shaderModuleVert));
        }

        if (shaderModuleFrag == VK_NULL_HANDLE)
        {
            VkShaderModuleCreateInfo frag_info = {};
            frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            frag_info.codeSize = sizeof(__glsl_shader_frag_spv__);
            frag_info.pCode = (uint32_t*)__glsl_shader_frag_spv__;

            VK_CHECK(vkCreateShaderModule(initInfo.device, &frag_info, nullptr,
                &shaderModuleFrag));
        }

        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = shaderModuleVert;
        shaderStages[0].pName = "main";
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = shaderModuleFrag;
        shaderStages[1].pName = "main";

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vk::Init::PipelineCreateInfo(pipelineLayout, renderPass);

        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.stageCount = 2;
        pipelineCreateInfo.pStages = shaderStages;

        // Vertex bindings an attributes based on ImGui vertex definition
        std::vector<VkVertexInputBindingDescription> vertexInputBindings =
        {
            Vk::Init::VertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
        };

        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes =
        {
            Vk::Init::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),  // Location 0: Position
            Vk::Init::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),   // Location 1: UV
            Vk::Init::VertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)), // Location 0: Color
        };

        VkPipelineVertexInputStateCreateInfo vertexInputState = Vk::Init::PipelineVertexInputStateCreateInfo();
        vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
        vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
        vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
        vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

        pipelineCreateInfo.pVertexInputState = &vertexInputState;

        VK_CHECK(vkCreateGraphicsPipelines(initInfo.device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

        vkDestroyShaderModule(initInfo.device, shaderModuleFrag, nullptr);
        vkDestroyShaderModule(initInfo.device, shaderModuleVert, nullptr);
    }

    void CreateFontsTextureCustom(const ImGuiVulkanInitInfo& initInfo, const VkCommandPool commandPool,
        ImGuiVulkanFontData& fontData)
    {
        VkDeviceMemory& fontMemory = fontData.fontMemory;
        VkImage& fontImage = fontData.fontImage;
        VkImageView& fontView = fontData.fontView;
        VkSampler& fontSampler = fontData.fontSampler;

        ImGuiIO& io = ImGui::GetIO();

        // Create font texture
        unsigned char* fontRawData;
        int32_t texWidth, texHeight;
        io.Fonts->GetTexDataAsRGBA32(&fontRawData, &texWidth, &texHeight);
        const VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

        // Create target image for copy
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent.width = texWidth;
        imageInfo.extent.height = texHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vkCreateImage(initInfo.device, &imageInfo, nullptr, &fontImage));

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(initInfo.device, fontImage, &memReqs);

        VkMemoryAllocateInfo memAllocInfo{};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = Vk::Utils::FindMemoryType(initInfo.physicalDevice,
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(initInfo.device, &memAllocInfo, nullptr, &fontMemory));
        VK_CHECK(vkBindImageMemory(initInfo.device, fontImage, fontMemory, 0));

        // Image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = fontImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(initInfo.device, &viewInfo, nullptr, &fontView));

        // Staging buffers for font data upload
        Vk::Buffer stagingBuffer;
        stagingBuffer.m_Device = initInfo.device;

        Vk::Utils::CreateBuffer(initInfo.device, initInfo.physicalDevice,
            uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer.m_Buffer, &stagingBuffer.m_Memory);

        stagingBuffer.Map();
        memcpy(stagingBuffer.m_Mapped, fontRawData, uploadSize);
        stagingBuffer.UnMap();

        // Copy buffer data to font image
        VkCommandBuffer command_buffer = Vk::Utils::BeginSingleTimeCommands(initInfo.device,
            commandPool);
        {
            // Prepare for transfer
            Vk::Utils::SetImageLayout(
                command_buffer,
                fontImage,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_HOST_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);

            // Copy
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = texWidth;
            bufferCopyRegion.imageExtent.height = texHeight;
            bufferCopyRegion.imageExtent.depth = 1;

            vkCmdCopyBufferToImage(
                command_buffer,
                stagingBuffer.m_Buffer,
                fontImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &bufferCopyRegion
            );

            // Prepare for shader read
            Vk::Utils::SetImageLayout(
                command_buffer,
                fontImage,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        Vk::Utils::EndSingleTimeCommands(initInfo.device, initInfo.graphicsQueue,
            commandPool, command_buffer);

        stagingBuffer.UnMap();
        stagingBuffer.Destroy();

        // Font texture Sampler
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VK_CHECK(vkCreateSampler(initInfo.device, &samplerInfo, nullptr, &fontSampler));
    }
}