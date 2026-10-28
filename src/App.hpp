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
#include "DefaultMaterial.hpp"
#include "Mesh.hpp"
#include "GeometryPipeline.hpp"
#include "LightingPipeline.hpp"
#include "Renderer.hpp"

struct GLFWwindow;

// Orquestra a aplicação: cria os módulos na ordem certa e os libera ao final.
class App {
public:
    void run();

private:
    void initWindow();
    void initVulkan();
    void writeInitialScene();
    void mainLoop();
    void cleanup();

    GLFWwindow* m_window = nullptr;
    Instance m_instance;
    Device m_device;
    Swapchain m_swapchain;
    Shaders m_shaders;
    Buffer m_cameraUbo;
    Buffer m_lightBuffer;
    GBuffer m_gbuffer;
    RenderPass m_renderPass;
    Descriptors m_descriptors;
    DefaultMaterial m_material;
    Mesh m_mesh;
    GeometryPipeline m_geometryPipeline;
    LightingPipeline m_pipeline;
    Renderer m_renderer;
};