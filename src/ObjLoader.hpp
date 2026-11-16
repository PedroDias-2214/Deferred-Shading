#pragma once

#include <string>
#include "Mesh.hpp"

// Lê um arquivo .obj e devolve vértices e índices prontos para o Mesh.
class ObjLoader {
public:
    static MeshData load(const std::string& path);
};