#include "Swapchain.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <stdexcept>

void Swapchain::create(const Device& device, GLFWwindow* window) {
    m_device = device.logical();

    SwapchainSupport support = device.querySwapchainSupport();
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    m_extent = chooseExtent(support.capabilities, window);
    m_format = surfaceFormat.format;

    // Pedimos uma imagem a mais que o mínimo, para não esperar a GPU sem necessidade.
    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = device.surface();
    info.minImageCount = imageCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent = m_extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Se desenho e apresentação estiverem em famílias diferentes, a imagem
    // precisa ser compartilhada entre elas.
    const QueueFamilyIndices& q = device.queueFamilies();
    uint32_t families[] = { q.graphicsFamily.value(), q.presentFamily.value() };
    if (q.graphicsFamily.value() != q.presentFamily.value()) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = families;
    }
    else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar a swapchain");
    }

    // A GPU pode criar mais imagens do que pedimos; sempre consultamos a lista final.
    // Primeira chamada: descobre quantas imagens a swapchain tem.
    uint32_t count = 0;
    VkResult result = vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Falha ao consultar a quantidade de imagens da swapchain");
    }
    std::printf("[debug] imagens na swapchain: %u\n", count);

    // Segunda chamada: preenche um vetor local e só depois copia para o membro.
    std::vector<VkImage> images(count);
    result = vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, images.data());
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Falha ao obter as imagens da swapchain");
    }
    m_images = images;

    createImageViews();

    std::printf("Swapchain: %ux%u, %u imagens, modo %s, sRGB: %s\n",
        m_extent.width, m_extent.height, count,
        presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO",
        m_format == VK_FORMAT_B8G8R8A8_SRGB ? "sim" : "não");
}

void Swapchain::createImageViews() {
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = m_images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = m_format;
        info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &info, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar image view da swapchain");
        }
    }
}

void Swapchain::destroy() {
    // Ordem inversa: primeiro as image views, depois a swapchain.
    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(m_device, view, nullptr);
    }
    m_imageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_images.clear();
}

// Prefere sRGB, que faz a conversão de cor automaticamente na escrita.
VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

// MAILBOX é o mais responsivo (troca a imagem sem tearing). FIFO é o padrão
// obrigatório do Vulkan (vsync), usado como fallback.
VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    for (VkPresentModeKHR m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

// Se a surface define um tamanho fixo, usamos ele. Caso contrário, usamos o
// tamanho da janela em pixels, limitado pelos valores aceitos pela GPU.
VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window) const {
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }

    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);

    VkExtent2D extent{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}