#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>

#include "Buffer.hpp"
#include "GBuffer.hpp"

// Índices dos sets, na mesma ordem da Tabela 1 da Entrega 3.
enum DescriptorSet : uint32_t {
    SET_CAMERA = 0,    // UBO da câmera (um set por frame em voo)
    SET_MATERIAL = 1,  // texturas e UBO do material
    SET_LIGHTS = 2,    // SSBO de luzes
    SET_GBUFFER = 3,   // input attachments do G-Buffer
    SET_COUNT = 4
};

class Descriptors {
public:
    // framesInFlight: quantos sets de câmera criar, um por frame em voo.
    void create(VkDevice device, uint32_t framesInFlight);
    void destroy();

    void writeCamera(uint32_t frame, const Buffer& ubo);
    void writeMaterial(VkImageView albedo, VkImageView normal, VkImageView specular,
        VkSampler sampler, const Buffer& ubo);
    void writeLights(const Buffer& ssbo);
    void writeGBuffer(const GBuffer& gbuffer);

    const std::array<VkDescriptorSetLayout, SET_COUNT>& layouts() const { return m_layouts; }
    VkDescriptorSet cameraSet(uint32_t frame) const { return m_cameraSets[frame]; }
    VkDescriptorSet set(DescriptorSet index) const { return m_sets[index]; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::array<VkDescriptorSetLayout, SET_COUNT> m_layouts{};
    std::array<VkDescriptorSet, SET_COUNT> m_sets{};  // o índice SET_CAMERA não é usado aqui
    std::vector<VkDescriptorSet> m_cameraSets;
};