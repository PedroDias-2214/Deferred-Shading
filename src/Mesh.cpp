#include "Mesh.hpp"

void Mesh::create(VkDevice device, VkPhysicalDevice physical, const std::vector<Vertex>& vertices) {
    m_count = static_cast<uint32_t>(vertices.size());
    const VkDeviceSize size = sizeof(Vertex) * vertices.size();

    m_vertexBuffer.create(device, physical, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_vertexBuffer.write(vertices.data(), size);
}

void Mesh::bind(VkCommandBuffer cmd) const {
    VkBuffer buffer = m_vertexBuffer.handle();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
}

void Mesh::destroy() {
    m_vertexBuffer.destroy();
    m_count = 0;
}