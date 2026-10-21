#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

struct GLFWwindow;

// Índices das filas que o programa precisa. Podem apontar para a mesma família.
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;  // desenha
    std::optional<uint32_t> presentFamily;   // apresenta na surface

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// Capacidades da swapchain para uma combinação GPU + surface.
struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Dona da surface, da GPU escolhida e do dispositivo lógico.
class Device {
public:
    void create(VkInstance instance, GLFWwindow* window);
    void destroy();

    VkPhysicalDevice physical() const { return m_physical; }
    VkDevice logical() const { return m_device; }
    VkSurfaceKHR surface() const { return m_surface; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    const QueueFamilyIndices& queueFamilies() const { return m_queueFamilies; }

    SwapchainSupport querySwapchainSupport() const { return querySwapchainSupport(m_physical); }

private:
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();

    bool isDeviceSuitable(VkPhysicalDevice candidate) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice candidate) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice candidate) const;
    SwapchainSupport querySwapchainSupport(VkPhysicalDevice candidate) const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilies;
};