#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "Buffer.hpp"

// Vértice no formato esperado pelo geometry.vert: posição, normal, UV e tangente
// (xyz + sinal da bitangente em w). 48 bytes no total.
struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
    float tangent[4];
};
static_assert(sizeof(Vertex) == 48, "Vertex deve ter 48 bytes");

// Dona de um vertex buffer. Por enquanto fica em memória visível pela CPU, por simplicidade.
class Mesh {
public:
    void create(VkDevice device, VkPhysicalDevice physical, const std::vector<Vertex>& vertices);
    void destroy();

    // Liga o vertex buffer ao command buffer.
    void bind(VkCommandBuffer cmd) const;
    uint32_t vertexCount() const { return m_count; }

private:
    Buffer m_vertexBuffer;
    uint32_t m_count = 0;
};