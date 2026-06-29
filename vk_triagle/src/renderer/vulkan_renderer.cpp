#include "vulkan_renderer.hpp"
#include "../args.hpp"
#include "../shader_compiler.hpp"
#include "../platform/window.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <set>

// ===========================================================================
// init
// ===========================================================================
bool VulkanRenderer::init(Window* window, const Args& args) {
    m_window     = window;
    m_validation = args.validation;
    m_vertPath   = args.vertShader;
    m_fragPath   = args.fragShader;

    auto exts = window->getRequiredInstanceExtensions();

    return createInstance(exts)
        && (window->createSurface(m_instance) != VK_NULL_HANDLE)
        && pickPhysicalDevice()
        && createDevice()
        && createSwapchain()
        && createRenderPass()
        && createPipeline()
        && createFramebuffers()
        && createVertexBuffer()
        && createCommandBuffers()
        && createSyncObjects();
}

// ===========================================================================
// Instance
// ===========================================================================
bool VulkanRenderer::createInstance(const std::vector<const char*>& exts) {
    VkApplicationInfo app{};
    app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName   = "Vulkan Triangle";
    app.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app.pEngineName        = "No Engine";
    app.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app.apiVersion         = VK_API_VERSION_1_3;

    const char* layers[1]{};
    uint32_t layerCount = 0;
    if (m_validation) {
        layers[0] = "VK_LAYER_KHRONOS_validation";
        layerCount = 1;
    }

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &app;
    ci.enabledLayerCount       = layerCount;
    ci.ppEnabledLayerNames     = layerCount ? layers : nullptr;
    ci.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();

    VkResult res = vkCreateInstance(&ci, nullptr, &m_instance);
    if (res != VK_SUCCESS) {
        std::printf("ERROR: vkCreateInstance failed (%d)\n", res);
        return false;
    }
    std::printf("Vulkan instance created\n");
    return true;
}

// ===========================================================================
// Physical device
// ===========================================================================
bool VulkanRenderer::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (!count) {
        std::printf("ERROR: no Vulkan GPU\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
    VkSurfaceKHR surface = m_window->getSurface();

    for (auto d : devices) {
        uint32_t qc = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, nullptr);
        std::vector<VkQueueFamilyProperties> queues(qc);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, queues.data());

        int32_t gf = -1, pf = -1;
        for (uint32_t i = 0; i < qc; ++i) {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                gf = static_cast<int32_t>(i);
            VkBool32 support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface, &support);
            if (support) pf = static_cast<int32_t>(i);
        }

        uint32_t fc = 0, mc = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(d, surface, &fc, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(d, surface, &mc, nullptr);

        if (gf >= 0 && pf >= 0 && fc > 0 && mc > 0) {
            m_physDev   = d;
            m_gfxFamily = static_cast<uint32_t>(gf);
            m_presFamily = static_cast<uint32_t>(pf);
            break;
        }
    }

    if (!m_physDev) {
        std::printf("ERROR: no suitable GPU\n");
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physDev, &props);
    std::printf("GPU: %s\n", props.deviceName);
    return true;
}

// ===========================================================================
// Logical device
// ===========================================================================
bool VulkanRenderer::createDevice() {
    float priority = 1.0f;
    std::set<uint32_t> families{m_gfxFamily, m_presFamily};

    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (auto f : families) {
        qcis.push_back({VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        nullptr, 0, f, 1, &priority});
    }

    const char* devExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = static_cast<uint32_t>(qcis.size());
    ci.pQueueCreateInfos       = qcis.data();
    ci.enabledExtensionCount   = 1;
    ci.ppEnabledExtensionNames = devExts;
    ci.pEnabledFeatures        = &features;

    if (vkCreateDevice(m_physDev, &ci, nullptr, &m_dev) != VK_SUCCESS) {
        std::printf("ERROR: vkCreateDevice failed\n");
        return false;
    }

    vkGetDeviceQueue(m_dev, m_gfxFamily, 0, &m_gfxQueue);
    vkGetDeviceQueue(m_dev, m_presFamily, 0, &m_presQueue);
    return true;
}

// ===========================================================================
// Swapchain
// ===========================================================================
bool VulkanRenderer::createSwapchain() {
    VkSurfaceKHR surface = m_window->getSurface();

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physDev, surface, &caps);

    // --- format ---
    uint32_t fc = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDev, surface, &fc, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fc);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDev, surface, &fc, formats.data());

    VkSurfaceFormatKHR fmt = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            fmt = f;
            break;
        }
    }
    m_swapFmt = fmt.format;

    // --- present mode ---
    uint32_t mc = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDev, surface, &mc, nullptr);
    std::vector<VkPresentModeKHR> modes(mc);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDev, surface, &mc, modes.data());

    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { mode = m; break; }
    }

    // --- extent ---
    if (caps.currentExtent.width != UINT32_MAX) {
        m_swapExtent = caps.currentExtent;
    } else {
        int w, h;
        m_window->getFramebufferSize(w, h);
        m_swapExtent = {
            std::clamp(static_cast<uint32_t>(w),
                       caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(static_cast<uint32_t>(h),
                       caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    // If the window is minimized the extent can be zero — bail early
    if (m_swapExtent.width == 0 || m_swapExtent.height == 0) {
        std::printf("WARN: swapchain extent is 0 (window minimised)\n");
        return false;
    }

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount && imgCount > caps.maxImageCount)
        imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surface;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = m_swapFmt;
    ci.imageColorSpace  = fmt.colorSpace;
    ci.imageExtent      = m_swapExtent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = mode;
    ci.clipped          = VK_TRUE;

    if (vkCreateSwapchainKHR(m_dev, &ci, nullptr, &m_swapchain) != VK_SUCCESS) {
        std::printf("ERROR: vkCreateSwapchainKHR failed\n");
        return false;
    }

    vkGetSwapchainImagesKHR(m_dev, m_swapchain, &imgCount, nullptr);
    m_swapImages.resize(imgCount);
    vkGetSwapchainImagesKHR(m_dev, m_swapchain, &imgCount, m_swapImages.data());

    m_swapViews.resize(imgCount);
    for (size_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image    = m_swapImages[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format   = m_swapFmt;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(m_dev, &vi, nullptr, &m_swapViews[i]);
    }

    std::printf("Swapchain: %zu images, %dx%d\n",
                m_swapImages.size(), m_swapExtent.width, m_swapExtent.height);
    return true;
}

// ===========================================================================
// Render pass
// ===========================================================================
bool VulkanRenderer::createRenderPass() {
    VkAttachmentDescription att{};
    att.format         = m_swapFmt;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sp{};
    sp.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments    = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &att;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sp;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;

    if (vkCreateRenderPass(m_dev, &ci, nullptr, &m_renderPass) != VK_SUCCESS) {
        std::printf("ERROR: vkCreateRenderPass failed\n");
        return false;
    }
    return true;
}

// ===========================================================================
// Graphics pipeline
// ===========================================================================
bool VulkanRenderer::createPipeline() {
    auto vertCode = loadSPIRV(m_vertPath);
    auto fragCode = loadSPIRV(m_fragPath);
    if (vertCode.empty() || fragCode.empty()) {
        std::printf("ERROR: shaders not found -- run compile.bat first\n");
        return false;
    }

    VkShaderModule vertMod, fragMod;
    auto makeMod = [&](const auto& code, VkShaderModule* mod) {
        VkShaderModuleCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size() * sizeof(uint32_t);
        ci.pCode    = code.data();
        return vkCreateShaderModule(m_dev, &ci, nullptr, mod);
    };

    if (makeMod(vertCode, &vertMod) != VK_SUCCESS ||
        makeMod(fragCode, &fragMod) != VK_SUCCESS) {
        std::printf("ERROR: vkCreateShaderModule failed\n");
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "vs_main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "ps_main";

    VkVertexInputBindingDescription bind{};
    bind.binding   = 0;
    bind.stride    = 5 * sizeof(float);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr[2]{};
    attr[0].location = 0; attr[0].binding = 0;
    attr[0].format   = VK_FORMAT_R32G32_SFLOAT;
    attr[0].offset   = 0;
    attr[1].location = 1; attr[1].binding = 0;
    attr[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attr[1].offset   = 2 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &bind;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions    = attr;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{};
    vp.width  = static_cast<float>(m_swapExtent.width);
    vp.height = static_cast<float>(m_swapExtent.height);
    vp.maxDepth = 1.0f;

    VkRect2D sc{};
    sc.extent = m_swapExtent;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.pViewports    = &vp;
    vs.scissorCount  = 1;
    vs.pScissors     = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_BACK_BIT;
    rs.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAtt{};
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAtt;

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(m_dev, &pl, nullptr, &m_pipeLayout) != VK_SUCCESS) {
        std::printf("ERROR: vkCreatePipelineLayout failed\n");
        return false;
    }

    VkGraphicsPipelineCreateInfo gi{};
    gi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gi.stageCount          = 2;
    gi.pStages             = stages;
    gi.pVertexInputState   = &vi;
    gi.pInputAssemblyState = &ia;
    gi.pViewportState      = &vs;
    gi.pRasterizationState = &rs;
    gi.pMultisampleState   = &ms;
    gi.pColorBlendState    = &cb;
    gi.layout              = m_pipeLayout;
    gi.renderPass          = m_renderPass;
    gi.subpass             = 0;

    VkResult res = vkCreateGraphicsPipelines(m_dev, VK_NULL_HANDLE,
                                             1, &gi, nullptr, &m_pipeline);

    vkDestroyShaderModule(m_dev, vertMod, nullptr);
    vkDestroyShaderModule(m_dev, fragMod, nullptr);

    if (res != VK_SUCCESS) {
        std::printf("ERROR: vkCreateGraphicsPipelines failed\n");
        return false;
    }
    std::printf("Graphics pipeline created\n");
    return true;
}

// ===========================================================================
// Framebuffers
// ===========================================================================
bool VulkanRenderer::createFramebuffers() {
    m_framebufs.resize(m_swapViews.size());
    for (size_t i = 0; i < m_swapViews.size(); ++i) {
        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = m_renderPass;
        ci.attachmentCount = 1;
        ci.pAttachments    = &m_swapViews[i];
        ci.width           = m_swapExtent.width;
        ci.height          = m_swapExtent.height;
        ci.layers          = 1;
        if (vkCreateFramebuffer(m_dev, &ci, nullptr, &m_framebufs[i]) != VK_SUCCESS) {
            std::printf("ERROR: framebuffer %zu\n", i);
            return false;
        }
    }
    return true;
}

// ===========================================================================
// Vertex buffer
// ===========================================================================
bool VulkanRenderer::createVertexBuffer() {
    struct Vertex { float pos[2]; float color[3]; };
    const Vertex verts[] = {
        {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    };
    VkDeviceSize size = sizeof(verts);

    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size  = size;
    bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_dev, &bi, nullptr, &m_vertBuf) != VK_SUCCESS) {
        std::printf("ERROR: vertex buffer\n");
        return false;
    }

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(m_dev, m_vertBuf, &mr);

    VkMemoryAllocateInfo ai{};
    ai.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemoryType(
        mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_dev, &ai, nullptr, &m_vertMem) != VK_SUCCESS) {
        std::printf("ERROR: vertex memory\n");
        return false;
    }
    vkBindBufferMemory(m_dev, m_vertBuf, m_vertMem, 0);

    void* data;
    vkMapMemory(m_dev, m_vertMem, 0, size, 0, &data);
    std::memcpy(data, verts, size);
    vkUnmapMemory(m_dev, m_vertMem);
    return true;
}

uint32_t VulkanRenderer::findMemoryType(uint32_t filter,
                                         VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(m_physDev, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    std::printf("ERROR: no suitable memory type\n");
    return 0;
}

// ===========================================================================
// Command pool & buffers
// ===========================================================================
bool VulkanRenderer::createCommandBuffers() {
    VkCommandPoolCreateInfo pci{};
    pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = m_gfxFamily;

    if (vkCreateCommandPool(m_dev, &pci, nullptr, &m_cmdPool) != VK_SUCCESS) {
        std::printf("ERROR: command pool\n");
        return false;
    }

    m_cmdBufs.resize(m_framebufs.size());
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = m_cmdPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = static_cast<uint32_t>(m_cmdBufs.size());

    if (vkAllocateCommandBuffers(m_dev, &ai, m_cmdBufs.data()) != VK_SUCCESS) {
        std::printf("ERROR: allocate command buffers\n");
        return false;
    }

    for (size_t i = 0; i < m_cmdBufs.size(); ++i) {
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        vkBeginCommandBuffer(m_cmdBufs[i], &bi);

        VkClearValue clear{{{0.0f, 0.0f, 0.0f, 1.0f}}};

        VkRenderPassBeginInfo rp{};
        rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass  = m_renderPass;
        rp.framebuffer = m_framebufs[i];
        rp.renderArea  = {{0, 0}, m_swapExtent};
        rp.clearValueCount = 1;
        rp.pClearValues    = &clear;
        vkCmdBeginRenderPass(m_cmdBufs[i], &rp, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(m_cmdBufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline);

        VkBuffer     vb[] = {m_vertBuf};
        VkDeviceSize off[] = {0};
        vkCmdBindVertexBuffers(m_cmdBufs[i], 0, 1, vb, off);
        vkCmdDraw(m_cmdBufs[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(m_cmdBufs[i]);

        if (vkEndCommandBuffer(m_cmdBufs[i]) != VK_SUCCESS) {
            std::printf("ERROR: record cmd buf %zu\n", i);
            return false;
        }
    }
    return true;
}

// ===========================================================================
// Sync objects  – one pair per swapchain image to avoid semaphore reuse
// ===========================================================================
bool VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_dev, &si, nullptr, &m_semImage[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_dev, &si, nullptr, &m_semRender[i]) != VK_SUCCESS ||
            vkCreateFence(m_dev, &fi, nullptr, &m_fences[i]) != VK_SUCCESS) {
            std::printf("ERROR: sync object %d\n", i);
            return false;
        }
    }
    m_currentFrame = 0;
    return true;
}

// ===========================================================================
// destroySwapchain  – tear down swapchain-dependent objects
// ===========================================================================
void VulkanRenderer::destroySwapchain() {
    vkDeviceWaitIdle(m_dev);

    if (m_cmdPool) {
        vkFreeCommandBuffers(m_dev, m_cmdPool,
            static_cast<uint32_t>(m_cmdBufs.size()), m_cmdBufs.data());
        m_cmdBufs.clear();
    }

    for (auto& fb : m_framebufs)
        if (fb) vkDestroyFramebuffer(m_dev, fb, nullptr);
    m_framebufs.clear();

    for (auto& v : m_swapViews)
        if (v) vkDestroyImageView(m_dev, v, nullptr);
    m_swapViews.clear();
    m_swapImages.clear();

    if (m_swapchain) {
        vkDestroySwapchainKHR(m_dev, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

// ===========================================================================
// recreateSwapchain  – full swapchain + framebuffer + cmd buffer rebuild
// ===========================================================================
bool VulkanRenderer::recreateSwapchain() {
    destroySwapchain();
    return createSwapchain()
        && createFramebuffers()
        && createCommandBuffers();
}

// ===========================================================================
// draw  – called every frame
// ===========================================================================
bool VulkanRenderer::draw() {
    auto& fence   = m_fences[m_currentFrame];
    auto& semImg  = m_semImage[m_currentFrame];
    auto& semRend = m_semRender[m_currentFrame];

    vkWaitForFences(m_dev, 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_dev, 1, &fence);

    uint32_t idx;
    VkResult r = vkAcquireNextImageKHR(m_dev, m_swapchain, UINT64_MAX,
                                        semImg, VK_NULL_HANDLE, &idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!recreateSwapchain()) {
            std::printf("WARN: swapchain recreate failed (minimised?)\n");
            return true;               // keep loop alive, try again next frame
        }
        return true;                   // skip rendering; swapchain was just rebuilt
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        std::printf("WARN: vkAcquireNextImageKHR = %d\n", r);
        return false;
    }

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &semImg;
    si.pWaitDstStageMask    = &wait;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &m_cmdBufs[idx];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &semRend;
    vkQueueSubmit(m_gfxQueue, 1, &si, fence);

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &semRend;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &m_swapchain;
    pi.pImageIndices      = &idx;

    r = vkQueuePresentKHR(m_presQueue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        if (!recreateSwapchain()) {
            std::printf("WARN: swapchain recreate failed after present\n");
            // Keep the loop alive; next acquire will handle the stale state
        }
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

// ===========================================================================
// cleanup
// ===========================================================================
void VulkanRenderer::cleanup() {
    if (!m_dev) return;
    vkDeviceWaitIdle(m_dev);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_fences[i])     vkDestroyFence(m_dev, m_fences[i], nullptr);
        if (m_semRender[i])  vkDestroySemaphore(m_dev, m_semRender[i], nullptr);
        if (m_semImage[i])   vkDestroySemaphore(m_dev, m_semImage[i], nullptr);
    }

    destroySwapchain();

    if (m_cmdPool) {
        vkDestroyCommandPool(m_dev, m_cmdPool, nullptr);
        m_cmdPool = VK_NULL_HANDLE;
    }

    if (m_vertBuf) vkDestroyBuffer(m_dev, m_vertBuf, nullptr);
    if (m_vertMem) vkFreeMemory(m_dev, m_vertMem, nullptr);

    if (m_pipeline)    vkDestroyPipeline(m_dev, m_pipeline, nullptr);
    if (m_pipeLayout)  vkDestroyPipelineLayout(m_dev, m_pipeLayout, nullptr);
    if (m_renderPass)  vkDestroyRenderPass(m_dev, m_renderPass, nullptr);

    vkDestroyDevice(m_dev, nullptr);
    m_dev = VK_NULL_HANDLE;

    // Destroy surface + instance via window
    if (m_instance) {
        m_window->destroySurface(m_instance);
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}
