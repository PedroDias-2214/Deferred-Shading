#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Mesh.hpp"

// Imagem RGBA de 8 bits por canal, do topo para a base (mesma convenção das texturas).
struct ImageRGBA {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels;

    bool valid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

// Tudo o que o programa usa de um arquivo .glb: geometria pronta no espaço do mundo
// e as texturas do material principal.
struct ModelData {
    MeshData mesh;
    ImageRGBA albedo;
    ImageRGBA normal;
    float baseColorFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};

class GltfLoader {
public:
    static ModelData load(const std::string& path);
};