#pragma once
#include "renderer.hpp"
#include "vulkan_texture.hpp"
#include <array>
#include <string>
#include <vector>
#include <imgui.h>
#include <vulkan/vulkan.h>
#include "../utils/math3d.hpp"

class VulkanRenderer : public Renderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() override { cleanup(); }

    bool init(Window* window, const Args& args) override;
    bool draw() override;
    void cleanup() override;
    
    float getAngle() const override { return m_angle; }

    bool initImGui();
    void cleanupImGui();

    void handleMouseClick(int x, int y) override;

    void changeTexture(const std::string& texturePath);

private:
    void updateScoreTexture();
    Window* m_window          = nullptr;
    bool    m_validation      = false;
    float   m_rotationSpeed   = 1.0f;
    float   m_angle           = 0.0f;
    int     m_score           = 0;
    int     m_polygonSides    = 4;     // piramida default = 4 sisi alas
    uint32_t m_vertexCount    = 18;    // jumlah vertex aktif
    
    struct ImFont* m_scoreFont = nullptr;

    std::string m_vertPath, m_fragPath;
    std::string m_texturePath;

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

    // -- Vertex buffer -------------------------------------------------------
    VkBuffer         m_vertBuf        = VK_NULL_HANDLE;
    VkDeviceMemory   m_vertMem        = VK_NULL_HANDLE;

    // -- Uniform buffer (MVP) ------------------------------------------------
    VkBuffer         m_ubo            = VK_NULL_HANDLE;
    VkDeviceMemory   m_uboMem         = VK_NULL_HANDLE;

    // -- Depth buffer --------------------------------------------------------
    VkImage          m_depthImage     = VK_NULL_HANDLE;
    VkDeviceMemory   m_depthImageMem  = VK_NULL_HANDLE;
    VkImageView      m_depthImageView = VK_NULL_HANDLE;
    VkFormat         m_depthFormat    = VK_FORMAT_UNDEFINED;

    // -- Descriptor ----------------------------------------------------------
    VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descPool   = VK_NULL_HANDLE;
    VkDescriptorSet       m_descSet    = VK_NULL_HANDLE;
    VkDescriptorPool      m_imguiPool  = VK_NULL_HANDLE;

    // -- Texture -------------------------------------------------------------
    VulkanTexture         m_texture;

    uint32_t m_gfxFamily = 0;
    uint32_t m_presFamily = 0;

    std::vector<VkImage>         m_swapImages;
    std::vector<VkImageView>     m_swapViews;
    std::vector<VkFramebuffer>   m_framebufs;
    std::vector<VkCommandBuffer> m_cmdBufs;

    // Per-swapchain-image sync objects
    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_semImage{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_semRender{};
    std::array<VkFence,     MAX_FRAMES_IN_FLIGHT> m_fences{};
    uint32_t m_currentFrame = 0;

    // -- Animasi & Camera ----------------------------------------------------
    float m_rotX = 0.0f; // Pitch (rotasi sumbu X)
    float m_rotY = 0.0f; // Yaw (rotasi sumbu Y)
    float m_cameraDistance = 3.5f; // Jarak kamera dari objek (Z-axis)

    // -- helpers -------------------------------------------------------------
    bool createInstance(const std::vector<const char*>& extensions);
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapchain();
    void destroySwapchain();
    bool recreateSwapchain();
    bool createRenderPass();
    bool createDescriptorSetLayout();
    bool createPipeline();
    bool createFramebuffers();
    bool createVertexBuffer();
    bool createUniformBuffer();
    bool createDescriptorPool();
    bool createDescriptorSet();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createTextureImage();
    bool createDepthResources();
    bool rebuildVertexBuffer(int sides);

    VkFormat findDepthFormat();
    uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags props);
};
