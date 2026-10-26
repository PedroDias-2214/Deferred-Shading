#include "App.hpp"
#include "Config.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>

void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void App::initWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Falha ao inicializar o GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(WIDTH, HEIGHT, "Deferred Shading", nullptr, nullptr);
    if (!m_window) {
        throw std::runtime_error("Falha ao criar a janela");
    }
}

void App::initVulkan() {
    m_instance.create();
    m_device.create(m_instance.handle(), m_window);
    m_swapchain.create(m_device, m_window);
    m_shaders.load(m_device.logical(), {
        "fullscreen.vert.spv",
        "geometry.vert.spv",
        "geometry.frag.spv",
        "lighting.frag.spv",
        });

    // Buffers com os dados da cena, lidos pelos shaders através dos descriptor sets.
    m_cameraUbo.create(m_device.logical(), m_device.physical(),
        sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_lightBuffer.create(m_device.logical(), m_device.physical(),
        sizeof(LightHeader) + MAX_LIGHTS * sizeof(GpuLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    writeInitialScene();

    m_gbuffer.create(m_device, m_swapchain.extent());
    m_renderPass.create(m_device.logical(), m_swapchain, m_gbuffer);

    // Descriptor sets ligam os buffers e o G-Buffer aos shaders.
    m_descriptors.create(m_device.logical());
    m_descriptors.writeCamera(m_cameraUbo);
    m_descriptors.writeLights(m_lightBuffer);
    m_descriptors.writeGBuffer(m_gbuffer);
    // Pipeline do lighting pass: usa os shaders e os sets criados acima.
    // Se a swapchain não for sRGB, o shader precisa aplicar a correção de gama.
    m_pipeline.create(m_device.logical(), m_renderPass.handle(), 1,
        m_shaders, m_descriptors, m_swapchain.format() != VK_FORMAT_B8G8R8A8_SRGB);
    m_renderer.create(m_device, m_swapchain, m_renderPass, m_pipeline, m_descriptors);
}

// Dados iniciais da cena. As matrizes são identidade por enquanto:
// a câmera real entra em uma etapa posterior.
void App::writeInitialScene() {
    CameraData cam{};
    setIdentity(cam.view);
    setIdentity(cam.proj);
    setIdentity(cam.viewProj);
    setIdentity(cam.invView);
    setIdentity(cam.invProj);
    cam.cameraPos[3] = 1.0f;
    cam.screenSize[0] = static_cast<float>(WIDTH);
    cam.screenSize[1] = static_cast<float>(HEIGHT);
    cam.nearPlane = 0.1f;
    cam.farPlane = 1000.0f;
    m_cameraUbo.write(&cam, sizeof(cam));

    // Sem luzes por enquanto. Só a luz ambiente, para o shader ter um valor válido.
    LightHeader header{};
    header.lightCount = 0;
    header.ambientColor[0] = 0.1f;
    header.ambientColor[1] = 0.1f;
    header.ambientColor[2] = 0.1f;
    m_lightBuffer.write(&header, sizeof(header));
}

void App::cleanup() {
    // Ordem inversa da criação: descriptors antes dos recursos que eles apontam.
    m_renderer.destroy();
    m_pipeline.destroy();
    m_descriptors.destroy();
    m_renderPass.destroy();
    m_gbuffer.destroy();
    m_lightBuffer.destroy();
    m_cameraUbo.destroy();
    m_shaders.destroy();
    m_swapchain.destroy();
    m_device.destroy();
    m_instance.destroy();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void App::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        m_renderer.drawFrame();
    }
}

