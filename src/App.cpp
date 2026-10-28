#include "App.hpp"
#include "Config.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

// Triângulo no espaço de recorte, à frente da câmera (z = 0.5).
// Está em sentido anti-horário, então a face fica voltada para a câmera.
static std::vector<Vertex> makeTriangle() {
    return {
        {{ 0.0f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
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

    m_cameraUbo.create(m_device.logical(), m_device.physical(),
        sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_lightBuffer.create(m_device.logical(), m_device.physical(),
        sizeof(LightHeader) + MAX_LIGHTS * sizeof(GpuLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    writeInitialScene();

    m_gbuffer.create(m_device, m_swapchain.extent());
    m_renderPass.create(m_device.logical(), m_swapchain, m_gbuffer);

    m_descriptors.create(m_device.logical());
    m_descriptors.writeCamera(m_cameraUbo);
    m_descriptors.writeLights(m_lightBuffer);
    m_descriptors.writeGBuffer(m_gbuffer);

    m_material.create(m_device);
    m_descriptors.writeMaterial(m_material.albedoView(), m_material.normalView(),
        m_material.specularView(), m_material.sampler(), m_material.ubo());

    m_mesh.create(m_device.logical(), m_device.physical(), makeTriangle());

    m_geometryPipeline.create(m_device.logical(), m_renderPass.handle(), 0, m_shaders, m_descriptors);
    m_pipeline.create(m_device.logical(), m_renderPass.handle(), 1,
        m_shaders, m_descriptors, m_swapchain.format() != VK_FORMAT_B8G8R8A8_SRGB);

    m_renderer.create(m_device, m_swapchain, m_renderPass,
        m_geometryPipeline, m_pipeline, m_mesh, m_descriptors);
}

// Dados iniciais da cena: câmera em identidade e uma luz direcional.
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
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        m_renderer.drawFrame();
    }
}

void App::cleanup() {
    // Ordem inversa da criação: pipelines e renderer antes dos recursos que usam.
    m_renderer.destroy();
    m_pipeline.destroy();
    m_geometryPipeline.destroy();
    m_mesh.destroy();
    m_material.destroy();
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