#include "Mesh.hpp"

#include <stdexcept>

void Mesh::create(VkDevice device, VkPhysicalDevice physical, const MeshData& data) {
    if (data.vertices.empty() || data.indices.empty()) {
        throw std::runtime_error("Malha vazia");
    }

    m_indexCount = static_cast<uint32_t>(data.indices.size());

    const VkDeviceSize vSize = sizeof(Vertex) * data.vertices.size();
    m_vertexBuffer.create(device, physical, vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_vertexBuffer.write(data.vertices.data(), vSize);

    const VkDeviceSize iSize = sizeof(uint32_t) * data.indices.size();
    m_indexBuffer.create(device, physical, iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    m_indexBuffer.write(data.indices.data(), iSize);
}

void Mesh::bind(VkCommandBuffer cmd) const {
    VkBuffer vb = m_vertexBuffer.handle();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer.handle(), 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::destroy() {
    m_indexBuffer.destroy();
    m_vertexBuffer.destroy();
    m_indexCount = 0;
}