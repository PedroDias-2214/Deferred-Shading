#include "DefaultMaterial.hpp"

#include <cstdio>
#include <stdexcept>

// Parâmetros do material no formato do MaterialUBO do geometry.frag (std140, 32 bytes).
struct MaterialParams {
    float baseColorFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float specularFactor = 0.5f;
    float shininess = 32.0f;
    float alphaCutoff = 0.5f;
    uint32_t flags = 0;  // sem normal map, sem specular map, sem alpha test
};
static_assert(sizeof(MaterialParams) == 32, "MaterialParams deve ter 32 bytes");

void DefaultMaterial::create(const Device& device) {
    m_device = device.logical();

    // Laranja em sRGB: a GPU converte para linear na leitura.
    const uint8_t albedoPixel[4] = { 230, 120, 40, 255 };
    // Normal apontando para fora da superfície, codificada em [0,1].
    const uint8_t normalPixel[4] = { 128, 128, 255, 255 };
    // Especular no máximo (1 canal).
    const uint8_t specularPixel[1] = { 255 };

    createTexture(device, m_albedo, VK_FORMAT_R8G8B8A8_SRGB, albedoPixel, 4);
    createTexture(device, m_normal, VK_FORMAT_R8G8B8A8_UNORM, normalPixel, 4);
    createTexture(device, m_specular, VK_FORMAT_R8_UNORM, specularPixel, 1);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar o sampler do material");
    }

    MaterialParams params{};
    m_ubo.create(m_device, device.physical(), sizeof(params), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_ubo.write(&params, sizeof(params));

    std::printf("Material padrão criado (3 texturas 1x1)\n");
}

void DefaultMaterial::createTexture(const Device& device, Texture& tex, VkFormat format,
    const void* pixels, VkDeviceSize size) {
    VkDevice dev = device.logical();

    // ---- Imagem 1x1, em memória local da GPU ----
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { 1, 1, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(dev, &imageInfo, nullptr, &tex.image) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar imagem do material");
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(dev, tex.image, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(device.physical(), req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(dev, &alloc, nullptr, &tex.memory) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao alocar memória do material");
    }
    vkBindImageMemory(dev, tex.image, tex.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(dev, &viewInfo, nullptr, &tex.view) != VK_SUCCESS) {
        throw std::runtime_error("Falha ao criar image view do material");
    }

    // ---- Upload: copia os pixels por um buffer de staging ----
    Buffer staging;
    staging.create(dev, device.physical(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    staging.write(pixels, size);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = device.queueFamilies().graphicsFamily.value();

    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(dev, &poolInfo, nullptr, &pool);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = pool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(dev, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    // Transição 1: UNDEFINED -> TRANSFER_DST, para poder receber a cópia.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { 1, 1, 1 };
    vkCmdCopyBufferToImage(cmd, staging.handle(), tex.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transição 2: TRANSFER_DST -> SHADER_READ_ONLY, para o fragment shader amostrar.
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(device.graphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(device.graphicsQueue());  // simples: espera a cópia terminar

    vkDestroyCommandPool(dev, pool, nullptr);
    staging.destroy();
}

uint32_t DefaultMaterial::findMemoryType(VkPhysicalDevice physical, uint32_t typeBits,
    VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(physical, &mem);

    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        bool allowed = (typeBits & (1u << i)) != 0;
        bool hasProps = (mem.memoryTypes[i].propertyFlags & properties) == properties;
        if (allowed && hasProps) return i;
    }
    throw std::runtime_error("Nenhum tipo de memória adequado para o material");
}

void DefaultMaterial::destroy() {
    if (m_device == VK_NULL_HANDLE) return;

    m_ubo.destroy();

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    auto destroyTexture = [this](Texture& t) {
        if (t.view != VK_NULL_HANDLE) vkDestroyImageView(m_device, t.view, nullptr);
        if (t.image != VK_NULL_HANDLE) vkDestroyImage(m_device, t.image, nullptr);
        if (t.memory != VK_NULL_HANDLE) vkFreeMemory(m_device, t.memory, nullptr);
        t = Texture{};
        };
    destroyTexture(m_albedo);
    destroyTexture(m_normal);
    destroyTexture(m_specular);

    m_device = VK_NULL_HANDLE;
}