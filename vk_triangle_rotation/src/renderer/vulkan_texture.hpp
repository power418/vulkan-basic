#pragma once
/*
 * vulkan_texture.hpp – Texture loading & Vulkan image management
 * Load gambar dari file (PNG/JPG/BMP) dan buat Vulkan image + sampler
 */
#include <vulkan/vulkan.h>
#include <string>
#include <cstdint>

struct VulkanTexture {
    VkImage        image      = VK_NULL_HANDLE;
    VkDeviceMemory memory     = VK_NULL_HANDLE;
    VkImageView    view       = VK_NULL_HANDLE;
    VkSampler      sampler    = VK_NULL_HANDLE;
    uint32_t       width      = 0;
    uint32_t       height     = 0;
};

// Load texture dari file gambar (PNG, JPG, BMP, TGA)
// Jika path kosong atau gagal, buat checkerboard pattern sebagai fallback
bool loadTextureFromFile(VkDevice device,
                         VkPhysicalDevice physDevice,
                         VkCommandPool cmdPool,
                         VkQueue gfxQueue,
                         const std::string& path,
                         VulkanTexture& outTex);

// Buat texture dari raw RGBA pixel data
bool createTextureFromPixels(VkDevice device,
                             VkPhysicalDevice physDevice,
                             VkCommandPool cmdPool,
                             VkQueue gfxQueue,
                             const uint8_t* pixels,
                             uint32_t width, uint32_t height,
                             VulkanTexture& outTex);

// Bersihkan semua resource texture
void destroyTexture(VkDevice device, VulkanTexture& tex);
