#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
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

// Dados de uma malha antes de irem para a GPU.
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    float boundsMin[3] = { 0.0f, 0.0f, 0.0f };
    float boundsMax[3] = { 0.0f, 0.0f, 0.0f };
};

// Dona dos buffers de vértice e de índice da malha.
class Mesh {
public:
    void create(VkDevice device, VkPhysicalDevice physical, const MeshData& data);
    void destroy();

    // Liga os buffers de vértice e de índice ao command buffer.
    void bind(VkCommandBuffer cmd) const;
    uint32_t indexCount() const { return m_indexCount; }

private:
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
    uint32_t m_indexCount = 0;
};