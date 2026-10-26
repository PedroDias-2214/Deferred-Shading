#pragma once

#include <vulkan/vulkan.h>
#include "Device.hpp"

// Dona das imagens do G-Buffer: posição, normal, albedo/especular e profundidade.
// Elas são escritas no subpass de geometria e lidas no subpass de iluminação.
class GBuffer {
public:
    // Formatos da Tabela 2 da Entrega 3.
    static constexpr VkFormat POSITION_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat NORMAL_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat ALBEDO_SPEC_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

    void create(const Device& device, VkExtent2D extent);
    void destroy();

    VkImageView positionView() const { return m_position.view; }
    VkImageView normalView() const { return m_normal.view; }
    VkImageView albedoSpecView() const { return m_albedoSpec.view; }
    VkImageView depthView() const { return m_depth.view; }

private:
    struct Attachment {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    void createAttachment(Attachment& att, VkFormat format, VkImageUsageFlags usage,
        VkImageAspectFlags aspect, VkExtent2D extent);
    void destroyAttachment(Attachment& att);
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    Attachment m_position;
    Attachment m_normal;
    Attachment m_albedoSpec;
    Attachment m_depth;
};