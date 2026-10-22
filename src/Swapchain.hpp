#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Device.hpp"

struct GLFWwindow;

// Dona da swapchain (as imagens que vão para a tela) e das suas image views.
class Swapchain {
public:
    void create(const Device& device, GLFWwindow* window);
    void destroy();

    VkSwapchainKHR handle() const { return m_swapchain; }
    VkFormat format() const { return m_format; }
    VkExtent2D extent() const { return m_extent; }
    const std::vector<VkImageView>& imageViews() const { return m_imageViews; }

private:
    void createImageViews();

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window) const;

    VkDevice m_device = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
};