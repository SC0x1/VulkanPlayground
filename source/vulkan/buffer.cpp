#include <assert.h>
#include <memory>

#include <vulkan/buffer.h>

vkBEGIN_NAMESPACE

/**
* Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
*
* @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the buffer mapping call
*/
VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
    return vkMapMemory(m_Device, m_Memory, offset, size, 0, &m_Mapped);
}

/**
* Unmap a mapped memory range
*
* @note Does not return a result as vkUnmapMemory can't fail
*/
void Buffer::UnMap()
{
    if (m_Mapped)
    {
        vkUnmapMemory(m_Device, m_Memory);
        m_Mapped = nullptr;
    }
}

/**
* Attach the allocated memory block to the buffer
*
* @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
*
* @return VkResult of the bindBufferMemory call
*/
VkResult Buffer::Bind(VkDeviceSize offset)
{
    return vkBindBufferMemory(m_Device, m_Buffer, m_Memory, offset);
}

/**
* Setup the default descriptor for this buffer
*
* @param size (Optional) Size of the memory range of the descriptor
* @param offset (Optional) Byte offset from beginning
*
*/
void Buffer::SetupDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
    m_Descriptor.offset = offset;
    m_Descriptor.buffer = m_Buffer;
    m_Descriptor.range = size;
}

/**
* Copies the specified data to the mapped buffer
*
* @param data Pointer to the data to copy
* @param size Size of the data to copy in machine units
*
*/
void Buffer::CopyTo(void* data, VkDeviceSize size)
{
    assert(m_Mapped);
    memcpy(m_Mapped, data, size);
}

/**
* Flush a memory range of the buffer to make it visible to the device
*
* @note Only required for non-coherent memory
*
* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the flush call
*/
VkResult Buffer::Flush(VkDeviceSize size, VkDeviceSize offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_Memory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(m_Device, 1, &mappedRange);
}

/**
* Invalidate a memory range of the buffer to make it visible to the host
*
* @note Only required for non-coherent memory
*
* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the invalidate call
*/
VkResult Buffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_Memory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkInvalidateMappedMemoryRanges(m_Device, 1, &mappedRange);
}

/**
* Release all Vulkan resources held by this buffer
*/
void Buffer::Destroy()
{
    assert(m_Mapped == nullptr);
    //assert(m_Device != nullptr);

    if (m_Buffer)
    {
        vkDestroyBuffer(m_Device, m_Buffer, nullptr);
        m_Buffer = VK_NULL_HANDLE;
    }

    if (m_Memory)
    {
        vkFreeMemory(m_Device, m_Memory, nullptr);
        m_Memory = VK_NULL_HANDLE;
    }

    //m_Device = VK_NULL_HANDLE;
    m_Size = 0;
    m_Alignment = 0;
}

vkEND_NAMESPACE
