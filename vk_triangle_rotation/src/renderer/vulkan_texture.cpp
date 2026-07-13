#include "vulkan_texture.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "../../utils/stb_image.h"

// ===========================================================================
// Helper: find memory type
// ===========================================================================
static uint32_t findMemType(VkPhysicalDevice physDev, uint32_t filter,
                             VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(physDev, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    std::printf("ERROR: no suitable memory type for texture\n");
    return 0;
}

// ===========================================================================
// Helper: create buffer
// ===========================================================================
static bool createBuffer(VkDevice dev, VkPhysicalDevice physDev,
                          VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags props,
                          VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(dev, &bi, nullptr, &buf) != VK_SUCCESS)
        return false;

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(dev, buf, &mr);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = findMemType(physDev, mr.memoryTypeBits, props);

    if (vkAllocateMemory(dev, &ai, nullptr, &mem) != VK_SUCCESS)
        return false;

    vkBindBufferMemory(dev, buf, mem, 0);
    return true;
}

// ===========================================================================
// Helper: one-shot command buffer
// ===========================================================================
static VkCommandBuffer beginSingleCmd(VkDevice dev, VkCommandPool pool) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = pool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(dev, &ai, &cmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

static void endSingleCmd(VkDevice dev, VkCommandPool pool,
                          VkQueue queue, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(dev, pool, 1, &cmd);
}

// ===========================================================================
// Helper: transition image layout
// ===========================================================================
static void transitionImageLayout(VkDevice dev, VkCommandPool pool,
                                   VkQueue queue, VkImage image,
                                   VkImageLayout oldLayout,
                                   VkImageLayout newLayout) {
    VkCommandBuffer cmd = beginSingleCmd(dev, pool);

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                          0, nullptr, 0, nullptr, 1, &barrier);

    endSingleCmd(dev, pool, queue, cmd);
}

// ===========================================================================
// Helper: copy buffer to image
// ===========================================================================
static void copyBufferToImage(VkDevice dev, VkCommandPool pool,
                               VkQueue queue, VkBuffer buf,
                               VkImage image, uint32_t w, uint32_t h) {
    VkCommandBuffer cmd = beginSingleCmd(dev, pool);

    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset       = {0, 0, 0};
    region.imageExtent       = {w, h, 1};

    vkCmdCopyBufferToImage(cmd, buf, image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1, &region);

    endSingleCmd(dev, pool, queue, cmd);
}

// ===========================================================================
// Helper: generate checkerboard pattern (fallback texture)
// ===========================================================================
static std::vector<uint8_t> generateCheckerboard(uint32_t w, uint32_t h,
                                                   uint32_t tileSize = 16) {
    std::vector<uint8_t> pixels(w * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            bool white = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            uint32_t idx = (y * w + x) * 4;
            pixels[idx + 0] = white ? 255 : 200;  // R
            pixels[idx + 1] = white ? 200 : 150;  // G
            pixels[idx + 2] = white ? 220 : 170;  // B
            pixels[idx + 3] = 255;                 // A (opaque)
        }
    }
    return pixels;
}

// ===========================================================================
// createTextureFromPixels – buat VkImage dari raw RGBA data
// ===========================================================================
bool createTextureFromPixels(VkDevice device,
                              VkPhysicalDevice physDevice,
                              VkCommandPool cmdPool,
                              VkQueue gfxQueue,
                              const uint8_t* pixels,
                              uint32_t width, uint32_t height,
                              VulkanTexture& outTex,
                              bool keepCpuPixels) {
    outTex.width  = width;
    outTex.height = height;
    if (keepCpuPixels) {
        outTex.cpuPixels.assign(pixels, pixels + (width * height * 4));
    }
    VkDeviceSize imageSize = width * height * 4;

    // --- staging buffer ---
    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    if (!createBuffer(device, physDevice, imageSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       stagingBuf, stagingMem)) {
        std::printf("ERROR: texture staging buffer\n");
        return false;
    }

    void* data;
    vkMapMemory(device, stagingMem, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, imageSize);
    vkUnmapMemory(device, stagingMem);

    // --- create VkImage ---
    VkImageCreateInfo imgInfo{};
    imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.extent        = {width, height, 1};
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imgInfo, nullptr, &outTex.image) != VK_SUCCESS) {
        std::printf("ERROR: vkCreateImage for texture\n");
        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        return false;
    }

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device, outTex.image, &mr);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = findMemType(physDevice, mr.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &ai, nullptr, &outTex.memory) != VK_SUCCESS) {
        std::printf("ERROR: texture image memory\n");
        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        return false;
    }
    vkBindImageMemory(device, outTex.image, outTex.memory, 0);

    // --- transition → TRANSFER_DST, copy, transition → SHADER_READ ---
    transitionImageLayout(device, cmdPool, gfxQueue, outTex.image,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(device, cmdPool, gfxQueue, stagingBuf,
                       outTex.image, width, height);

    transitionImageLayout(device, cmdPool, gfxQueue, outTex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // cleanup staging
    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    // --- image view ---
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = outTex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(device, &viewInfo, nullptr, &outTex.view) != VK_SUCCESS) {
        std::printf("ERROR: texture image view\n");
        return false;
    }

    // --- sampler ---
    VkSamplerCreateInfo sampInfo{};
    sampInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampInfo.magFilter    = VK_FILTER_LINEAR;
    sampInfo.minFilter    = VK_FILTER_LINEAR;
    sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampInfo.anisotropyEnable = VK_FALSE;
    sampInfo.maxAnisotropy    = 1.0f;
    sampInfo.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampInfo.unnormalizedCoordinates = VK_FALSE;
    sampInfo.compareEnable    = VK_FALSE;
    sampInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &sampInfo, nullptr, &outTex.sampler) != VK_SUCCESS) {
        std::printf("ERROR: texture sampler\n");
        return false;
    }

    std::printf("Texture created: %ux%u\n", width, height);
    return true;
}

// ===========================================================================
// loadTextureFromFile – load dari file, fallback ke checkerboard
// ===========================================================================
bool loadTextureFromFile(VkDevice device,
                          VkPhysicalDevice physDevice,
                          VkCommandPool cmdPool,
                          VkQueue gfxQueue,
                          const std::string& path,
                          VulkanTexture& outTex) {
    int w, h, channels;
    uint8_t* pixels = nullptr;

    if (!path.empty()) {
        pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if (pixels) {
            std::printf("Loaded texture: %s (%dx%d, %d channels)\n",
                        path.c_str(), w, h, channels);
        } else {
            std::printf("WARN: Failed to load texture '%s': %s\n",
                        path.c_str(), stbi_failure_reason());
            std::printf("      Using fallback checkerboard pattern\n");
        }
    }

    if (!pixels) {
        // Fallback: checkerboard pattern
        uint32_t fw = 256, fh = 256;
        auto checker = generateCheckerboard(fw, fh);
        bool ok = createTextureFromPixels(device, physDevice, cmdPool, gfxQueue,
                                           checker.data(), fw, fh, outTex, true);
        return ok;
    }

    bool ok = createTextureFromPixels(device, physDevice, cmdPool, gfxQueue,
                                       pixels,
                                       static_cast<uint32_t>(w),
                                       static_cast<uint32_t>(h),
                                       outTex, true);
    stbi_image_free(pixels);
    return ok;
}

// ===========================================================================
// destroyTexture
// ===========================================================================
void destroyTexture(VkDevice device, VulkanTexture& tex) {
    if (tex.sampler) vkDestroySampler(device, tex.sampler, nullptr);
    if (tex.view)    vkDestroyImageView(device, tex.view, nullptr);
    if (tex.image)   vkDestroyImage(device, tex.image, nullptr);
    if (tex.memory)  vkFreeMemory(device, tex.memory, nullptr);
    tex = {};
}

// ===========================================================================
// updateTextureSubresource
// ===========================================================================
bool updateTextureSubresource(VkDevice device,
                              VkPhysicalDevice physDevice,
                              VkCommandPool cmdPool,
                              VkQueue gfxQueue,
                              VulkanTexture& tex,
                              const uint8_t* pixels,
                              uint32_t width, uint32_t height) {
    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    if (!createBuffer(device, physDevice, imageSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       stagingBuf, stagingMem)) {
        return false;
    }

    void* data;
    vkMapMemory(device, stagingMem, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, imageSize);
    vkUnmapMemory(device, stagingMem);

    // Transition SHADER_READ -> TRANSFER_DST
    transitionImageLayout(device, cmdPool, gfxQueue, tex.image,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(device, cmdPool, gfxQueue, stagingBuf,
                       tex.image, width, height);

    // Transition TRANSFER_DST -> SHADER_READ
    transitionImageLayout(device, cmdPool, gfxQueue, tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);
    return true;
}
