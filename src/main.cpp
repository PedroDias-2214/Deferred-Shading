#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE   // impede o GLFW de incluir o Vulkan por conta própria
#include <GLFW/glfw3.h>

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

// Validação só em Debug. Em Release (NDEBUG definido) fica desligada.
#ifdef NDEBUG
const bool enableValidation = false;
#else
const bool enableValidation = true;
#endif

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

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
    return VK_FALSE;  // VK_FALSE: não aborta a chamada que gerou a mensagem
}

class App {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    // ------------------------------------------------------------------
    // Janela. GLFW_NO_API avisa que não é OpenGL: o Vulkan cuida do desenho.
    // ------------------------------------------------------------------
    void initWindow() {
        if (!glfwInit()) {
            throw std::runtime_error("Falha ao inicializar o GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);  // resize será tratado depois (swapchain)
        window = glfwCreateWindow(WIDTH, HEIGHT, "Deferred Shading", nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Falha ao criar a janela");
        }
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
    }

    // ------------------------------------------------------------------
    // Instância: a conexão com a biblioteca Vulkan.
    // ------------------------------------------------------------------
    void createInstance() {
        if (enableValidation && !checkValidationLayerSupport()) {
            throw std::runtime_error("Camada de validação não encontrada (instale o Vulkan SDK)");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Deferred Shading";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;  // a versão alvo do trabalho

        // Extensões que o GLFW precisa para desenhar na janela (surface).
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

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar a instância Vulkan");
        }
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwCount = 0;
        const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
        std::vector<const char*> extensions(glfwExt, glfwExt + glfwCount);
        if (enableValidation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    bool checkValidationLayerSupport() {
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

    VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerInfo() {
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

    void setupDebugMessenger() {
        if (!enableValidation) return;

        // Funções de extensão não são exportadas diretamente: são buscadas pelo nome.
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!func) throw std::runtime_error("vkCreateDebugUtilsMessengerEXT não encontrada");

        VkDebugUtilsMessengerCreateInfoEXT info = makeDebugMessengerInfo();
        if (func(instance, &info, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar o debug messenger");
        }
    }

    // ------------------------------------------------------------------
    // Loop principal: por enquanto só processa eventos da janela.
    // ------------------------------------------------------------------
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    // ------------------------------------------------------------------
    // Liberação: na ordem inversa da criação.
    // ------------------------------------------------------------------
    void cleanup() {
        if (enableValidation && debugMessenger != VK_NULL_HANDLE) {
            auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (func) func(instance, debugMessenger, nullptr);
        }
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    SetConsoleOutputCP(CP_UTF8);
    try {
        App app;
        app.run();
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "Erro: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}