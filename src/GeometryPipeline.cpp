#include "GeometryPipeline.hpp"

#include <cstddef>
#include <cstdio>
#include <stdexcept>

void GeometryPipeline::create(VkDevice device, VkRenderPass renderPass, uint32_t subpass,
    const Shaders& shaders, const Descriptors& descriptors)
{
    m_device = device;

    // ---- Layout: os quatro sets e a push constant do vertex shader ----
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(ObjectPushConstants);

    const auto& layouts = descriptors.layouts();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    layoutInfo.pSetLayouts = layouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;

    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o pipeline layout da geometria");
    }

    // ---- Estágios: vertex e fragment ----
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = shaders.get("geometry.vert.spv");
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = shaders.get("geometry.frag.spv");
    stages[1].pName = "main";

    // ---- Vertex input: um buffer de vértices de 48 bytes, com quatro atributos ----
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[4]{};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    static_cast<uint32_t>(offsetof(Vertex, position)) };
    attrs[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT,    static_cast<uint32_t>(offsetof(Vertex, normal)) };
    attrs[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT,       static_cast<uint32_t>(offsetof(Vertex, uv)) };
    attrs[3] = { 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, tangent)) };

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;  // sem descarte de faces: é o comportamento de dupla face
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Teste de profundidade: o fragmento mais próximo da câmera vence e escreve no depth.
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;

    // Três attachments de cor do G-Buffer, na ordem do subpass: posição, normal, albedo/especular.
    VkPipelineColorBlendAttachmentState blends[3]{};
    for (auto& b : blends) {
        b.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        b.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 3;
    blend.pAttachments = blends;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = m_layout;
    info.renderPass = renderPass;
    info.subpass = subpass;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o pipeline de geometria");
    }

    std::printf("Pipeline de geometria criado (subpass %u)\n", subpass);
}

void GeometryPipeline::destroy() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
}