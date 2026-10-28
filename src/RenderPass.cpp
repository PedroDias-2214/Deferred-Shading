#include "RenderPass.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>

void RenderPass::create(VkDevice device, const Swapchain& swapchain, const GBuffer& gbuffer) {
    m_device = device;
    m_extent = swapchain.extent();

    createRenderPass(swapchain.format());
    createFramebuffers(swapchain, gbuffer);

    std::printf("Render pass (2 subpasses, %u attachments) e %zu framebuffers criados\n",
        ATTACHMENT_COUNT, m_framebuffers.size());
}

void RenderPass::createRenderPass(VkFormat swapchainFormat) {
    // Helper: descreve um attachment de cor ou profundidade sem stencil.
    auto describe = [](VkFormat format, VkAttachmentLoadOp load, VkAttachmentStoreOp store,
        VkImageLayout finalLayout) {
            VkAttachmentDescription d{};
            d.format = format;
            d.samples = VK_SAMPLE_COUNT_1_BIT;
            d.loadOp = load;
            d.storeOp = store;
            d.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            d.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            d.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            d.finalLayout = finalLayout;
            return d;
        };

    // ---- Attachments ---------------------------------------------------
    // O G-Buffer não precisa ser guardado: só existe dentro do render pass.
    std::array<VkAttachmentDescription, ATTACHMENT_COUNT> attachments{};
    attachments[ATT_SWAPCHAIN] = describe(swapchainFormat,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    attachments[ATT_POSITION] = describe(GBuffer::POSITION_FORMAT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    attachments[ATT_NORMAL] = describe(GBuffer::NORMAL_FORMAT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    attachments[ATT_ALBEDO_SPEC] = describe(GBuffer::ALBEDO_SPEC_FORMAT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    attachments[ATT_DEPTH] = describe(GBuffer::DEPTH_FORMAT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    // ---- Subpass 0: geometria -------------------------------------------
    // Escreve nos três attachments de cor do G-Buffer e no de profundidade.
    std::array<VkAttachmentReference, 3> geomColor = { {
        {ATT_POSITION, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {ATT_NORMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {ATT_ALBEDO_SPEC, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    } };
    VkAttachmentReference geomDepth{ ATT_DEPTH, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    // ---- Subpass 1: iluminação ------------------------------------------
    // A ordem aqui define os input_attachment_index 0..3 usados no lighting.frag.
    std::array<VkAttachmentReference, 4> lightInput = { {
        {ATT_POSITION, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},       // índice 0: gPosition
        {ATT_NORMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},         // índice 1: gNormal
        {ATT_ALBEDO_SPEC, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},    // índice 2: gAlbedoSpec
        {ATT_DEPTH, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},   // índice 3: gDepth
    } };
    VkAttachmentReference lightColor{ ATT_SWAPCHAIN, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    std::array<VkSubpassDescription, 2> subpasses{};

    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = static_cast<uint32_t>(geomColor.size());
    subpasses[0].pColorAttachments = geomColor.data();
    subpasses[0].pDepthStencilAttachment = &geomDepth;

    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[1].inputAttachmentCount = static_cast<uint32_t>(lightInput.size());
    subpasses[1].pInputAttachments = lightInput.data();
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &lightColor;

    // ---- Dependências ---------------------------------------------------
    std::array<VkSubpassDependency, 3> deps{};

    // Antes da geometria: a imagem da swapchain e o G-Buffer precisam estar prontos para escrita.
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].dstStageMask = deps[0].srcStageMask;
    deps[0].srcAccessMask = 0;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Entre geometria e iluminação: a leitura como input só começa depois das escritas.
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = 1;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Depois da iluminação: a swapchain fica pronta para apresentação.
    deps[2].srcSubpass = 1;
    deps[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[2].dstAccessMask = 0;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = static_cast<uint32_t>(subpasses.size());
    info.pSubpasses = subpasses.data();
    info.dependencyCount = static_cast<uint32_t>(deps.size());
    info.pDependencies = deps.data();

    if (vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o render pass");
    }
}

void RenderPass::createFramebuffers(const Swapchain& swapchain, const GBuffer& gbuffer) {
    const auto& swapViews = swapchain.imageViews();
    m_framebuffers.resize(swapViews.size());

    for (size_t i = 0; i < swapViews.size(); ++i) {
        // A ordem precisa ser exatamente a dos índices ATT_* do render pass.
        std::array<VkImageView, ATTACHMENT_COUNT> views = { {
            swapViews[i],
            gbuffer.positionView(),
            gbuffer.normalView(),
            gbuffer.albedoSpecView(),
            gbuffer.depthView(),
        } };

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_renderPass;
        info.attachmentCount = ATTACHMENT_COUNT;
        info.pAttachments = views.data();
        info.width = m_extent.width;
        info.height = m_extent.height;
        info.layers = 1;

        if (vkCreateFramebuffer(m_device, &info, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar framebuffer");
        }
    }
}

std::vector<VkClearValue> RenderPass::clearValues() const {
    std::vector<VkClearValue> values(ATTACHMENT_COUNT);
    values[ATT_SWAPCHAIN].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    values[ATT_POSITION].color = { {0.0f, 0.0f, 0.0f, 0.0f} };    // w = 0: sem geometria
    values[ATT_NORMAL].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    values[ATT_ALBEDO_SPEC].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    values[ATT_DEPTH].depthStencil = { 1.0f, 0 };                 // plano distante
    return values;
}

void RenderPass::destroy() {
    destroyFramebuffers();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

void RenderPass::destroyFramebuffers() {
    for (VkFramebuffer fb : m_framebuffers) {
        vkDestroyFramebuffer(m_device, fb, nullptr);
    }
    m_framebuffers.clear();
}

void RenderPass::buildFramebuffers(const Swapchain& swapchain, const GBuffer& gbuffer) {
    m_extent = swapchain.extent();
    createFramebuffers(swapchain, gbuffer);
}