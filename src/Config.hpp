#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// Tamanho inicial da janela.
inline constexpr uint32_t WIDTH = 1280;
inline constexpr uint32_t HEIGHT = 720;

// Validação só em Debug. Em Release (NDEBUG definido) fica desligada.
#ifdef NDEBUG
inline constexpr bool enableValidation = false;
#else
inline constexpr bool enableValidation = true;
#endif

inline const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

// Extensão de dispositivo exigida: a swapchain permite apresentar imagens na tela.
inline const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };