#pragma once

#include "common/vulkanbaseapp.h"

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        /*
        VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
        VK_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance
        */
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
    {

        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        // Pos
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        /*
        shader: format
        float:  VK_FORMAT_R32_SFLOAT
        vec2:   VK_FORMAT_R32G32_SFLOAT
        vec3:   VK_FORMAT_R32G32B32_SFLOAT
        vec4:   VK_FORMAT_R32G32B32A32_SFLOAT
        ivec2:  VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
        uvec4:  VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
        double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
        */
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // TexCoord
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const
        {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class VulkanExample : public VulkanBaseApp
{
public:
    VulkanExample();
    ~VulkanExample();

private:
    virtual void InitializeVulkan() override;
    virtual void Cleanup() override;

    virtual void Render() override;

    //////////////////////////////////////////////////////////////////////////
    /// Vulkan
    
    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();

    void CreateVertexBuffer();
    void CreateIndexBuffer();

    void CreateGraphicsPipeline();

    void CreateDescriptorSetLayout();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateCommandBuffers();

    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) const;

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    void UpdateUniformBuffer(uint32_t currentImage) override;

    void DrawFrame();

    void LoadModel();

private:
    // Pipeline
    VkDescriptorSetLayout m_DescriptorSetLayout; // UBO
    VkPipelineLayout m_PipelineLayout;
    VkPipeline m_GraphicsPipeline;

    std::vector<VkDescriptorSet> m_DescriptorSets;

    // Mesh data
    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;

    VkBuffer m_VertexBuffer;
    VkDeviceMemory m_VertexBufferMemory;
    VkBuffer m_IndexBuffer;
    VkDeviceMemory m_IndexBufferMemory;

    std::vector<VkBuffer> m_UniformBuffers;
    std::vector<VkDeviceMemory> m_UniformBuffersMemory;
    std::vector<void*> m_UniformBuffersMapped;

    // Mipmaps
    uint32_t m_MipLevels;

    // Texture
    VkImage m_TextureImage;
    VkDeviceMemory m_TextureImageMemory;
    VkImageView m_TextureImageView;
    VkSampler m_TextureSampler;
};