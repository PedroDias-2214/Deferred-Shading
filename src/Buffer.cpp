#include "Buffer.hpp"

#include <cstring>
#include <stdexcept>

void Buffer::create(VkDevice device, VkPhysicalDevice physical,
    VkDeviceSize size, VkBufferUsageFlags usage) {
    m_device = device;
    m_size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar buffer");
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(m_device, m_buffer, &req);

    // Memória visível pela CPU e coerente: o programa escreve direto, sem sincronização extra.
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(physical, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &alloc, nullptr, &m_memory) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar memória do buffer");
    }
    vkBindBufferMemory(m_device, m_buffer, m_memory, 0);

    // Mapeia a memória uma vez e mantém o ponteiro até o destroy.
    if (vkMapMemory(m_device, m_memory, 0, size, 0, &m_mapped) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao mapear memória do buffer");
    }
}

void Buffer::write(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (offset + size > m_size) {
        throw std::runtime_error("Escrita além do tamanho do buffer");
    }
    std::memcpy(static_cast<char*>(m_mapped) + offset, data, static_cast<size_t>(size));
}

void Buffer::destroy() {
    // Ordem: desmapear, destruir o buffer, liberar a memória.
    if (m_mapped) {
        vkUnmapMemory(m_device, m_memory);
        m_mapped = nullptr;
    }
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

uint32_t Buffer::findMemoryType(VkPhysicalDevice physical, uint32_t typeBits,
    VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(physical, &mem);

    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        bool allowed = (typeBits & (1u << i)) != 0;
        bool hasProps = (mem.memoryTypes[i].propertyFlags & properties) == properties;
        if (allowed && hasProps) return i;
    }
    throw std::runtime_error("Nenhum tipo de memória adequado para o buffer");
}