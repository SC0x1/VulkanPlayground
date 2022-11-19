#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

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

#include <vulkan/vulkan.h>

/*
    https://vulkan-tutorial.com/
*/

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

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

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
    {

        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
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

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

const std::vector<Vertex> verticesData =
{
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indicesData =
{
    0, 1, 2, 2, 3, 0
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool IsComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication
{
public:

    void Run();

private:
    void InitVulkan();

    void MainLoop();

    void Cleanup();

    void InitWindow();

    //////////////////////////////////////////////////////////////////////////
    /// Vulkan
    void CreateInstance();
    void CreateSurface();

    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapChain();
    void RecreateSwapChain();
    void CleanupSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateCommandBuffers();
    void CreateSyncObjects();

    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) const;

    void UpdateUniformBuffer(uint32_t currentImage);

    void DrawFrame();

    bool CheckValidationLayerSupport() const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

    std::vector<const char*> GetRequiredExtensions() const;

    // Swap Chain
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;

    int RateDeviceSuitability(VkPhysicalDevice device) const;

    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

private:
    const int MAX_FRAMES_IN_FLIGHT = 2;

    // is measured in screen coordinates
    // But Vulkan works with pixels
    // glfwGetFramebufferSize to query the resolution of the window in pixel
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    GLFWwindow* m_Window = nullptr;

    VkInstance m_Instance; 

    VkDebugUtilsMessengerEXT m_DebugMessenger;
    VkSurfaceKHR m_Surface;

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // A Physical device
    VkDevice m_Device;                                  // A Logical device
    VkQueue m_GraphicsQueue;                            // Device queues are implicitly cleaned up when the device is destroyed
    VkQueue m_PresentQueue;

    // Swap chain
    std::vector<VkImage> m_SwapChainImages;
    std::vector<VkImageView> m_SwapChainImageViews;
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

    VkSwapchainKHR m_SwapChain;
    VkFormat m_SwapChainImageFormat;
    VkExtent2D m_SwapChainExtent;

    // Pipeline
    VkRenderPass m_RenderPass;
    VkDescriptorSetLayout m_DescriptorSetLayout; // UBO
    VkPipelineLayout m_PipelineLayout;
    VkPipeline m_GraphicsPipeline;

    VkCommandPool m_CommandPool;
    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    VkBuffer m_VertexBuffer;
    VkDeviceMemory m_VertexBufferMemory;
    VkBuffer m_IndexBuffer;
    VkDeviceMemory m_IndexBufferMemory;

    std::vector<VkBuffer> m_UniformBuffers;
    std::vector<VkDeviceMemory> m_UniformBuffersMemory;
    std::vector<void*> m_UniformBuffersMapped;

    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    uint32_t m_CurrentFrameIdx = 0;

    const std::vector<const char*> m_ValidationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> m_DeviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif

    bool m_IsFamebufferResized = false;
};