#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Swapchain.hpp"

// Dono do render pass e dos framebuffers. O render pass descreve como os
// attachments são usados; o framebuffer liga esse modelo às imagens reais.
class RenderPass {
public:
    void create(VkDevice device, const Swapchain& swapchain);
    void destroy();

    VkRenderPass handle() const { return m_renderPass; }
    VkFramebuffer framebuffer(size_t index) const { return m_framebuffers[index]; }
    size_t framebufferCount() const { return m_framebuffers.size(); }
    VkExtent2D extent() const { return m_extent; }

private:
    void createRenderPass(VkFormat colorFormat);
    void createFramebuffers(const Swapchain& swapchain);

    VkDevice m_device = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    VkExtent2D m_extent{};
};