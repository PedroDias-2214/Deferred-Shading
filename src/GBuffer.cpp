#include "GBuffer.hpp"

#include <cstdio>
#include <stdexcept>

void GBuffer::create(const Device& device, VkExtent2D extent) {
    m_device = device.logical();
    m_physical = device.physical();

    // Attachments de cor são escritos no subpass 0 e lidos como input no subpass 1.
    const VkImageUsageFlags colorUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    const VkImageUsageFlags depthUsage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    createAttachment(m_position, POSITION_FORMAT, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, extent);
    createAttachment(m_normal, NORMAL_FORMAT, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, extent);
    createAttachment(m_albedoSpec, ALBEDO_SPEC_FORMAT, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, extent);
    createAttachment(m_depth, DEPTH_FORMAT, depthUsage, VK_IMAGE_ASPECT_DEPTH_BIT, extent);

    std::printf("G-Buffer criado: %ux%u\n", extent.width, extent.height);
}

void GBuffer::createAttachment(Attachment& att, VkFormat format, VkImageUsageFlags usage,
    VkImageAspectFlags aspect, VkExtent2D extent) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &att.image) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar imagem do G-Buffer");
    }

    // A imagem precisa de memória da própria GPU (device local).
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(m_device, att.image, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &alloc, nullptr, &att.memory) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar memória do G-Buffer");
    }
    vkBindImageMemory(m_device, att.image, att.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = att.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &att.view) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar image view do G-Buffer");
    }
}

uint32_t GBuffer::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(m_physical, &mem);

    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        bool allowedByImage = (typeBits & (1u << i)) != 0;
        bool hasProperties = (mem.memoryTypes[i].propertyFlags & properties) == properties;
        if (allowedByImage && hasProperties) {
            return i;
        }
    }
    throw std::runtime_error("Nenhum tipo de memória adequado encontrado");
}

void GBuffer::destroy() {
    destroyAttachment(m_depth);
    destroyAttachment(m_albedoSpec);
    destroyAttachment(m_normal);
    destroyAttachment(m_position);
}

void GBuffer::destroyAttachment(Attachment& att) {
    // Ordem: view, depois imagem, depois memória.
    if (att.view != VK_NULL_HANDLE) vkDestroyImageView(m_device, att.view, nullptr);
    if (att.image != VK_NULL_HANDLE) vkDestroyImage(m_device, att.image, nullptr);
    if (att.memory != VK_NULL_HANDLE) vkFreeMemory(m_device, att.memory, nullptr);
    att = Attachment{};
}