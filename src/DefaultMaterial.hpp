#pragma once

#include <vulkan/vulkan.h>
#include "Device.hpp"
#include "Buffer.hpp"

// Material padrão: texturas 1x1 e parâmetros fixos. Serve até existirem texturas reais.
// O geometry.frag amostra sempre as três texturas, então elas precisam existir.
class DefaultMaterial {
public:
    void create(const Device& device);
    void destroy();

    VkImageView albedoView() const { return m_albedo.view; }
    VkImageView normalView() const { return m_normal.view; }
    VkImageView specularView() const { return m_specular.view; }
    VkSampler sampler() const { return m_sampler; }
    const Buffer& ubo() const { return m_ubo; }

private:
    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    void createTexture(const Device& device, Texture& tex, VkFormat format,
        const void* pixels, VkDeviceSize size);
    uint32_t findMemoryType(VkPhysicalDevice physical, uint32_t typeBits,
        VkMemoryPropertyFlags properties) const;

    VkDevice m_device = VK_NULL_HANDLE;
    Texture m_albedo;
    Texture m_normal;
    Texture m_specular;
    VkSampler m_sampler = VK_NULL_HANDLE;
    Buffer m_ubo;
};