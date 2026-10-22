#include "Shaders.hpp"

#include <windows.h>   // GetModuleFileNameA

#include <cstdio>
#include <fstream>
#include <stdexcept>

void Shaders::load(VkDevice device, const std::vector<std::string>& fileNames) {
    m_device = device;
    const std::string dir = spvDirectory();

    for (const std::string& name : fileNames) {
        std::vector<char> code = readFile(dir + "\\" + name);
        m_modules[name] = createShaderModule(code);
        std::printf("Shader carregado: %s (%zu bytes)\n", name.c_str(), code.size());
    }
}

void Shaders::destroy() {
    for (auto& [name, module] : m_modules) {
        vkDestroyShaderModule(m_device, module, nullptr);
    }
    m_modules.clear();
}

VkShaderModule Shaders::get(const std::string& fileName) const {
    auto it = m_modules.find(fileName);
    if (it == m_modules.end()) {
        throw std::runtime_error("Shader não carregado: " + fileName);
    }
    return it->second;
}

// Os .spv ficam numa pasta "spv" ao lado do executável (o CMake faz essa cópia).
// Usamos o caminho do próprio .exe, para funcionar independentemente da pasta de trabalho.
std::string Shaders::spvDirectory() {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    std::string exePath(buffer, len);
    return exePath.substr(0, exePath.find_last_of("\\/")) + "\\spv";
}

std::vector<char> Shaders::readFile(const std::string& path) {
    // ate: já posiciona no fim, para descobrir o tamanho do arquivo.
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Não foi possível abrir o shader: " + path);
    }

    std::vector<char> buffer(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

// Um SPIR-V é uma sequência de palavras de 32 bits, por isso o tamanho precisa ser múltiplo de 4.
VkShaderModule Shaders::createShaderModule(const std::vector<char>& code) const {
    if (code.size() % 4 != 0) {
        throw std::runtime_error("Arquivo SPIR-V inválido (tamanho não múltiplo de 4)");
    }

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_device, &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o shader module");
    }
    return module;
}