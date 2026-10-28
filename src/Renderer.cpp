#include "Renderer.hpp"
#include "SceneData.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

void Renderer::create(const Device& device, const Swapchain& swapchain, const RenderPass& renderPass,
    const GeometryPipeline& geometry, const LightingPipeline& lighting,
    const Mesh& mesh, const Descriptors& descriptors,
    std::vector<Buffer>& cameraUbos, std::vector<Buffer>& lightUbos)
{
    m_device = device.logical();
    m_graphicsQueue = device.graphicsQueue();
    m_presentQueue = device.presentQueue();
    m_swapchain = swapchain.handle();
    m_renderPass = &renderPass;
    m_geometry = &geometry;
    m_lighting = &lighting;
    m_mesh = &mesh;
    m_descriptors = &descriptors;
    m_cameraUbos = &cameraUbos;
    m_lightUbos = &lightUbos;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = device.queueFamilies().graphicsFamily.value();

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o command pool");
    }

    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(m_device, &alloc, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar command buffers");
    }

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

    const size_t imageCount = swapchain.imageViews().size();
    m_renderFinished.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar semáforo de renderização");
        }
    }

    std::printf("Renderer criado: %u frames em voo, %zu imagens\n", MAX_FRAMES_IN_FLIGHT, imageCount);
}

bool Renderer::drawFrame(const CameraData& camera, const LightBlock& lights) {
    // Espera a GPU terminar o frame que usava este slot.
    vkWaitForFences(m_device, 1, &m_inFlight[m_currentFrame], VK_TRUE, UINT64_MAX);

    // Com o fence cumprido, a GPU não lê mais este slot.
    (*m_cameraUbos)[m_currentFrame].write(&camera, sizeof(camera));

    Buffer& lightUbo = (*m_lightUbos)[m_currentFrame];
    lightUbo.write(&lights.header, sizeof(LightHeader));
    if (!lights.lights.empty()) {
        lightUbo.write(lights.lights.data(), lights.lights.size() * sizeof(GpuLight), sizeof(LightHeader));
    }

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
        m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Nada foi submetido, então o slot continua disponível para o próximo quadro.
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Falha ao adquirir imagem da swapchain");
    }
    bool stillValid = (result == VK_SUCCESS);

    vkResetFences(m_device, 1, &m_inFlight[m_currentFrame]);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);
    recordCommands(cmd, imageIndex, m_currentFrame);

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

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &m_renderFinished[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &present);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        stillValid = false;
    }
    else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("Falha ao apresentar a imagem");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return stillValid;
}

void Renderer::onSwapchainRecreated(const Swapchain& swapchain) {
    m_swapchain = swapchain.handle();

    // Um semáforo de apresentação por imagem. A quantidade pode mudar com a nova swapchain.
    for (VkSemaphore s : m_renderFinished) {
        vkDestroySemaphore(m_device, s, nullptr);
    }

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    m_renderFinished.assign(swapchain.imageViews().size(), VK_NULL_HANDLE);
    for (VkSemaphore& s : m_renderFinished) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &s) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao recriar semáforo de renderização");
        }
    }
}

void Renderer::recordCommands(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t frame) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao iniciar o command buffer");
    }

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

    // Os quatro sets na ordem dos números do shader. Câmera e luzes são de cada frame;
    // material e G-Buffer são compartilhados. Os dois pipelines têm os mesmos layouts.
    const VkDescriptorSet sets[SET_COUNT] = {
        m_descriptors->cameraSet(frame),
        m_descriptors->materialSet(),
        m_descriptors->lightSet(frame),
        m_descriptors->gbufferSet(),
    };
    auto bindSets = [&](VkPipelineLayout layout) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
            0, SET_COUNT, sets, 0, nullptr);
        };

    // ---- Subpass 0: geometria ----
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geometry->handle());
    bindSets(m_geometry->layout());
    m_mesh->bind(cmd);

    ObjectPushConstants obj{};
    setIdentity(obj.model);
    setIdentity(obj.normalMatrix);
    vkCmdPushConstants(cmd, m_geometry->layout(), VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(obj), &obj);

    vkCmdDraw(cmd, m_mesh->vertexCount(), 1, 0, 0);

    // ---- Subpass 1: iluminação ----
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lighting->handle());
    bindSets(m_lighting->layout());

    LightingPushConstants pc{};
    pc.debugView = m_debugView;
    vkCmdPushConstants(cmd, m_lighting->layout(), VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(pc), &pc);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao finalizar o command buffer");
    }
}

void Renderer::destroy() {
    if (m_device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(m_device);

    for (VkFence f : m_inFlight) vkDestroyFence(m_device, f, nullptr);
    for (VkSemaphore s : m_imageAvailable) vkDestroySemaphore(m_device, s, nullptr);
    for (VkSemaphore s : m_renderFinished) vkDestroySemaphore(m_device, s, nullptr);

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