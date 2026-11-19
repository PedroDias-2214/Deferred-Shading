#pragma once

#include <vulkan/vulkan.h>

#include "Device.hpp"
#include "Buffer.hpp"
#include "GltfLoader.hpp"

// Material do modelo: textura de cor, textura de normal e parâmetros, nos formatos
// que o geometry.frag espera (set 1). Quando o modelo não traz uma textura, usa um
// pixel neutro no lugar.
class Material {
public:
    void create(const Device& device, const ModelData& model);
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
        uint32_t width, uint32_t height, const uint8_t* pixels, VkDeviceSize size);

    VkDevice m_device = VK_NULL_HANDLE;
    Texture m_albedo;
    Texture m_normal;
    Texture m_specular;
    VkSampler m_sampler = VK_NULL_HANDLE;
    Buffer m_ubo;
};