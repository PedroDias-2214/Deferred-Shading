#include "RenderPass.hpp"

#include <cstdio>
#include <stdexcept>

void RenderPass::create(VkDevice device, const Swapchain& swapchain) {
    m_device = device;
    m_extent = swapchain.extent();

    createRenderPass(swapchain.format());
    createFramebuffers(swapchain);

    std::printf("Render pass e %zu framebuffers criados\n", m_framebuffers.size());
}

void RenderPass::createRenderPass(VkFormat colorFormat) {
    // Attachment de cor: a própria imagem da swapchain.
    VkAttachmentDescription color{};
    color.format = colorFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;     // limpa o conteúdo ao começar
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // guarda o resultado para apresentar
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // não nos importamos com o conteúdo anterior
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // pronto para apresentar na tela

    // Referência ao attachment dentro do subpass: índice 0, usado como cor.
    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Um único subpass, que escreve só no attachment de cor.
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Dependência: espera a imagem estar disponível antes de escrever nela.
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;   // o que acontece antes do render pass
    dependency.dstSubpass = 0;                     // o subpass 0 (nosso único)
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o render pass");
    }
}

void RenderPass::createFramebuffers(const Swapchain& swapchain) {
    const auto& views = swapchain.imageViews();
    m_framebuffers.resize(views.size());

    // Um framebuffer por image view: cada imagem da swapchain tem o seu.
    for (size_t i = 0; i < views.size(); ++i) {
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_renderPass;
        info.attachmentCount = 1;
        info.pAttachments = &views[i];
        info.width = m_extent.width;
        info.height = m_extent.height;
        info.layers = 1;

        if (vkCreateFramebuffer(m_device, &info, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar framebuffer");
        }
    }
}

void RenderPass::destroy() {
    // Ordem inversa: primeiro os framebuffers, depois o render pass.
    for (VkFramebuffer fb : m_framebuffers) {
        vkDestroyFramebuffer(m_device, fb, nullptr);
    }
    m_framebuffers.clear();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}