#pragma once

#include <vulkan/vulkan.h>
#include <vector>

// Dona da instância Vulkan e do debug messenger.
class Instance {
public:
    void create();
    void destroy();

    VkInstance handle() const { return m_instance; }

private:
    bool checkValidationLayerSupport() const;
    std::vector<const char*> getRequiredExtensions() const;
    VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerInfo() const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
};