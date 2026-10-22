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
    m_renderPass.create(m_device.logical(), m_swapchain);
}

void App::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
    }
}

void App::cleanup() {
    // Ordem inversa da criação.
    m_renderPass.destroy();
    m_shaders.destroy();
    m_swapchain.destroy();
    m_device.destroy();
    m_instance.destroy();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}