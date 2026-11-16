#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Device.hpp"
#include "Swapchain.hpp"
#include "RenderPass.hpp"
#include "GeometryPipeline.hpp"
#include "LightingPipeline.hpp"
#include "Mesh.hpp"
#include "Descriptors.hpp"
#include "Buffer.hpp"
#include "SceneData.hpp"

// Quantos frames podem estar sendo preparados ao mesmo tempo pela GPU.
inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
    // Matriz de modelo (16 floats, em ordem de colunas) usada para desenhar a malha.
    void setModelMatrix(const float m[16]);
    // cameraUbos e lightUbos: um buffer por frame em voo. O Renderer os preenche, mas não é dono.
    void create(const Device& device, const Swapchain& swapchain, const RenderPass& renderPass,
        const GeometryPipeline& geometry, const LightingPipeline& lighting,
        const Mesh& mesh, const Descriptors& descriptors,
        std::vector<Buffer>& cameraUbos, std::vector<Buffer>& lightUbos);
    void destroy();

    // Escreve câmera e luzes no slot do frame atual e desenha o frame.
    // Retorna false quando a swapchain precisa ser recriada.
    bool drawFrame(const CameraData& camera, const LightBlock& lights);

    // Chamado depois que a swapchain foi recriada: atualiza o handle e os semáforos.
    void onSwapchainRecreated(const Swapchain& swapchain);
    void setDebugView(uint32_t view) { m_debugView = view; }

private:
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t frame);

    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

    // Recursos de outras classes. Não são donos: o App cuida da vida deles.
    const RenderPass* m_renderPass = nullptr;
    const GeometryPipeline* m_geometry = nullptr;
    const LightingPipeline* m_lighting = nullptr;
    const Mesh* m_mesh = nullptr;
    const Descriptors* m_descriptors = nullptr;
    std::vector<Buffer>* m_cameraUbos = nullptr;
    std::vector<Buffer>* m_lightUbos = nullptr;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence> m_inFlight;
    uint32_t m_debugView = 0;
    float m_model[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

    uint32_t m_currentFrame = 0;
};