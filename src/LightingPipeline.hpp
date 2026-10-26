#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

#include "Shaders.hpp"
#include "Descriptors.hpp"

// Parâmetros do lighting.frag, enviados por push constant (32 bytes, dentro do limite de 128).
struct LightingPushConstants {
    float exposure = 1.0f;
    uint32_t debugView = 0;      // 0 = cor final; 1..5 = canais do G-Buffer
    uint32_t maxLights = 1024;
    uint32_t pad = 0;
    float backgroundColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
};
static_assert(sizeof(LightingPushConstants) == 32, "LightingPushConstants deve ter 32 bytes");

// Dono do pipeline gráfico usado no subpass de iluminação.
class LightingPipeline {
public:
    void create(VkDevice device, VkRenderPass renderPass, uint32_t subpass,
        const Shaders& shaders, const Descriptors& descriptors, bool applyGamma);
    void destroy();

    VkPipeline handle() const { return m_pipeline; }
    VkPipelineLayout layout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};