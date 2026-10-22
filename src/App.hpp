#pragma once

#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "Shaders.hpp"

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

    GLFWwindow* m_window = nullptr;
    Instance m_instance;
    Device m_device;
    Swapchain m_swapchain;
    Shaders m_shaders;
};