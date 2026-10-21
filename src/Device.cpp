#include "Device.hpp"
#include "Config.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

void Device::create(VkInstance instance, GLFWwindow* window) {
    m_instance = instance;
    createSurface(window);     // primeiro a surface: a escolha da GPU depende dela
    pickPhysicalDevice();
    createLogicalDevice();
}

void Device::destroy() {
    // Ordem inversa da criação: dispositivo lógico, depois a surface.
    // A instância é destruída por quem a criou, depois deste método.
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

// ------------------------------------------------------------------
// Surface: liga o Vulkan à janela do sistema operacional.
// ------------------------------------------------------------------
void Device::createSurface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar a surface");
    }
}

// ------------------------------------------------------------------
// Escolha da GPU: testa cada dispositivo físico e pega o primeiro adequado.
// ------------------------------------------------------------------
void Device::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("Nenhuma GPU com suporte a Vulkan foi encontrada");
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (VkPhysicalDevice candidate : devices) {
        if (isDeviceSuitable(candidate)) {
            m_physical = candidate;
            break;
        }
    }

    if (m_physical == VK_NULL_HANDLE) {
        throw std::runtime_error("Nenhuma GPU adequada encontrada");
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physical, &props);
    std::printf("GPU selecionada: %s\n", props.deviceName);
}

bool Device::isDeviceSuitable(VkPhysicalDevice candidate) const {
    QueueFamilyIndices indices = findQueueFamilies(candidate);
    bool extensionsOk = checkDeviceExtensionSupport(candidate);

    bool swapchainOk = false;
    if (extensionsOk) {
        SwapchainSupport support = querySwapchainSupport(candidate);
        swapchainOk = !support.formats.empty() && !support.presentModes.empty();
    }

    return indices.isComplete() && extensionsOk && swapchainOk;
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice candidate) const {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Verifica se esta família consegue apresentar na nossa surface.
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
    }

    return indices;
}

bool Device::checkDeviceExtensionSupport(VkPhysicalDevice candidate) const {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, available.data());

    // Remove do conjunto as extensões exigidas que a GPU oferece.
    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : available) {
        required.erase(ext.extensionName);
    }
    return required.empty();
}

SwapchainSupport Device::querySwapchainSupport(VkPhysicalDevice candidate) const {
    SwapchainSupport details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, m_surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, m_surface, &formatCount, nullptr);
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, m_surface, &formatCount, details.formats.data());

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, m_surface, &modeCount, nullptr);
    details.presentModes.resize(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, m_surface, &modeCount, details.presentModes.data());

    return details;
}

// ------------------------------------------------------------------
// Dispositivo lógico: a interface que usaremos daqui para frente.
// ------------------------------------------------------------------
void Device::createLogicalDevice() {
    m_queueFamilies = findQueueFamilies(m_physical);

    // Se graphics e present estiverem na mesma família, criamos só uma fila.
    std::set<uint32_t> uniqueFamilies = {
        m_queueFamilies.graphicsFamily.value(),
        m_queueFamilies.presentFamily.value()
    };

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(queueInfo);
    }

    // Recursos opcionais da GPU. Nenhum é necessário por enquanto.
    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_physical, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o dispositivo lógico");
    }

    vkGetDeviceQueue(m_device, m_queueFamilies.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueFamilies.presentFamily.value(), 0, &m_presentQueue);
}