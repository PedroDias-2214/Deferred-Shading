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

// Quantos frames podem estar sendo preparados ao mesmo tempo pela GPU.
inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// Dono dos comandos, da sincronização e do loop de desenho.
class Renderer {
public:
    void create(const Device& device, const Swapchain& swapchain, const RenderPass& renderPass,
        const GeometryPipeline& geometry, const LightingPipeline& lighting,
        const Mesh& mesh, const Descriptors& descriptors);
    void destroy();

    // Desenha e apresenta um frame.
    void drawFrame();

private:
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex);

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

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence> m_inFlight;

    uint32_t m_currentFrame = 0;
};