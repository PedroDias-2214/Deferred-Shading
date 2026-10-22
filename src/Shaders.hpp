#pragma once

#include <vulkan/vulkan.h>
#include <map>
#include <string>
#include <vector>

// Carrega os arquivos .spv gerados pelo CMake e cria os shader modules Vulkan.
class Shaders {
public:
    void load(VkDevice device, const std::vector<std::string>& fileNames);
    void destroy();

    // Retorna o módulo pelo nome do arquivo, por exemplo "geometry.vert.spv".
    VkShaderModule get(const std::string& fileName) const;

private:
    static std::string spvDirectory();
    static std::vector<char> readFile(const std::string& path);
    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    VkDevice m_device = VK_NULL_HANDLE;
    std::map<std::string, VkShaderModule> m_modules;
};