#include "App.hpp"
#include "Config.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

// Triângulo no plano z = 0, voltado para a câmera, que começa em z = 3 olhando para -Z.
// A ordem dos vértices é anti-horária vista de +Z, que é a face da frente.
static std::vector<Vertex> makeTriangle() {
    return {
        {{ 0.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    };
}

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

    // Um UBO de câmera por frame em voo, para não alterar a memória que a GPU ainda usa.
    m_cameraUbos.resize(MAX_FRAMES_IN_FLIGHT);
    for (Buffer& ubo : m_cameraUbos) {
        ubo.create(m_device.logical(), m_device.physical(),
            sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    m_lightBuffer.create(m_device.logical(), m_device.physical(),
        sizeof(LightHeader) + MAX_LIGHTS * sizeof(GpuLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    writeInitialScene();

    m_gbuffer.create(m_device, m_swapchain.extent());
    m_renderPass.create(m_device.logical(), m_swapchain, m_gbuffer);

    m_descriptors.create(m_device.logical(), MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_descriptors.writeCamera(i, m_cameraUbos[i]);
    }
    m_descriptors.writeLights(m_lightBuffer);
    m_descriptors.writeGBuffer(m_gbuffer);

    m_material.create(m_device);
    m_descriptors.writeMaterial(m_material.albedoView(), m_material.normalView(),
        m_material.specularView(), m_material.sampler(), m_material.ubo());

    m_mesh.create(m_device.logical(), m_device.physical(), makeTriangle());

    m_geometryPipeline.create(m_device.logical(), m_renderPass.handle(), 0, m_shaders, m_descriptors);
    m_pipeline.create(m_device.logical(), m_renderPass.handle(), 1,
        m_shaders, m_descriptors, m_swapchain.format() != VK_FORMAT_B8G8R8A8_SRGB);

    m_camera.init(m_window, WIDTH, HEIGHT);

    m_renderer.create(m_device, m_swapchain, m_renderPass,
        m_geometryPipeline, m_pipeline, m_mesh, m_descriptors, m_cameraUbos);
}

// Luzes da cena. A câmera é atualizada a cada quadro, em mainLoop.
void App::writeInitialScene() {
    LightHeader header{};
    header.lightCount = 1;
    header.ambientColor[0] = 0.1f;
    header.ambientColor[1] = 0.1f;
    header.ambientColor[2] = 0.1f;
    m_lightBuffer.write(&header, sizeof(header));

    // Luz direcional branca. A direção (0,0,-1) resulta em L = (0,0,1),
    // iluminando a face do triângulo voltada para a câmera.
    GpuLight sun{};
    sun.colorIntensity[0] = 1.0f;
    sun.colorIntensity[1] = 1.0f;
    sun.colorIntensity[2] = 1.0f;
    sun.colorIntensity[3] = 1.0f;
    sun.directionType[2] = -1.0f;
    sun.directionType[3] = 0.0f;  // tipo 0 = direcional
    m_lightBuffer.write(&sun, sizeof(sun), sizeof(header));
}

void App::mainLoop() {
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // Esc fecha a aplicação.
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }

        const double now = glfwGetTime();
        const float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        m_camera.update(m_window, dt);

        CameraData cam{};
        m_camera.fillCameraData(cam);
        m_renderer.drawFrame(cam);
    }
}

void App::cleanup() {
    // Ordem inversa da criação: renderer e pipelines antes dos recursos que usam.
    m_renderer.destroy();
    m_pipeline.destroy();
    m_geometryPipeline.destroy();
    m_mesh.destroy();
    m_material.destroy();
    m_descriptors.destroy();
    m_renderPass.destroy();
    m_gbuffer.destroy();
    m_lightBuffer.destroy();
    for (Buffer& ubo : m_cameraUbos) {
        ubo.destroy();
    }
    m_shaders.destroy();
    m_swapchain.destroy();
    m_device.destroy();
    m_instance.destroy();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}