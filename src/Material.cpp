#include "Material.hpp"

#include <cstdio>
#include <stdexcept>

namespace {

    // Parâmetros no formato do MaterialUBO do geometry.frag (std140, 32 bytes).
    struct MaterialParams {
        float baseColorFactor[4];
        float specularFactor;
        float shininess;
        float alphaCutoff;
        uint32_t flags;
    };
    static_assert(sizeof(MaterialParams) == 32, "MaterialParams deve ter 32 bytes");

    constexpr uint32_t FLAG_HAS_NORMAL_MAP = 1u;

    ImageRGBA solidPixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        ImageRGBA img;
        img.width = 1;
        img.height = 1;
        img.pixels = { r, g, b, a };
        return img;
    }

    uint32_t findMemoryType(VkPhysicalDevice physical, uint32_t typeBits, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(physical, &mem);

        for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
            bool allowed = (typeBits & (1u << i)) != 0;
            bool hasProps = (mem.memoryTypes[i].propertyFlags & properties) == properties;
            if (allowed && hasProps) return i;
        }
        throw std::runtime_error("Nenhum tipo de memória adequado para o material");
    }

} // namespace

void Material::create(const Device& device, const ModelData& model) {
    m_device = device.logical();

    const ImageRGBA albedo = model.albedo.valid() ? model.albedo : solidPixel(255, 255, 255, 255);
    const bool hasNormal = model.normal.valid();
    const ImageRGBA normal = hasNormal ? model.normal : solidPixel(128, 128, 255, 255);
    const uint8_t specular = 255;

    createTexture(device, m_albedo, VK_FORMAT_R8G8B8A8_SRGB,
        albedo.width, albedo.height, albedo.pixels.data(), albedo.pixels.size());
    createTexture(device, m_normal, VK_FORMAT_R8G8B8A8_UNORM,
        normal.width, normal.height, normal.pixels.data(), normal.pixels.size());
    createTexture(device, m_specular, VK_FORMAT_R8_UNORM, 1, 1, &specular, 1);

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
    for (int i = 0; i < 4; ++i) params.baseColorFactor[i] = model.baseColorFactor[i];
    params.specularFactor = 0.5f;
    params.shininess = 32.0f;
    params.alphaCutoff = 0.5f;
    params.flags = hasNormal ? FLAG_HAS_NORMAL_MAP : 0u;

    m_ubo.create(m_device, device.physical(), sizeof(params), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_ubo.write(&params, sizeof(params));

    std::printf("Material criado: albedo %ux%u, normal %s\n",
        albedo.width, albedo.height, hasNormal ? "do modelo" : "neutra");
}

void Material::createTexture(const Device& device, Texture& tex, VkFormat format,
    uint32_t width, uint32_t height, const uint8_t* pixels, VkDeviceSize size)
{
    VkDevice dev = device.logical();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { width, height, 1 };
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

    // Upload: os pixels passam por um buffer de staging e são copiados para a imagem.
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
    region.imageExtent = { width, height, 1 };
    vkCmdCopyBufferToImage(cmd, staging.handle(), tex.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

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
    vkQueueWaitIdle(device.graphicsQueue());

    vkDestroyCommandPool(dev, pool, nullptr);
    staging.destroy();
}

void Material::destroy() {
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