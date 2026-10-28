#pragma once

#include <vulkan/vulkan.h>
#include "Shaders.hpp"
#include "Descriptors.hpp"
#include "Mesh.hpp"

// Push constants do geometry.vert: matriz de modelo e normal matrix (128 bytes).
struct ObjectPushConstants {
    float model[16];
    float normalMatrix[16];
};
static_assert(sizeof(ObjectPushConstants) == 128, "ObjectPushConstants deve ter 128 bytes");

// Pipeline do subpass de geometria: escreve no G-Buffer com teste de profundidade.
class GeometryPipeline {
public:
    void create(VkDevice device, VkRenderPass renderPass, uint32_t subpass,
        const Shaders& shaders, const Descriptors& descriptors);
    void destroy();

    VkPipeline handle() const { return m_pipeline; }
    VkPipelineLayout layout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};