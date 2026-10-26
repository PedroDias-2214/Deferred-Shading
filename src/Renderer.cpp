#include "Renderer.hpp"

#include <cstdio>
#include <stdexcept>

void Renderer::create(const Device& device, const Swapchain& swapchain, const RenderPass& renderPass,
    const LightingPipeline& pipeline, const Descriptors& descriptors)
{
    m_device = device.logical();
    m_graphicsQueue = device.graphicsQueue();
    m_presentQueue = device.presentQueue();
    m_swapchain = swapchain.handle();
    m_renderPass = &renderPass;
    m_pipeline = &pipeline;
    m_descriptors = &descriptors;

    // ---- Command pool: permite resetar cada command buffer separadamente ----
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = device.queueFamilies().graphicsFamily.value();

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o command pool");
    }

    // ---- Command buffers: um por frame em voo ----
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(m_device, &alloc, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar command buffers");
    }

    // ---- Sincronização ----
    // Fences começam sinalizadas: a primeira espera de cada frame passa sem bloquear.
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    m_imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlight.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailable[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlight[i]) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar semáforos ou fences");
        }
    }

    // O semáforo de "renderização terminada" é usado pela apresentação, que é
    // por imagem da swapchain, então há um para cada imagem.
    const size_t imageCount = swapchain.imageViews().size();
    m_renderFinished.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar semáforo de renderização");
        }
    }

    std::printf("Renderer criado: %u frames em voo, %zu imagens\n", MAX_FRAMES_IN_FLIGHT, imageCount);
}

void Renderer::drawFrame() {
    // Espera a GPU terminar o frame que usava este slot antes de reaproveitá-lo.
    vkWaitForFences(m_device, 1, &m_inFlight[m_currentFrame], VK_TRUE, UINT64_MAX);

    // Pede a próxima imagem livre da swapchain. O semáforo avisa quando ela estiver pronta.
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
        m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        throw std::runtime_error("Swapchain desatualizada (redimensionamento ainda não tratado)");
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Falha ao adquirir imagem da swapchain");
    }

    // Só agora resetamos o fence, porque vamos submeter um novo trabalho com ele.
    vkResetFences(m_device, 1, &m_inFlight[m_currentFrame]);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);
    recordCommands(cmd, imageIndex);

    // Submissão: espera a imagem estar pronta antes de escrever nela.
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_imageAvailable[m_currentFrame];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_renderFinished[imageIndex];

    if (vkQueueSubmit(m_graphicsQueue, 1, &submit, m_inFlight[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao submeter comandos à fila gráfica");
    }

    // Apresentação: só mostra a imagem depois que a renderização terminar.
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &m_renderFinished[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_presentQueue, &present);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Falha ao apresentar a imagem");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao iniciar o command buffer");
    }

    // ---- Início do render pass: limpa os attachments ----
    std::vector<VkClearValue> clears = m_renderPass->clearValues();

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_renderPass->handle();
    rpInfo.framebuffer = m_renderPass->framebuffer(imageIndex);
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = m_renderPass->extent();
    rpInfo.clearValueCount = static_cast<uint32_t>(clears.size());
    rpInfo.pClearValues = clears.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ---- Subpass 0: geometria ----
    // Ainda não há malhas, então nada é desenhado aqui. O G-Buffer fica limpo.
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

    // ---- Subpass 1: iluminação ----
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->handle());

    // Os quatro sets, na ordem dos números do shader.
    VkDescriptorSet sets[SET_COUNT];
    for (uint32_t i = 0; i < SET_COUNT; ++i) {
        sets[i] = m_descriptors->set(static_cast<DescriptorSet>(i));
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->layout(),
        0, SET_COUNT, sets, 0, nullptr);

    // Viewport e scissor são dinâmicos, então precisam ser definidos aqui.
    VkExtent2D ext = m_renderPass->extent();

    VkViewport viewport{};
    viewport.width = static_cast<float>(ext.width);
    viewport.height = static_cast<float>(ext.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = ext;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Parâmetros padrão do lighting pass.
    LightingPushConstants pc{};
    vkCmdPushConstants(cmd, m_pipeline->layout(), VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(pc), &pc);

    // Triângulo de tela cheia: 3 vértices gerados pelo shader, sem vertex buffer.
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao finalizar o command buffer");
    }
}

void Renderer::destroy() {
    if (m_device == VK_NULL_HANDLE) return;

    // Espera a GPU terminar tudo antes de destruir qualquer recurso usado por ela.
    vkDeviceWaitIdle(m_device);

    for (VkFence f : m_inFlight) vkDestroyFence(m_device, f, nullptr);
    for (VkSemaphore s : m_imageAvailable) vkDestroySemaphore(m_device, s, nullptr);
    for (VkSemaphore s : m_renderFinished) vkDestroySemaphore(m_device, s, nullptr);

    // Destruir o pool libera os command buffers alocados nele.
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    m_inFlight.clear();
    m_imageAvailable.clear();
    m_renderFinished.clear();
    m_commandBuffers.clear();
    m_device = VK_NULL_HANDLE;
}