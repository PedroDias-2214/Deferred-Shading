#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

// Buffer com memória visível pela CPU. Serve para UBO e SSBO atualizados pelo programa.
class Buffer {
public:
    void create(VkDevice device, VkPhysicalDevice physical,
        VkDeviceSize size, VkBufferUsageFlags usage);
    void destroy();

    // Copia dados para a memória do buffer, a partir do offset indicado.
    void write(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    VkBuffer handle() const { return m_buffer; }
    VkDeviceSize size() const { return m_size; }

private:
    uint32_t findMemoryType(VkPhysicalDevice physical, uint32_t typeBits,
        VkMemoryPropertyFlags properties) const;

    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    void* m_mapped = nullptr;
};