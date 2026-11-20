#pragma once

#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "Shaders.hpp"
#include "GBuffer.hpp"
#include "RenderPass.hpp"
#include "Buffer.hpp"
#include "Descriptors.hpp"
#include "SceneData.hpp"
#include "Lights.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "GeometryPipeline.hpp"
#include "LightingPipeline.hpp"
#include "Renderer.hpp"
#include "Camera.hpp"

#include <vector>
#include <string>

struct GLFWwindow;

// Orquestra a aplicação: cria os módulos na ordem certa e os libera ao final.
class App {
public:
    void run();

private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    void recreateSwapchain();

    GLFWwindow* m_window = nullptr;
    Instance m_instance;
    Device m_device;
    Swapchain m_swapchain;
    Shaders m_shaders;
    std::vector<Buffer> m_cameraUbos;  // um por frame em voo
    std::vector<Buffer> m_lightUbos;   // um por frame em voo
    GBuffer m_gbuffer;
    RenderPass m_renderPass;
    Descriptors m_descriptors;
    Material m_material;
    Mesh m_mesh;
    GeometryPipeline m_geometryPipeline;
    LightingPipeline m_pipeline;
    Renderer m_renderer;
    Camera m_camera;
    LightSettings m_lightSettings;
    bool m_downL = false;
    bool m_downK = false;
    bool m_downC = false;
    bool m_downPlus = false;
    bool m_downMinus = false;
    bool m_downH = false;
    std::string m_title;
    void updateTitle();
    bool m_downTab = false;
    void handleInput();
};