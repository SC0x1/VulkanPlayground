#include "VKPlayground_PCH.h"

#include "vulkan/utils.h"
#include "example_app.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

QB_SINGLETON_GENERIC_DEFINE(VulkanExample);

const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

void ReadFile(const std::string& filename, std::vector<char>& buffer)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    buffer.resize(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
}

VulkanExample::VulkanExample()
{

}

VulkanExample::~VulkanExample()
{

}

void VulkanExample::OnInitialize()
{
    VulkanBaseApp::OnInitialize();

    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();

    LoadModel();

    CreateVertexBuffer();
    CreateIndexBuffer();

    CreateDescriptorSetLayout();

    CreateGraphicsPipeline();

    CreateUniformBuffers();

    CreateDescriptorPool();
    CreateDescriptorSets();

    CreateCommandBuffers();

    m_ImGuiLayer.Initialize(this);
}

void VulkanExample::OnCleanup()
{
    vkDestroySampler(m_Device, m_TextureSampler, nullptr);
    vkDestroyImageView(m_Device, m_TextureImageView, nullptr);

    vkDestroyImage(m_Device, m_TextureImage, nullptr);
    vkFreeMemory(m_Device, m_TextureImageMemory, nullptr);

    for (size_t i = 0; i < m_MaxFramesInFlight; i++)
    {
        vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
        vkFreeMemory(m_Device, m_UniformBuffersMemory[i], nullptr);
    }

    vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);

    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

    vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);

    vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
    vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);

    vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
    vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);

    m_ImGuiLayer.Shutdown();

    VulkanBaseApp::OnCleanup();
}

void VulkanExample::OnRender()
{
    //m_ImGuiLayer.StartFrame();
    DrawFrame();
}

void VulkanExample::OnRecreateSwapchain()
{
    VulkanBaseApp::OnRecreateSwapchain();
    m_ImGuiLayer.OnRecreateSwapchain();
}

void VulkanExample::CreateDescriptorSetLayout()
{
    // Sampler
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    /*
    It is possible to use texture sampling in the vertex shader,
    for example to dynamically deform a grid of vertices by a heightmap.
    */

    // UBO 
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());;
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout));
}

//BOOKMARK: CreateGraphicsPipeline
void VulkanExample::CreateGraphicsPipeline()
{
    std::vector<char> vertShaderCode;
    const std::string pathVert = std::string("shaders/vert.spv");
    const std::string pathFrag = std::string("shaders/frag.spv");

    ReadFile(pathVert.c_str(), vertShaderCode);
    std::vector<char> fragShaderCode;
    ReadFile(pathFrag.c_str(), fragShaderCode);

    VkShaderModule vertShaderModule;
    VK_CHECK(Vk::Utils::CreateShaderModule(m_Device, vertShaderCode, vertShaderModule));
    VkShaderModule fragShaderModule;
    VK_CHECK(Vk::Utils::CreateShaderModule(m_Device, fragShaderCode, fragShaderModule));

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    //////////////////////////////////////////////////////////////////////////
    // Input assembly
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    //////////////////////////////////////////////////////////////////////////
    // Viewports and scissors
    const VkExtent2D swapchainExtend = m_Swapchain.GetExtent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchainExtend.width;
    viewport.height = (float)swapchainExtend.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchainExtend;

    std::vector<VkDynamicState> dynamicStates =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // For static states
    //VkPipelineViewportStateCreateInfo viewportState{};
    //viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    //viewportState.viewportCount = 1;
    //viewportState.pViewports = &viewport;
    //viewportState.scissorCount = 1;
    //viewportState.pScissors = &scissor;

    //////////////////////////////////////////////////////////////////////////
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    /*
        If depthClampEnable is set to VK_TRUE, then fragments that are beyond the near and far planes
        are clamped to them as opposed to discarding them
    */
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    /*
        If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through the rasterizer stage.
        This basically disables any output to the framebuffer
    */
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    /*
        VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
        VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
        VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
            Using any mode other than fill requires enabling a GPU feature.
    */
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    //////////////////////////////////////////////////////////////////////////
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
    multisampling.rasterizationSamples = m_MsaaSamples;
    multisampling.minSampleShading = .2f; // min fraction for sample shading; closer to one is smoother
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional
    multisampling.sampleShadingEnable = VK_TRUE;

    //////////////////////////////////////////////////////////////////////////
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    {
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;  // field specifies if the depth of new fragments should be compared to the depth buffer to see if they should be discarded
        depthStencil.depthWriteEnable = VK_TRUE; // field specifies if the new depth of fragments that pass the depth test should actually be written to the depth buffer
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        // depthCompareOp field specifies the comparison that is performed to keep 
        // or discard fragments. We're sticking to the convention of lower depth = closer,
        // so the depth of new fragments should be less.
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        // The depthBoundsTestEnable, minDepthBounds and maxDepthBounds fields
        // are used for the optional depth bound test.
        // Basically, this allows you to only keep fragments that fall within the
        // specified depth range.
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional
    }

    //////////////////////////////////////////////////////////////////////////
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    /*
    This per-framebuffer struct allows you to configure the first way of color blending. The operations that will be performed are best demonstrated using the following pseudocode:

        if (blendEnable) {
            finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
            finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
        } else {
            finalColor = newColor;
        }

        finalColor = finalColor & colorWriteMask;

    The most common way to use color blending is to implement alpha blending, where we want the new color to be blended with the old color based on its opacity. The finalColor should then be computed as follows:

        finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
        finalColor.a = newAlpha.a;
        This can be accomplished with the following parameters:

        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    */

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    // If you want to use the second method of blending (bitwise combination),
    // then you should set logicOpEnable to VK_TRUE.
    // The bitwise operation can then be specified in the logicOp field.
    // Note that this will automatically disable the first method, as if you had set
    // blendEnable to VK_FALSE for every attached framebuffer! The colorWriteMask will
    // also be used in this mode to determine which channels in the framebuffer will
    // actually be affected. It is also possible to disable both modes, as we've done here,
    // in which case the fragment colors will be written to the framebuffer unmodified.

    //////////////////////////////////////////////////////////////////////////
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    // You can use uniform values in shaders, which are globals similar to dynamic
    // state variables that can be changed at drawing time to alter the behavior
    // of your shaders without having to recreate them.

    VK_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    // Then we reference all of the structures describing the fixed-function stage.
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = m_PipelineLayout;

    pipelineInfo.renderPass = m_RenderPass;
    pipelineInfo.subpass = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    // There are actually two more parameters: basePipelineHandle and basePipelineIndex.
    // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
    // The idea of pipeline derivatives is that it is less expensive to set up pipelines when
    // they have much functionality in common with an existing pipeline and switching between
    // pipelines from the same parent can also be done quicker.
    // You can either specify the handle of an existing pipeline with basePipelineHandle
    // or reference another pipeline that is about to be created by index with basePipelineIndex.

    VK_CHECK(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline));

    vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
}

void VulkanExample::CreateTextureImage()
{
    int texWidth, texHeight, texChannels;
    //stbi_uc* pixels = stbi_load("textures/texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    const std::string& pathTexture = TEXTURE_PATH;
    stbi_uc* pixels = stbi_load(pathTexture.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    // The max function selects the largest dimension.
    // The log2 function calculates how many times that dimension can be divided by 2.
    // The floor function handles cases where the largest dimension is not a power of 2.
    // 1 is added so that the original image has a mip level.

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    Vk::Utils::CreateBuffer(m_Device, m_PhysicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_Device, stagingBufferMemory);

    stbi_image_free(pixels);

    Vk::Utils::CreateImage2D(m_Device, m_PhysicalDevice, texWidth, texHeight, m_MipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_TextureImage, m_TextureImageMemory);

    // The next step is to copy the staging buffer to the texture image.
    // This involves two steps:
    //   Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    //   Execute the buffer to image copy operation

    TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_MipLevels);
    CopyBufferToImage(stagingBuffer, m_TextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

    GenerateMipmaps(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, m_MipLevels);
}

void VulkanExample::CreateTextureImageView()
{
    m_TextureImageView = CreateImageView(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_MipLevels);
}

void VulkanExample::CreateTextureSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // *VK_SAMPLER_ADDRESS_MODE_REPEAT*
    // Repeat the texture when going beyond the image dimensions.
    // *VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT*
    // Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
    // *VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE*
    // Take the color of the edge closest to the coordinate beyond the image dimensions.
    // *VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE*
    // Like clamp to edge, but instead uses the edge opposite to the closest edge.
    // *VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER*
    // Return a solid color when sampling beyond the dimensions of the image.

    // Anisotropy
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

    samplerInfo.anisotropyEnable = VK_TRUE;
    //samplerInfo.anisotropyEnable = VK_FALSE; // in case if we don't want to request it at startup
    //samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    // The borderColor field specifies which color is returned when sampling beyond
    // the image with clamp to border addressing mode.
    // It is possible to return black, white or transparent in either float or
    // int formats. You cannot specify an arbitrary color.

    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    // The unnormalizedCoordinates field specifies which coordinate system you want
    // to use to address texels in an image.
    // If this field is VK_TRUE, then you can simply use coordinates within the
    // [0, texWidth) and [0, texHeight) range. If it is VK_FALSE, then the texels
    // are addressed using the [0, 1) range on all axes. Real-world applications
    // almost always use normalized coordinates, because then it's possible to use
    // textures of varying resolutions with the exact same coordinates.

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    // If a comparison function is enabled, then texels will first be compared to a value,
    // and the result of that comparison is used in filtering operations.
    // This is mainly used for percentage-closer filtering on shadow maps.

    // Mipmapping
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f; // static_cast<float>(m_MipLevels / 2);
    samplerInfo.maxLod = static_cast<float>(m_MipLevels);
    samplerInfo.mipLodBias = 0.0f; // Optional

    VK_CHECK(vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_TextureSampler));

    // The sampler does not reference a VkImage anywhere.
    // The sampler is a distinct object that provides an interface to extract colors from a texture.
    // It can be applied to any image you want, whether it is 1D, 2D or 3D.
    // This is different from many older APIs, which combined texture images and
    // filtering into a single state.
}

void VulkanExample::CreateVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices.size(); //sizeof(verticesData[0]) * verticesData.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    Vk::Utils::CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    Vk::Utils::CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);

    CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
    // The driver may not immediately copy the data into the buffer memory,
    // for example because of caching. It is also possible that writes to the buffer
    // are not visible in the mapped memory yet.
    // There are two ways to deal with that problem:
    // * Use a memory heap that is host coherent, indicated with
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    // * Call vkFlushMappedMemoryRanges after writing to the mapped memory,
    // and call vkInvalidateMappedMemoryRanges before reading from the mapped memory.
    // The transfer of memory to GPU is guaranteed to be complete
    // as of the next call to vkQueueSubmit.
}

void VulkanExample::CreateIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();//sizeof(indicesData[0]) * indicesData.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    Vk::Utils::CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Indices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    Vk::Utils::CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);

    CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void VulkanExample::CreateUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_UniformBuffers.resize(m_MaxFramesInFlight);
    m_UniformBuffersMemory.resize(m_MaxFramesInFlight);
    m_UniformBuffersMapped.resize(m_MaxFramesInFlight);

    for (size_t i = 0; i < m_MaxFramesInFlight; i++)
    {
        Vk::Utils::CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_UniformBuffers[i], m_UniformBuffersMemory[i]);

        vkMapMemory(m_Device, m_UniformBuffersMemory[i], 0, bufferSize, 0, &m_UniformBuffersMapped[i]);
    }
}

void VulkanExample::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // UBO
    poolSizes[0].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // Sampler
    poolSizes[1].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(m_MaxFramesInFlight);

    // Inadequate descriptor pools are a good example of a problem that the
    // validation layers will not catch:
    // As of Vulkan 1.1, vkAllocateDescriptorSets may fail with the error code
    // VK_ERROR_POOL_OUT_OF_MEMORY if the pool is not sufficiently large, but the
    // driver may also try to solve the problem internally. This means that sometimes
    // (depending on hardware, pool size and allocation size) the driver will let us
    // get away with an allocation that exceeds the limits of our descriptor pool.
    // Other times, vkAllocateDescriptorSets will fail and return VK_ERROR_POOL_OUT_OF_MEMORY.
    // This can be particularly frustrating if the allocation succeeds on some machines,
    // but fails on others.

    VK_CHECK(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool));
}

void VulkanExample::CreateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(m_MaxFramesInFlight, m_DescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(m_MaxFramesInFlight);
    allocInfo.pSetLayouts = layouts.data();

    m_DescriptorSets.resize(m_MaxFramesInFlight);

    if (vkAllocateDescriptorSets(m_Device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < m_MaxFramesInFlight; i++)
    {
        // UBO
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        // Sampler
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_TextureImageView;
        imageInfo.sampler = m_TextureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        // We need to specify the type of descriptor again.
        // It's possible to update multiple descriptors at once in an array,
        // starting at index dstArrayElement. The descriptorCount field specifies
        // how many array elements you want to update.

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_DescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;
        descriptorWrites[1].pTexelBufferView = nullptr; // Optional

        // The pBufferInfo field is used for descriptors that refer to buffer data,
        // pImageInfo is used for descriptors that refer to image data,
        // and pTexelBufferView is used for descriptors that refer to buffer views.

        vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
    // The descriptors are now ready to be used by the shaders!
}

void VulkanExample::CreateCommandBuffers()
{
    m_CommandBuffers.resize(m_MaxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

    VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()));
    // The level parameter specifies if the allocated command buffers are primary
    //  or secondary command buffers.
    // *VK_COMMAND_BUFFER_LEVEL_PRIMARY*
    //  Can be submitted to a queue for execution, but cannot be called from other command buffers.
    // *VK_COMMAND_BUFFER_LEVEL_SECONDARY*
    //  Cannot be submitted directly, but can be called from primary command buffers.
    //  (it's helpful to reuse common operations from primary command buffers.)
}

// BOOKMARK:2 RecordCommandBuffer - VulkanExample
void VulkanExample::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    // The flags parameter specifies how we're going to use the command buffer.
    // The following values are available:
    // *VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT*
    //   The command buffer will be rerecorded right after executing it once.
    //  *VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT*
    //   This is a secondary command buffer that will be entirely within a single render pass.
    //  *VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT*
    //   The command buffer can be resubmitted while it is also already pending execution.
    // The pInheritanceInfo parameter is only relevant for secondary command buffers.
    // It specifies which state to inherit from the calling primary command buffers
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    const VkExtent2D swapchainExtend = m_Swapchain.GetExtent();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_SwapChainFramebuffers[imageIndex];

    // Define the size of the render area
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchainExtend;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    // The range of depths in the depth buffer is 0.0 to 1.0 in Vulkan,
    // where 1.0 lies at the far view plane and 0.0 at the near view plane.
    // The initial value at each point in the depth buffer should be
    // the furthest possible depth, which is 1.0.
    //
    // !!! Note that the order of clearValues should be identical to the order of your attachments.
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        // The first parameter for every command is always the command buffer to record the command to.
        // The second parameter specifies the details of the render pass we've just provided.
        // The final parameter controls how the drawing commands within the render pass will be provided.
        // It can have one of two values:
        // *VK_SUBPASS_CONTENTS_INLINE*
        // The render pass commands will be embedded in the primary command buffer
        // itself and no secondary command buffers will be executed.
        // *VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS*
        // The render pass commands will be executed from secondary command buffers.
        // All of the functions that record commands can be recognized by their vkCmd prefix.

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

        // We've now told Vulkan which operations to execute in the graphics pipeline and
        // which attachment to use in the fragment shader.

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchainExtend.width);
        viewport.height = static_cast<float>(swapchainExtend.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapchainExtend;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // We did specify viewport and scissor state for this pipeline to be dynamic.
        // So we need to set them in the command buffer before issuing our draw command.

        VkBuffer vertexBuffers[] = { m_VertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[imageIndex], 0, nullptr);

        //vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_Indices.dataindicesData.size()), 1, 0, 0, 0);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_Indices.size()), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);

    // We've finished recording the command buffer
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void VulkanExample::UpdateUniformBuffer(uint32_t currentImage)
{
    const VkExtent2D swapchainExtend = m_Swapchain.GetExtent();

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    const float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.proj = glm::perspective(glm::radians(45.0f), swapchainExtend.width / (float)swapchainExtend.height, 0.1f, 10.0f);

    ubo.proj[1][1] *= -1;
    // GLM was originally designed for OpenGL, where the Y coordinate of the clip
    // coordinates is inverted.
    // The easiest way to compensate for that is to flip the sign on the scaling
    // factor of the Y axis in the projection matrix.
    // If you don't do this, then the image will be rendered upside down.

    memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

// BOOKMARK: DrawFrame
void VulkanExample::DrawFrame()
{
    auto syncObject = m_FramesInFlight.NextSyncObject();
    std::optional<uint32_t> imageIndex = AcquireNextImage(syncObject);
    if (imageIndex.has_value())
    {
        m_FrameBufferImageIndex = imageIndex.value();
        const uint32_t currentFrameIdx = m_FramesInFlight.GetCurrentFrameIndex();

        // Reset the in-flight fences so we do not get blocked waiting on in-flight images
        //vkResetFences(m_Device, 1, &m_ImagesInFlight[currentFrameIdx]);
        m_FramesInFlight.ResetFence(currentFrameIdx);

        UpdateUniformBuffer(m_FrameBufferImageIndex);

        //////////////////////////////////////////////////////////////////////////
        // Recording the command buffer

        // With the imageIndex specifying the swap chain image to use in hand,
        // we can now record the command buffer.
        // First, we call vkResetCommandBuffer on the command buffer to make sure
        // it is able to be recorded.

        // Now call the function recordCommandBuffer to record the commands we want.
        RecordCommandBuffer(m_CommandBuffers[m_FrameBufferImageIndex], m_FrameBufferImageIndex);

        m_ImGuiLayer.OnRender(m_FrameBufferImageIndex);
        // After waiting, we need to manually reset the fence to the unsignaled state

        VulkanBaseApp::PresentFrame(syncObject, m_FrameBufferImageIndex, currentFrameIdx);
    }
}

void VulkanExample::LoadModel()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    const std::string& pathModel = MODEL_PATH;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, pathModel.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            vertex.pos =
            {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord =
            {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                /*
                The OBJ format assumes a coordinate system where a vertical coordinate of 0 means the bottom of the image,
                however we've uploaded our image into Vulkan in a top to bottom orientation where 0 means the top of the image.
                Solve this by flipping the vertical component of the texture coordinates
                */
            };

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(m_Vertices.size());
                m_Vertices.push_back(vertex);
            }

            m_Indices.push_back(uniqueVertices[vertex]);

            vertex.color = { 1.0f, 1.0f, 1.0f };
        }
    }
}