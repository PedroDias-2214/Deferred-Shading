#include "Descriptors.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

    // Cria uma entrada de binding com uma só descrição.
    VkDescriptorSetLayoutBinding binding(uint32_t index, VkDescriptorType type, VkShaderStageFlags stages) {
        VkDescriptorSetLayoutBinding b{};
        b.binding = index;
        b.descriptorType = type;
        b.descriptorCount = 1;
        b.stageFlags = stages;
        return b;
    }

    VkDescriptorSetLayout makeLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<uint32_t>(bindings.size());
        info.pBindings = bindings.data();

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("Falha ao criar descriptor set layout");
        }
        return layout;
    }

} // namespace

void Descriptors::create(VkDevice device) {
    m_device = device;

    const VkShaderStageFlags vertFrag = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkShaderStageFlags frag = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Set 0: câmera.
    m_layouts[SET_CAMERA] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vertFrag),
        });

    // Set 1: material (geometria). Ainda não usado pelo pipeline de iluminação.
    m_layouts[SET_MATERIAL] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag),  // albedo
        binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag),  // normal
        binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag),  // especular
        binding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frag),          // parâmetros do material
        });

    // Set 2: luzes.
    m_layouts[SET_LIGHTS] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frag),
        });

    // Set 3: G-Buffer como input attachments (gPosition, gNormal, gAlbedoSpec, gDepth).
    m_layouts[SET_GBUFFER] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        binding(1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        binding(2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        binding(3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        });

    // Pool com espaço para os quatro sets (2 UBOs, 3 texturas, 1 SSBO, 4 input attachments).
    std::array<VkDescriptorPoolSize, 4> sizes = { {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4},
    } };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes = sizes.data();
    poolInfo.maxSets = SET_COUNT;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o descriptor pool");
    }

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_pool;
    alloc.descriptorSetCount = SET_COUNT;
    alloc.pSetLayouts = m_layouts.data();

    if (vkAllocateDescriptorSets(m_device, &alloc, m_sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar descriptor sets");
    }

    std::printf("Descriptor sets criados (%u sets)\n", static_cast<unsigned>(SET_COUNT));
}

void Descriptors::writeCamera(const Buffer& ubo) {
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = ubo.handle();
    bufInfo.offset = 0;
    bufInfo.range = ubo.size();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_sets[SET_CAMERA];
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void Descriptors::writeLights(const Buffer& ssbo) {
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = ssbo.handle();
    bufInfo.offset = 0;
    bufInfo.range = ssbo.size();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_sets[SET_LIGHTS];
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void Descriptors::writeGBuffer(const GBuffer& g) {
    // Os layouts precisam ser os mesmos usados no render pass durante o subpass 1.
    VkDescriptorImageInfo infos[4] = {
        {VK_NULL_HANDLE, g.positionView(),   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, g.normalView(),     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, g.albedoSpecView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, g.depthView(),      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
    };

    VkWriteDescriptorSet writes[4]{};
    for (uint32_t i = 0; i < 4; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_sets[SET_GBUFFER];
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[i].pImageInfo = &infos[i];
    }

    vkUpdateDescriptorSets(m_device, 4, writes, 0, nullptr);
}

void Descriptors::destroy() {
    // Liberar o pool libera todos os sets alocados nele.
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
    for (VkDescriptorSetLayout& layout : m_layouts) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
}