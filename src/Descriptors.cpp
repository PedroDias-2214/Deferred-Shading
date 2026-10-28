#include "Descriptors.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

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

void Descriptors::create(VkDevice device, uint32_t framesInFlight) {
    m_device = device;

    const VkShaderStageFlags vertFrag = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkShaderStageFlags frag = VK_SHADER_STAGE_FRAGMENT_BIT;

    m_layouts[SET_CAMERA] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vertFrag),
        });
    m_layouts[SET_MATERIAL] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag),
        binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag),
        binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frag),
        binding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frag),
        });
    m_layouts[SET_LIGHTS] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frag),
        });
    m_layouts[SET_GBUFFER] = makeLayout(m_device, {
        binding(0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        binding(1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        binding(2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        binding(3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frag),
        });

    // Pool: um UBO por frame para a câmera, mais o UBO do material.
    std::array<VkDescriptorPoolSize, 4> sizes = { {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight + 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4},
    } };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes = sizes.data();
    poolInfo.maxSets = framesInFlight + 3;  // sets de câmera + material, luzes e G-Buffer

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o descriptor pool");
    }

    // Sets de câmera: um por frame em voo, todos com o mesmo layout.
    std::vector<VkDescriptorSetLayout> cameraLayouts(framesInFlight, m_layouts[SET_CAMERA]);
    m_cameraSets.assign(framesInFlight, VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo camAlloc{};
    camAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    camAlloc.descriptorPool = m_pool;
    camAlloc.descriptorSetCount = framesInFlight;
    camAlloc.pSetLayouts = cameraLayouts.data();

    if (vkAllocateDescriptorSets(m_device, &camAlloc, m_cameraSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar descriptor sets de câmera");
    }

    // Sets compartilhados: material, luzes e G-Buffer (índices 1 a 3, em sequência).
    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_pool;
    alloc.descriptorSetCount = SET_COUNT - 1;
    alloc.pSetLayouts = &m_layouts[SET_MATERIAL];

    if (vkAllocateDescriptorSets(m_device, &alloc, &m_sets[SET_MATERIAL]) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar descriptor sets");
    }

    std::printf("Descriptor sets criados: %u de câmera, 3 compartilhados\n", framesInFlight);
}

void Descriptors::writeCamera(uint32_t frame, const Buffer& ubo) {
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = ubo.handle();
    bufInfo.offset = 0;
    bufInfo.range = ubo.size();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_cameraSets[frame];
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void Descriptors::writeMaterial(VkImageView albedo, VkImageView normal, VkImageView specular,
    VkSampler sampler, const Buffer& ubo) {
    VkDescriptorImageInfo images[3] = {
        {sampler, albedo,   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {sampler, normal,   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {sampler, specular, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    };
    VkDescriptorBufferInfo bufInfo{ ubo.handle(), 0, ubo.size() };

    VkWriteDescriptorSet writes[4]{};
    for (uint32_t i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_sets[SET_MATERIAL];
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &images[i];
    }
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_sets[SET_MATERIAL];
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(m_device, 4, writes, 0, nullptr);
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
    m_cameraSets.clear();
}