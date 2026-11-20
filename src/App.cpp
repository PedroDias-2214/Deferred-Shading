#include "App.hpp"
#include "Config.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

#include "GltfLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#ifndef MODEL_DIR
#define MODEL_DIR "assets/models"

#include <algorithm>
#include <cstdio>
#include <string>

#endif

// Plano quadrado em z = 0, com a normal apontando para +Z (a câmera fica em z positivo).
// Os dois triângulos seguem a ordem anti-horária vista de +Z, que é a face da frente.

// Matriz que centraliza o modelo na origem e o reduz a raio 1, para caber na câmera.
// Com a câmera em z = 3 e FOV de 60 graus, um modelo de raio 1 ocupa boa parte da tela.

static void makeNormalizingMatrix(const MeshData& m, float out[16]) {
    float c[3];
    for (int a = 0; a < 3; ++a) {
        c[a] = 0.5f * (m.boundsMin[a] + m.boundsMax[a]);
    }

    float radius = 0.0f;
    for (const Vertex& v : m.vertices) {
        const float dx = v.position[0] - c[0];
        const float dy = v.position[1] - c[1];
        const float dz = v.position[2] - c[2];
        radius = std::max(radius, std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    const float s = radius > 0.0f ? 1.0f / radius : 1.0f;
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = out[5] = out[10] = s;
    out[15] = 1.0f;
    out[12] = -c[0] * s;
    out[13] = -c[1] * s;
    out[14] = -c[2] * s;
}

namespace {
    // Retorna true só no instante em que a tecla passa de solta para pressionada.
    bool pressedOnce(GLFWwindow* window, int key, bool& wasDown) {
        const bool down = glfwGetKey(window, key) == GLFW_PRESS;
        const bool edge = down && !wasDown;
        wasDown = down;
        return edge;
    }

    // Nome da paleta, para a barra de título.
    const char* paletteName(PointPalette p) {
        switch (p) {
        case PointPalette::White: return "Branca";
        case PointPalette::Warm:  return "Quente";
        case PointPalette::Cold:  return "Fria";
        default:                  return "Arcoiris";
        }
    }

    // Lista completa de comandos.
    void printHelp() {
        std::printf(
            "\n===== COMANDOS =====\n"
            "Camera:\n"
            "  W A S D      andar para frente, tras, esquerda, direita\n"
            "  Espaco       subir\n"
            "  Ctrl / Shift esquerdo  descer\n"
            "  Mouse        olhar em volta (com o mouse preso)\n"
            "  Tab          solta ou prende o mouse\n"
            "  Clique esq.  prende o mouse de novo\n"
            "Luzes:\n"
            "  L            liga / desliga as pontuais\n"
            "  K            liga / desliga os spots\n"
            "  C            troca a paleta das pontuais (Arcoiris, Branca, Quente, Fria)\n"
            "  =  e  -      aumenta / diminui a intensidade\n"
            "Visualizacao (G-Buffer):\n"
            "  0 cena final  |  1 posicao  |  2 normal\n"
            "  3 albedo      |  4 especular|  5 profundidade\n"
            "Outros:\n"
            "  H            mostra esta lista de novo\n"
            "  X da janela  fecha a aplicacao\n"
            "====================\n\n");
        std::fflush(stdout);
    }

} // namespace

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

    // Modelo glTF (GLB): geometria com as transformações aplicadas, textura de cor e de normal.
    const std::string modelPath = std::string(MODEL_DIR) + "/Daniel.glb";
    const ModelData model = GltfLoader::load(modelPath);
    std::printf("Modelo carregado: %zu vértices, %zu índices, albedo %ux%u, normal %ux%u\n",
        model.mesh.vertices.size(), model.mesh.indices.size(),
        model.albedo.width, model.albedo.height, model.normal.width, model.normal.height);

    m_material.create(m_device, model);
    m_descriptors.writeMaterial(m_material.albedoView(), m_material.normalView(),
        m_material.specularView(), m_material.sampler(), m_material.ubo());

    m_mesh.create(m_device.logical(), m_device.physical(), model.mesh);

    m_geometryPipeline.create(m_device.logical(), m_renderPass.handle(), 0, m_shaders, m_descriptors);
    m_pipeline.create(m_device.logical(), m_renderPass.handle(), 1,
        m_shaders, m_descriptors, m_swapchain.format() != VK_FORMAT_B8G8R8A8_SRGB);

    m_camera.init(m_window, WIDTH, HEIGHT);

    m_renderer.create(m_device, m_swapchain, m_renderPass,
        m_geometryPipeline, m_pipeline, m_mesh, m_descriptors, m_cameraUbos, m_lightUbos);

    float modelMatrix[16];
    makeNormalizingMatrix(model.mesh, modelMatrix);
    m_renderer.setModelMatrix(modelMatrix);

    printHelp();
}

// Atualiza o título da janela com o estado atual. Só chama o sistema se o texto mudou.
void App::updateTitle() {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "Deferred Shading | L pontuais:%s  K spots:%s  C paleta:%s  =/- intensidade:%.2f  Tab mouse:%s  H ajuda",
        m_lightSettings.points ? "on" : "off",
        m_lightSettings.spots ? "on" : "off",
        paletteName(m_lightSettings.palette),
        m_lightSettings.intensity,
        m_camera.isCaptured() ? "preso" : "solto");

    if (m_title != buf) {
        m_title = buf;
        glfwSetWindowTitle(m_window, buf);
    }
}

// Lê todo o teclado e o mouse de um quadro. Chamada uma vez por quadro, em mainLoop.
void App::handleInput() {
    // Tab solta ou prende o mouse.
    if (pressedOnce(m_window, GLFW_KEY_TAB, m_downTab)) {
        m_camera.setCaptured(m_window, !m_camera.isCaptured());
    }

    // Clique esquerdo prende o mouse, se ele estiver solto.
    if (!m_camera.isCaptured() &&
        glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        m_camera.setCaptured(m_window, true);
    }

    // Luzes.
    if (pressedOnce(m_window, GLFW_KEY_L, m_downL)) {
        m_lightSettings.points = !m_lightSettings.points;
    }
    if (pressedOnce(m_window, GLFW_KEY_K, m_downK)) {
        m_lightSettings.spots = !m_lightSettings.spots;
    }
    if (pressedOnce(m_window, GLFW_KEY_C, m_downC)) {
        const uint32_t count = static_cast<uint32_t>(PointPalette::Count);
        const uint32_t next = (static_cast<uint32_t>(m_lightSettings.palette) + 1) % count;
        m_lightSettings.palette = static_cast<PointPalette>(next);
    }
    if (pressedOnce(m_window, GLFW_KEY_EQUAL, m_downPlus)) {
        m_lightSettings.intensity = std::min(m_lightSettings.intensity * 1.25f, 8.0f);
    }
    if (pressedOnce(m_window, GLFW_KEY_MINUS, m_downMinus)) {
        m_lightSettings.intensity = std::max(m_lightSettings.intensity / 1.25f, 0.1f);
    }

    // Teclas 0 a 5: modo de visualização do G-Buffer.
    for (int key = GLFW_KEY_0; key <= GLFW_KEY_5; ++key) {
        if (glfwGetKey(m_window, key) == GLFW_PRESS) {
            m_renderer.setDebugView(static_cast<uint32_t>(key - GLFW_KEY_0));
        }
    }

    // H mostra a lista de comandos.
    if (pressedOnce(m_window, GLFW_KEY_H, m_downH)) {
        printHelp();
    }
}

void App::mainLoop() {
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        handleInput();

        // Janela minimizada tem tamanho zero: espera voltar.
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            glfwWaitEvents();
            lastTime = glfwGetTime();
            continue;
        }

        // Tamanho mudou desde a última swapchain: recria o que depende dele.
        const VkExtent2D current = m_swapchain.extent();
        if (static_cast<uint32_t>(fbWidth) != current.width ||
            static_cast<uint32_t>(fbHeight) != current.height) {
            recreateSwapchain();
        }

        const double now = glfwGetTime();
        const float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        m_camera.update(m_window, dt);
        updateTitle();

        CameraData cam{};
        m_camera.fillCameraData(cam);

        LightBlock lights;
        SceneLights::build(static_cast<float>(now), m_lightSettings, lights);

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