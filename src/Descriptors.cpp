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

    // Pool: UBO de câmera por frame + UBO do material; SSBO por frame; 3 texturas; 4 input attachments.
    VkDescriptorPoolSize sizes[4] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight + 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, framesInFlight},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 4;
    poolInfo.pPoolSizes = sizes;
    poolInfo.maxSets = 2 * framesInFlight + 2;  // câmera e luzes por frame, mais material e G-Buffer

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o descriptor pool");
    }

    m_cameraSets.assign(framesInFlight, VK_NULL_HANDLE);
    m_lightSets.assign(framesInFlight, VK_NULL_HANDLE);
    allocate(m_layouts[SET_CAMERA], framesInFlight, m_cameraSets.data());
    allocate(m_layouts[SET_LIGHTS], framesInFlight, m_lightSets.data());
    allocate(m_layouts[SET_MATERIAL], 1, &m_materialSet);
    allocate(m_layouts[SET_GBUFFER], 1, &m_gbufferSet);

    std::printf("Descriptor sets criados: %u de câmera, %u de luzes, material e G-Buffer\n",
        framesInFlight, framesInFlight);
}

void Descriptors::allocate(VkDescriptorSetLayout layout, uint32_t count, VkDescriptorSet* out) {
    std::vector<VkDescriptorSetLayout> layouts(count, layout);

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_pool;
    alloc.descriptorSetCount = count;
    alloc.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_device, &alloc, out) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar descriptor sets");
    }
}

void Descriptors::writeBuffer(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, const Buffer& buf) {
    VkDescriptorBufferInfo info{ buf.handle(), 0, buf.size() };

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void Descriptors::writeCamera(uint32_t frame, const Buffer& ubo) {
    writeBuffer(m_cameraSets[frame], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ubo);
}

void Descriptors::writeLights(uint32_t frame, const Buffer& ssbo) {
    writeBuffer(m_lightSets[frame], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ssbo);
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
        writes[i].dstSet = m_materialSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &images[i];
    }
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_materialSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(m_device, 4, writes, 0, nullptr);
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
        writes[i].dstSet = m_gbufferSet;
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
    m_lightSets.clear();
    m_materialSet = VK_NULL_HANDLE;
    m_gbufferSet = VK_NULL_HANDLE;
}