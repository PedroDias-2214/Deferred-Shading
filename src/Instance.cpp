#include "Instance.hpp"
#include "Config.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE   // impede o GLFW de incluir o Vulkan por conta própria
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

// Callback chamado pela camada de validação para cada aviso ou erro.
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::fprintf(stderr, "[validação] %s\n", data->pMessage);
    }
    return VK_FALSE;  // não aborta a chamada que gerou a mensagem
}

void Instance::create() {
    if (enableValidation && !checkValidationLayerSupport()) {
        throw std::runtime_error("Camada de validação não encontrada (instale o Vulkan SDK)");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Deferred Shading";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = getRequiredExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Mensagens de validação também durante a criação/destruição da instância.
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = makeDebugMessengerInfo();
    if (enableValidation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.pNext = &debugInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar a instância Vulkan");
    }

    // Funções de extensão não são exportadas diretamente: são buscadas pelo nome.
    if (enableValidation) {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!func) throw std::runtime_error("vkCreateDebugUtilsMessengerEXT não encontrada");

        if (func(m_instance, &debugInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar o debug messenger");
        }
    }
}

void Instance::destroy() {
    // Ordem inversa da criação: primeiro o messenger, depois a instância.
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) func(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

std::vector<const char*> Instance::getRequiredExtensions() const {
    uint32_t glfwCount = 0;
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
    std::vector<const char*> extensions(glfwExt, glfwExt + glfwCount);
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

bool Instance::checkValidationLayerSupport() const {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());

    for (const char* name : validationLayers) {
        bool found = false;
        for (const auto& layer : available) {
            if (std::strcmp(name, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

VkDebugUtilsMessengerCreateInfoEXT Instance::makeDebugMessengerInfo() const {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    return info;
}