#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Swapchain.hpp"
#include "GBuffer.hpp"

// Render pass de dois subpasses, seguindo a Entrega 2:
//   subpass 0 (geometria):  escreve posição, normal, albedo e profundidade;
//   subpass 1 (iluminação): lê esses attachments como input e escreve na swapchain.
class RenderPass {
public:
    // Índices dos attachments dentro do render pass.
    static constexpr uint32_t ATT_SWAPCHAIN = 0;
    static constexpr uint32_t ATT_POSITION = 1;
    static constexpr uint32_t ATT_NORMAL = 2;
    static constexpr uint32_t ATT_ALBEDO_SPEC = 3;
    static constexpr uint32_t ATT_DEPTH = 4;
    static constexpr uint32_t ATTACHMENT_COUNT = 5;

    void create(VkDevice device, const Swapchain& swapchain, const GBuffer& gbuffer);
    void destroy();

    VkRenderPass handle() const { return m_renderPass; }
    VkFramebuffer framebuffer(size_t index) const { return m_framebuffers[index]; }
    size_t framebufferCount() const { return m_framebuffers.size(); }
    VkExtent2D extent() const { return m_extent; }

    // Valores de limpeza na ordem dos attachments. Usados no vkCmdBeginRenderPass.
    std::vector<VkClearValue> clearValues() const;

private:
    void createRenderPass(VkFormat swapchainFormat);
    void createFramebuffers(const Swapchain& swapchain, const GBuffer& gbuffer);

    VkDevice m_device = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    VkExtent2D m_extent{};
};