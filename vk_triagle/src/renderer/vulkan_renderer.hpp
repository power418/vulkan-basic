#pragma once
#include "renderer.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <string>
#include <vector>

class VulkanRenderer : public Renderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() override { cleanup(); }

    bool init(Window* window, const Args& args) override;
    bool draw() override;
    void cleanup() override;

private:
    Window* m_window          = nullptr;
    bool    m_validation      = false;
    std::string m_vertPath, m_fragPath;

    // -- Vulkan handles ------------------------------------------------------
    VkInstance       m_instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDev        = VK_NULL_HANDLE;
    VkDevice         m_dev            = VK_NULL_HANDLE;
    VkQueue          m_gfxQueue       = VK_NULL_HANDLE;
    VkQueue          m_presQueue      = VK_NULL_HANDLE;
    VkSwapchainKHR   m_swapchain      = VK_NULL_HANDLE;
    VkFormat         m_swapFmt        = VK_FORMAT_UNDEFINED;
    VkExtent2D       m_swapExtent     = {};
    VkRenderPass     m_renderPass     = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout     = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
    VkCommandPool    m_cmdPool        = VK_NULL_HANDLE;
    VkBuffer         m_vertBuf        = VK_NULL_HANDLE;
    VkDeviceMemory   m_vertMem        = VK_NULL_HANDLE;

    uint32_t m_gfxFamily = 0;
    uint32_t m_presFamily = 0;

    std::vector<VkImage>         m_swapImages;
    std::vector<VkImageView>     m_swapViews;
    std::vector<VkFramebuffer>   m_framebufs;
    std::vector<VkCommandBuffer> m_cmdBufs;

    // Per-swapchain-image sync objects (avoid VUID-vkQueueSubmit-pSignalSemaphores-00067)
    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_semImage{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_semRender{};
    std::array<VkFence,     MAX_FRAMES_IN_FLIGHT> m_fences{};
    uint32_t m_currentFrame = 0;

    // -- helpers -------------------------------------------------------------
    bool createInstance(const std::vector<const char*>& extensions);
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapchain();
    void destroySwapchain();               // tear down + rebuild
    bool recreateSwapchain();
    bool createRenderPass();
    bool createPipeline();
    bool createFramebuffers();
    bool createVertexBuffer();
    bool createCommandBuffers();
    bool createSyncObjects();

    uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags props);
};
