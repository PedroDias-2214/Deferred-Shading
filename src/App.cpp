#include "App.hpp"
#include "Config.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

// Plano quadrado em z = 0, com a normal apontando para +Z (a câmera fica em z positivo).
// Os dois triângulos seguem a ordem anti-horária vista de +Z, que é a face da frente.
static std::vector<Vertex> makePlane(float h) {
    const Vertex a{ {-h, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
    const Vertex b{ { h, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
    const Vertex c{ { h,  h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
    const Vertex d{ {-h,  h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
    return { a, b, c, a, c, d };
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
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

    // Um buffer de câmera e um de luzes por frame em voo: a CPU escreve só no slot do
    // frame atual, sem mexer nos dados que a GPU ainda lê dos outros frames.
    m_cameraUbos.resize(MAX_FRAMES_IN_FLIGHT);
    m_lightUbos.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_cameraUbos[i].create(m_device.logical(), m_device.physical(),
            sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        m_lightUbos[i].create(m_device.logical(), m_device.physical(),
            sizeof(LightHeader) + MAX_LIGHTS * sizeof(GpuLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }

    m_gbuffer.create(m_device, m_swapchain.extent());
    m_renderPass.create(m_device.logical(), m_swapchain, m_gbuffer);

    m_descriptors.create(m_device.logical(), MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_descriptors.writeCamera(i, m_cameraUbos[i]);
        m_descriptors.writeLights(i, m_lightUbos[i]);
    }
    m_descriptors.writeGBuffer(m_gbuffer);

    m_material.create(m_device);
    m_descriptors.writeMaterial(m_material.albedoView(), m_material.normalView(),
        m_material.specularView(), m_material.sampler(), m_material.ubo());

    m_mesh.create(m_device.logical(), m_device.physical(), makePlane(2.0f));

    m_geometryPipeline.create(m_device.logical(), m_renderPass.handle(), 0, m_shaders, m_descriptors);
    m_pipeline.create(m_device.logical(), m_renderPass.handle(), 1,
        m_shaders, m_descriptors, m_swapchain.format() != VK_FORMAT_B8G8R8A8_SRGB);

    m_camera.init(m_window, WIDTH, HEIGHT);

    m_renderer.create(m_device, m_swapchain, m_renderPass,
        m_geometryPipeline, m_pipeline, m_mesh, m_descriptors, m_cameraUbos, m_lightUbos);
}

void App::mainLoop() {
    double lastTime = glfwGetTime();
    bool tabWasDown = false;

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // Tab alterna entre controlar a câmera e usar o mouse normalmente.
        const bool tabDown = glfwGetKey(m_window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabDown && !tabWasDown) {
            m_camera.setCaptured(m_window, !m_camera.isCaptured());
        }
        tabWasDown = tabDown;

        // Clique esquerdo na janela prende o mouse de novo.
        if (!m_camera.isCaptured() &&
            glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            m_camera.setCaptured(m_window, true);
        }

        // Esc solta o mouse. Não fecha a aplicação: para fechar, use o X da janela.
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS && m_camera.isCaptured()) {
            m_camera.setCaptured(m_window, false);
        }

        // Teclas 0 a 5 trocam o modo de depuração.
        for (int key = GLFW_KEY_0; key <= GLFW_KEY_5; ++key) {
            if (glfwGetKey(m_window, key) == GLFW_PRESS) {
                m_renderer.setDebugView(static_cast<uint32_t>(key - GLFW_KEY_0));
            }
        }

        // Janela minimizada tem tamanho zero, e isso não pode ter swapchain. Espera voltar.
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            glfwWaitEvents();
            lastTime = glfwGetTime();
            continue;
        }

        // O tamanho mudou desde a última swapchain: recria o que depende dele.
        const VkExtent2D current = m_swapchain.extent();
        if (static_cast<uint32_t>(fbWidth) != current.width ||
            static_cast<uint32_t>(fbHeight) != current.height) {
            recreateSwapchain();
        }

        const double now = glfwGetTime();
        const float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        m_camera.update(m_window, dt);

        CameraData cam{};
        m_camera.fillCameraData(cam);

        LightBlock lights;
        SceneLights::build(static_cast<float>(now), lights);

        // Se a swapchain ficou inválida durante o quadro, recria antes do próximo.
        if (!m_renderer.drawFrame(cam, lights)) {
            recreateSwapchain();
        }
    }
}

void App::cleanup() {
    // Ordem inversa da criação.
    m_renderer.destroy();
    m_pipeline.destroy();
    m_geometryPipeline.destroy();
    m_mesh.destroy();
    m_material.destroy();
    m_descriptors.destroy();
    m_renderPass.destroy();
    m_gbuffer.destroy();
    for (Buffer& ubo : m_lightUbos) {
        ubo.destroy();
    }
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

// Recria os recursos que dependem do tamanho da janela. O render pass e os pipelines
// não mudam: os formatos continuam iguais, e o viewport já é dinâmico.
void App::recreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    if (w == 0 || h == 0) return;  // minimizada: o loop tenta de novo depois

    // Garante que a GPU terminou de usar tudo antes de destruir.
    vkDeviceWaitIdle(m_device.logical());

    // Ordem: framebuffers primeiro (eles referenciam as views), depois a swapchain e o G-Buffer.
    m_renderPass.destroyFramebuffers();
    m_swapchain.destroy();
    m_swapchain.create(m_device, m_window);

    const VkExtent2D ext = m_swapchain.extent();
    m_gbuffer.destroy();
    m_gbuffer.create(m_device, ext);

    m_renderPass.buildFramebuffers(m_swapchain, m_gbuffer);
    m_descriptors.writeGBuffer(m_gbuffer);
    m_renderer.onSwapchainRecreated(m_swapchain);
    m_camera.setViewport(ext.width, ext.height);
}