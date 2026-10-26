#pragma once

#include <vulkan/vulkan.h>
#include <array>

#include "Buffer.hpp"
#include "GBuffer.hpp"

// Índices dos sets, na mesma ordem da Tabela 1 da Entrega 3.
enum DescriptorSet : uint32_t {
    SET_CAMERA = 0,    // UBO da câmera
    SET_MATERIAL = 1,  // texturas e UBO do material (geometria)
    SET_LIGHTS = 2,    // SSBO de luzes
    SET_GBUFFER = 3,   // input attachments do G-Buffer
    SET_COUNT = 4
};

// Dona das layouts, do pool e dos descriptor sets.
class Descriptors {
public:
    void create(VkDevice device);
    void destroy();

    // Liga os recursos reais aos sets. Chamar depois de criar os buffers e o G-Buffer.
    void writeCamera(const Buffer& ubo);
    void writeLights(const Buffer& ssbo);
    void writeGBuffer(const GBuffer& gbuffer);

    const std::array<VkDescriptorSetLayout, SET_COUNT>& layouts() const { return m_layouts; }
    VkDescriptorSet set(DescriptorSet index) const { return m_sets[index]; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::array<VkDescriptorSetLayout, SET_COUNT> m_layouts{};
    std::array<VkDescriptorSet, SET_COUNT> m_sets{};
};