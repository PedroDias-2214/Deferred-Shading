#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>

#include "Buffer.hpp"
#include "GBuffer.hpp"

// Índices dos sets, na mesma ordem da Tabela 1 da Entrega 3.
enum DescriptorSet : uint32_t {
    SET_CAMERA = 0,    // UBO da câmera (um por frame em voo)
    SET_MATERIAL = 1,  // texturas e UBO do material (compartilhado)
    SET_LIGHTS = 2,    // SSBO de luzes (um por frame em voo)
    SET_GBUFFER = 3,   // input attachments do G-Buffer (compartilhado)
    SET_COUNT = 4
};

class Descriptors {
public:
    // framesInFlight: quantos sets de câmera e de luzes criar.
    void create(VkDevice device, uint32_t framesInFlight);
    void destroy();

    void writeCamera(uint32_t frame, const Buffer& ubo);
    void writeLights(uint32_t frame, const Buffer& ssbo);
    void writeMaterial(VkImageView albedo, VkImageView normal, VkImageView specular,
        VkSampler sampler, const Buffer& ubo);
    void writeGBuffer(const GBuffer& gbuffer);

    const std::array<VkDescriptorSetLayout, SET_COUNT>& layouts() const { return m_layouts; }
    VkDescriptorSet cameraSet(uint32_t frame) const { return m_cameraSets[frame]; }
    VkDescriptorSet lightSet(uint32_t frame) const { return m_lightSets[frame]; }
    VkDescriptorSet materialSet() const { return m_materialSet; }
    VkDescriptorSet gbufferSet() const { return m_gbufferSet; }

private:
    void allocate(VkDescriptorSetLayout layout, uint32_t count, VkDescriptorSet* out);
    void writeBuffer(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, const Buffer& buf);

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::array<VkDescriptorSetLayout, SET_COUNT> m_layouts{};
    std::vector<VkDescriptorSet> m_cameraSets;
    std::vector<VkDescriptorSet> m_lightSets;
    VkDescriptorSet m_materialSet = VK_NULL_HANDLE;
    VkDescriptorSet m_gbufferSet = VK_NULL_HANDLE;
};