#include "vulkan_renderer.hpp"
#include "../../utils/math3d.hpp" // Vec3, Vec4, Mat4
#include "../args.hpp"
#include "../platform/window.hpp"
#include "../shader_compiler.hpp"
#include "vulkan_texture.hpp"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_vulkan.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <chrono>

// ===========================================================================
// init  –  urutan inisialisasi
// ===========================================================================
bool VulkanRenderer::init(Window *window, const Args &args) {
  m_window = window;
  m_validation = args.validation;
  m_vertPath = args.vertShader;
  m_fragPath = args.fragShader;
  m_rotationSpeed = args.rotationSpeed;

  auto exts = window->getRequiredInstanceExtensions();

  return createInstance(exts) &&
         (window->createSurface(m_instance) != VK_NULL_HANDLE) &&
         pickPhysicalDevice() && createDevice() && createSwapchain() &&
         createRenderPass() &&
         createDescriptorSetLayout() // harus sebelum pipeline
         && createPipeline() && createFramebuffers() && createVertexBuffer() &&
         createUniformBuffer() &&
         createCommandPool()     // command pool dulu, dibutuhkan texture upload
         && createTextureImage() // load texture sebelum descriptor
         && createDescriptorPool() && createDescriptorSet() &&
         createCommandBuffers() && createSyncObjects() && initImGui();
}

// ===========================================================================
// Instance
// ===========================================================================
bool VulkanRenderer::createInstance(const std::vector<const char *> &exts) {
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "Vulkan Triangle";
  app.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  app.pEngineName = "No Engine";
  app.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  app.apiVersion = VK_API_VERSION_1_3;

  const char *layers[1]{};
  uint32_t layerCount = 0;
  if (m_validation) {
    layers[0] = "VK_LAYER_KHRONOS_validation";
    layerCount = 1;
  }

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &app;
  ci.enabledLayerCount = layerCount;
  ci.ppEnabledLayerNames = layerCount ? layers : nullptr;
  ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
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

  auto it = std::find_if(devices.begin(), devices.end(), [&](VkPhysicalDevice d) {
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
      if (support)
        pf = static_cast<int32_t>(i);
    }

    uint32_t fc = 0, mc = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(d, surface, &fc, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(d, surface, &mc, nullptr);

    if (gf >= 0 && pf >= 0 && fc > 0 && mc > 0) {
      m_gfxFamily = static_cast<uint32_t>(gf);
      m_presFamily = static_cast<uint32_t>(pf);
      return true;
    }
    return false;
  });

  if (it != devices.end()) {
    m_physDev = *it;
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
    qcis.push_back({VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, f,
                    1, &priority});
  }

  const char *devExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkPhysicalDeviceFeatures features{};

  VkDeviceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  ci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
  ci.pQueueCreateInfos = qcis.data();
  ci.enabledExtensionCount = 1;
  ci.ppEnabledExtensionNames = devExts;
  ci.pEnabledFeatures = &features;

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

  auto fmtIt = std::find_if(
      formats.begin(), formats.end(), [](const VkSurfaceFormatKHR &f) {
        return f.format == VK_FORMAT_B8G8R8A8_SRGB &&
               f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      });
  VkSurfaceFormatKHR fmt = (fmtIt != formats.end()) ? *fmtIt : formats[0];
  m_swapFmt = fmt.format;

  // --- present mode ---
  uint32_t mc = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDev, surface, &mc, nullptr);
  std::vector<VkPresentModeKHR> modes(mc);
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDev, surface, &mc,
                                            modes.data());

  auto modeIt = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
  VkPresentModeKHR mode = (modeIt != modes.end()) ? *modeIt : VK_PRESENT_MODE_FIFO_KHR;

  // --- extent ---
  if (caps.currentExtent.width != UINT32_MAX) {
    m_swapExtent = caps.currentExtent;
  } else {
    int w, h;
    m_window->getFramebufferSize(w, h);
    m_swapExtent = {
        std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width,
                   caps.maxImageExtent.width),
        std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height,
                   caps.maxImageExtent.height)};
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
  ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  ci.surface = surface;
  ci.minImageCount = imgCount;
  ci.imageFormat = m_swapFmt;
  ci.imageColorSpace = fmt.colorSpace;
  ci.imageExtent = m_swapExtent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = mode;
  ci.clipped = VK_TRUE;

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
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_swapImages[i];
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_swapFmt;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(m_dev, &vi, nullptr, &m_swapViews[i]);
  }

  std::printf("Swapchain: %zu images, %dx%d\n", m_swapImages.size(),
              m_swapExtent.width, m_swapExtent.height);
  return true;
}

// ===========================================================================
// Render pass
// ===========================================================================
bool VulkanRenderer::createRenderPass() {
  VkAttachmentDescription att{};
  att.format = m_swapFmt;
  att.samples = VK_SAMPLE_COUNT_1_BIT;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription sp{};
  sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sp.colorAttachmentCount = 1;
  sp.pColorAttachments = &ref;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 1;
  ci.pAttachments = &att;
  ci.subpassCount = 1;
  ci.pSubpasses = &sp;
  ci.dependencyCount = 1;
  ci.pDependencies = &dep;

  if (vkCreateRenderPass(m_dev, &ci, nullptr, &m_renderPass) != VK_SUCCESS) {
    std::printf("ERROR: vkCreateRenderPass failed\n");
    return false;
  }
  return true;
}

// ===========================================================================
// Descriptor set layout  –  binding 0 = UBO (MVP), binding 1 = Texture sampler
// ===========================================================================
bool VulkanRenderer::createDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding bindings[2]{};

  // Binding 0: UBO (MVP matrix) - vertex shader
  bindings[0].binding = 0; // register(b0)
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  // Binding 1: Combined image sampler (texture) - fragment shader
  bindings[1].binding = 1; // register(t0, s0)
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  ci.bindingCount = 2;
  ci.pBindings = bindings;

  if (vkCreateDescriptorSetLayout(m_dev, &ci, nullptr, &m_descLayout) !=
      VK_SUCCESS) {
    std::printf("ERROR: vkCreateDescriptorSetLayout failed\n");
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
  auto makeMod = [&](const auto &code, VkShaderModule *mod) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode = code.data();
    return vkCreateShaderModule(m_dev, &ci, nullptr, mod);
  };

  if (makeMod(vertCode, &vertMod) != VK_SUCCESS ||
      makeMod(fragCode, &fragMod) != VK_SUCCESS) {
    std::printf("ERROR: vkCreateShaderModule failed\n");
    return false;
  }

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertMod;
  stages[0].pName = "vs_main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragMod;
  stages[1].pName = "ps_main";

  // Vertex binding: stride = 3 (pos) + 3 (color) + 2 (uv) = 8 float
  VkVertexInputBindingDescription bind{};
  bind.binding = 0;
  bind.stride = 8 * sizeof(float);
  bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  // Attribute 0: posisi   (float3) di offset 0
  // Attribute 1: warna    (float3) di offset 3*sizeof(float)
  // Attribute 2: texcoord (float2) di offset 6*sizeof(float)
  VkVertexInputAttributeDescription attr[3]{};
  attr[0].location = 0;
  attr[0].binding = 0;
  attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attr[0].offset = 0;
  attr[1].location = 1;
  attr[1].binding = 0;
  attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attr[1].offset = 3 * sizeof(float);
  attr[2].location = 2;
  attr[2].binding = 0;
  attr[2].format = VK_FORMAT_R32G32_SFLOAT;
  attr[2].offset = 6 * sizeof(float);

  VkPipelineVertexInputStateCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &bind;
  vi.vertexAttributeDescriptionCount = 3;
  vi.pVertexAttributeDescriptions = attr;

  VkPipelineInputAssemblyStateCreateInfo ia{};
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo vs{};
  vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vs.viewportCount = 1;
  vs.pViewports = nullptr; // Akan di-set secara dinamis
  vs.scissorCount = 1;
  vs.pScissors = nullptr;  // Akan di-set secara dinamis

  VkPipelineRasterizationStateCreateInfo rs{};
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_BACK_BIT;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{};
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState cbAtt{};
  cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo cb{};
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.attachmentCount = 1;
  cb.pAttachments = &cbAtt;

  // Pipeline layout: sekarang pakai descriptor set layout
  VkPipelineLayoutCreateInfo pl{};
  pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pl.setLayoutCount = 1;
  pl.pSetLayouts = &m_descLayout;
  if (vkCreatePipelineLayout(m_dev, &pl, nullptr, &m_pipeLayout) !=
      VK_SUCCESS) {
    std::printf("ERROR: vkCreatePipelineLayout failed\n");
    return false;
  }

  VkDynamicState dynamicStates[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  VkGraphicsPipelineCreateInfo gi{};
  gi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  gi.stageCount = 2;
  gi.pStages = stages;
  gi.pVertexInputState = &vi;
  gi.pInputAssemblyState = &ia;
  gi.pViewportState = &vs;
  gi.pRasterizationState = &rs;
  gi.pMultisampleState = &ms;
  gi.pColorBlendState = &cb;
  gi.pDynamicState = &dynamicState;
  gi.layout = m_pipeLayout;
  gi.renderPass = m_renderPass;
  gi.subpass = 0;

  VkResult res = vkCreateGraphicsPipelines(m_dev, VK_NULL_HANDLE, 1, &gi,
                                           nullptr, &m_pipeline);

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
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = m_renderPass;
    ci.attachmentCount = 1;
    ci.pAttachments = &m_swapViews[i];
    ci.width = m_swapExtent.width;
    ci.height = m_swapExtent.height;
    ci.layers = 1;
    if (vkCreateFramebuffer(m_dev, &ci, nullptr, &m_framebufs[i]) !=
        VK_SUCCESS) {
      std::printf("ERROR: framebuffer %zu\n", i);
      return false;
    }
  }
  return true;
}

// ===========================================================================
// Vertex buffer  –  segitiga 3D (posisi x,y,z + warna r,g,b)
// ===========================================================================
bool VulkanRenderer::createVertexBuffer() {
  // Format: float3 posisi, float3 warna, float2 texcoord = 8 float per vertex
  struct Vertex {
    float pos[3];
    float color[3];
    float uv[2];
  };
  const Vertex verts[] = {
      // ============ PIRAMIDA DENGAN ALAS PERSEGI ============
      // Apex (puncak):         ( 0, -1,  0)
      // Base front-left:       (-1,  1, -1)
      // Base front-right:      ( 1,  1, -1)
      // Base back-right:       ( 1,  1,  1)
      // Base back-left:        (-1,  1,  1)

      // Sisi Depan (Front face)
      {{ 0.0f, -1.0f,  0.0f}, {1.0f, 0.75f, 0.80f}, {0.50f, 0.05f}},
      {{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.75f, 0.80f}, {0.80f, 0.65f}},
      {{-1.0f,  1.0f, -1.0f}, {1.0f, 0.75f, 0.80f}, {0.20f, 0.65f}},

      // Sisi Kanan (Right face)
      {{ 0.0f, -1.0f,  0.0f}, {0.90f, 0.60f, 0.68f}, {0.50f, 0.05f}},
      {{ 1.0f,  1.0f,  1.0f}, {0.90f, 0.60f, 0.68f}, {0.80f, 0.65f}},
      {{ 1.0f,  1.0f, -1.0f}, {0.90f, 0.60f, 0.68f}, {0.20f, 0.65f}},

      // Sisi Belakang (Back face)
      {{ 0.0f, -1.0f,  0.0f}, {0.82f, 0.50f, 0.58f}, {0.50f, 0.05f}},
      {{-1.0f,  1.0f,  1.0f}, {0.82f, 0.50f, 0.58f}, {0.80f, 0.65f}},
      {{ 1.0f,  1.0f,  1.0f}, {0.82f, 0.50f, 0.58f}, {0.20f, 0.65f}},

      // Sisi Kiri (Left face)
      {{ 0.0f, -1.0f,  0.0f}, {0.85f, 0.55f, 0.63f}, {0.50f, 0.05f}},
      {{-1.0f,  1.0f, -1.0f}, {0.85f, 0.55f, 0.63f}, {0.80f, 0.65f}},
      {{-1.0f,  1.0f,  1.0f}, {0.85f, 0.55f, 0.63f}, {0.20f, 0.65f}},

      // Alas Bawah - segitiga 1 (Bottom base tri 1)
      {{-1.0f,  1.0f, -1.0f}, {0.75f, 0.42f, 0.50f}, {0.20f, 0.20f}},
      {{ 1.0f,  1.0f, -1.0f}, {0.75f, 0.42f, 0.50f}, {0.80f, 0.20f}},
      {{ 1.0f,  1.0f,  1.0f}, {0.75f, 0.42f, 0.50f}, {0.80f, 0.65f}},

      // Alas Bawah - segitiga 2 (Bottom base tri 2)
      {{-1.0f,  1.0f, -1.0f}, {0.75f, 0.42f, 0.50f}, {0.20f, 0.20f}},
      {{ 1.0f,  1.0f,  1.0f}, {0.75f, 0.42f, 0.50f}, {0.80f, 0.65f}},
      {{-1.0f,  1.0f,  1.0f}, {0.75f, 0.42f, 0.50f}, {0.20f, 0.65f}},
  };
  VkDeviceSize size = sizeof(verts);

  VkBufferCreateInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = size;
  bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_dev, &bi, nullptr, &m_vertBuf) != VK_SUCCESS) {
    std::printf("ERROR: vertex buffer\n");
    return false;
  }

  VkMemoryRequirements mr;
  vkGetBufferMemoryRequirements(m_dev, m_vertBuf, &mr);

  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (vkAllocateMemory(m_dev, &ai, nullptr, &m_vertMem) != VK_SUCCESS) {
    std::printf("ERROR: vertex memory\n");
    return false;
  }
  vkBindBufferMemory(m_dev, m_vertBuf, m_vertMem, 0);

  void *data;
  vkMapMemory(m_dev, m_vertMem, 0, size, 0, &data);
  std::memcpy(data, verts, size);
  vkUnmapMemory(m_dev, m_vertMem);
  return true;
}

// ===========================================================================
// Uniform buffer  –  tempat upload matriks MVP tiap frame
// ===========================================================================
bool VulkanRenderer::createUniformBuffer() {
  VkDeviceSize size = sizeof(Mat4); // 16 float = 64 byte

  VkBufferCreateInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = size;
  bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_dev, &bi, nullptr, &m_ubo) != VK_SUCCESS) {
    std::printf("ERROR: uniform buffer\n");
    return false;
  }

  VkMemoryRequirements mr;
  vkGetBufferMemoryRequirements(m_dev, m_ubo, &mr);

  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (vkAllocateMemory(m_dev, &ai, nullptr, &m_uboMem) != VK_SUCCESS) {
    std::printf("ERROR: uniform buffer memory\n");
    return false;
  }
  vkBindBufferMemory(m_dev, m_ubo, m_uboMem, 0);

  std::printf("Uniform buffer created (%llu bytes)\n",
              (unsigned long long)size);
  return true;
}

// ===========================================================================
// Texture image  –  load gambar dari file, atau fallback checkerboard
// ===========================================================================
bool VulkanRenderer::createTextureImage() {
  std::string path = m_texturePath;

  // Jika path kosong, coba default texture
  if (path.empty()) {
    // Coba beberapa lokasi umum
    const char *candidates[] = {
        "src/res/wowo_pencinta_sawit.png",
        "../src/res/wowo_pencinta_sawit.png",
        "../../src/res/wowo_pencinta_sawit.png",
        "res/wowo_pencinta_sawit.png",
    };

    auto candIt = std::find_if(
        std::begin(candidates), std::end(candidates), [](const char *c) {
          if (FILE *f = std::fopen(c, "rb")) {
            std::fclose(f);
            return true;
          }
          return false;
        }
    );
    if (candIt != std::end(candidates)) {
      path = *candIt;
    }
  }

  return loadTextureFromFile(m_dev, m_physDev, m_cmdPool, m_gfxQueue, path,
                             m_texture);
}

// ===========================================================================
// Descriptor pool
// ===========================================================================
bool VulkanRenderer::createDescriptorPool() {
  VkDescriptorPoolSize poolSizes[2]{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = 1;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = 1;

  VkDescriptorPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  ci.maxSets = 1;
  ci.poolSizeCount = 2;
  ci.pPoolSizes = poolSizes;

  if (vkCreateDescriptorPool(m_dev, &ci, nullptr, &m_descPool) != VK_SUCCESS) {
    std::printf("ERROR: vkCreateDescriptorPool failed\n");
    return false;
  }
  return true;
}

// ===========================================================================
// Descriptor set  –  hubungkan UBO ke binding 0, texture ke binding 1
// ===========================================================================
bool VulkanRenderer::createDescriptorSet() {
  VkDescriptorSetAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ai.descriptorPool = m_descPool;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts = &m_descLayout;

  if (vkAllocateDescriptorSets(m_dev, &ai, &m_descSet) != VK_SUCCESS) {
    std::printf("ERROR: vkAllocateDescriptorSets failed\n");
    return false;
  }

  // Binding 0: UBO
  VkDescriptorBufferInfo bufInfo{};
  bufInfo.buffer = m_ubo;
  bufInfo.offset = 0;
  bufInfo.range = sizeof(Mat4);

  // Binding 1: Texture sampler
  VkDescriptorImageInfo imgInfo{};
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imgInfo.imageView = m_texture.view;
  imgInfo.sampler = m_texture.sampler;

  VkWriteDescriptorSet writes[2]{};

  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_descSet;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &bufInfo;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = m_descSet;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(m_dev, 2, writes, 0, nullptr);
  return true;
}

// ===========================================================================
// Command pool  –  dibuat lebih awal, dibutuhkan untuk texture upload
// ===========================================================================
bool VulkanRenderer::createCommandPool() {
  if (m_cmdPool)
    return true; // sudah dibuat

  VkCommandPoolCreateInfo pci{};
  pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pci.queueFamilyIndex = m_gfxFamily;

  if (vkCreateCommandPool(m_dev, &pci, nullptr, &m_cmdPool) != VK_SUCCESS) {
    std::printf("ERROR: command pool\n");
    return false;
  }
  return true;
}

// ===========================================================================
// Command buffers
// ===========================================================================
bool VulkanRenderer::createCommandBuffers() {

  m_cmdBufs.resize(m_framebufs.size());
  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = m_cmdPool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = static_cast<uint32_t>(m_cmdBufs.size());

  if (vkAllocateCommandBuffers(m_dev, &ai, m_cmdBufs.data()) != VK_SUCCESS) {
    std::printf("ERROR: allocate command buffers\n");
    return false;
  }

  // for (size_t i = 0; i < m_cmdBufs.size(); ++i) {
  //   VkCommandBufferBeginInfo bi{};
  //   bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  //   bi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  //   vkBeginCommandBuffer(m_cmdBufs[i], &bi);

  //   VkClearValue clear{{{0.0f, 0.0f, 0.0f, 1.0f}}};

  //   VkRenderPassBeginInfo rp{};
  //   rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  //   rp.renderPass = m_renderPass;
  //   rp.framebuffer = m_framebufs[i];
  //   rp.renderArea = {{0, 0}, m_swapExtent};
  //   rp.clearValueCount = 1;
  //   rp.pClearValues = &clear;
  //   vkCmdBeginRenderPass(m_cmdBufs[i], &rp, VK_SUBPASS_CONTENTS_INLINE);

  //   vkCmdBindPipeline(m_cmdBufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
  //                     m_pipeline);

  //   VkViewport viewport{};
  //   viewport.x = 0.0f;
  //   viewport.y = 0.0f;
  //   viewport.width = static_cast<float>(m_swapExtent.width);
  //   viewport.height = static_cast<float>(m_swapExtent.height);
  //   viewport.minDepth = 0.0f;
  //   viewport.maxDepth = 1.0f;
  //   vkCmdSetViewport(m_cmdBufs[i], 0, 1, &viewport);

  //   VkRect2D scissor{};
  //   scissor.offset = {0, 0};
  //   scissor.extent = m_swapExtent;
  //   vkCmdSetScissor(m_cmdBufs[i], 0, 1, &scissor);

  //   // Bind vertex buffer
  //   VkBuffer vb[] = {m_vertBuf};
  //   VkDeviceSize off[] = {0};
  //   vkCmdBindVertexBuffers(m_cmdBufs[i], 0, 1, vb, off);

  //   // Bind descriptor set (berisi MVP)
  //   vkCmdBindDescriptorSets(m_cmdBufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
  //                           m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

  //   vkCmdDraw(m_cmdBufs[i], 18, 1, 0, 0);

  //   vkCmdEndRenderPass(m_cmdBufs[i]);

  //   if (vkEndCommandBuffer(m_cmdBufs[i]) != VK_SUCCESS) {
  //     std::printf("ERROR: record cmd buf %zu\n", i);
  //     return false;
  //   }
  // }
  return true;
}

// ===========================================================================
// Sync objects
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
// destroySwapchain
// ===========================================================================
void VulkanRenderer::destroySwapchain() {
  vkDeviceWaitIdle(m_dev);

  if (m_cmdPool) {
    vkFreeCommandBuffers(m_dev, m_cmdPool,
                         static_cast<uint32_t>(m_cmdBufs.size()),
                         m_cmdBufs.data());
    m_cmdBufs.clear();
  }

  for (auto &fb : m_framebufs)
    if (fb)
      vkDestroyFramebuffer(m_dev, fb, nullptr);
  m_framebufs.clear();

  for (auto &v : m_swapViews)
    if (v)
      vkDestroyImageView(m_dev, v, nullptr);
  m_swapViews.clear();
  m_swapImages.clear();

  if (m_swapchain) {
    vkDestroySwapchainKHR(m_dev, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }
}

// ===========================================================================
// recreateSwapchain
// ===========================================================================
bool VulkanRenderer::recreateSwapchain() {
  destroySwapchain();
  return createSwapchain() && createFramebuffers() && createCommandBuffers();
}

// ===========================================================================
// draw  –  update MVP, submit, present
// ===========================================================================
bool VulkanRenderer::draw() {
  // --- Handle Window Resize (mencegah gambar gepeng) ---
  int w = 0, h = 0;
  m_window->getFramebufferSize(w, h);
  if (w == 0 || h == 0) return true; // minimized

  if (static_cast<uint32_t>(w) != m_swapExtent.width || 
      static_cast<uint32_t>(h) != m_swapExtent.height) {
    if (!recreateSwapchain()) return false;
  }

  // --- update sudut rotasi berdasarkan waktu nyata (time-based animation) ---
  static auto startTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
  
  // Kecepatan putaran diatur melalui argumen CLI --speed
  m_angle = time * m_rotationSpeed;

  // --- hitung matriks MVP ---
  float aspect = (float)m_swapExtent.width / (float)m_swapExtent.height;
  Mat4 proj = Mat4::perspective(1.0472f, aspect, 0.1f, 10.0f); // ~60°
  Mat4 model = Mat4::translation(0.0f, 0.0f, 3.5f) * Mat4::rotationY(m_angle);
  Mat4 mvp = proj * model;       // P * M (standard column-vector)
  Mat4 upload = mvp.transpose(); // transpose untuk row_major HLSL

  // --- upload ke GPU ---
  void *data;
  vkMapMemory(m_dev, m_uboMem, 0, sizeof(Mat4), 0, &data);
  std::memcpy(data, upload.data(), sizeof(Mat4));
  vkUnmapMemory(m_dev, m_uboMem);

  // --- render seperti biasa ---
  auto &fence = m_fences[m_currentFrame];
  auto &semImg = m_semImage[m_currentFrame];
  auto &semRend = m_semRender[m_currentFrame];

  vkWaitForFences(m_dev, 1, &fence, VK_TRUE, UINT64_MAX);
  vkResetFences(m_dev, 1, &fence);

  uint32_t idx;
  VkResult r = vkAcquireNextImageKHR(m_dev, m_swapchain, UINT64_MAX, semImg,
                                     VK_NULL_HANDLE, &idx);
  if (r == VK_ERROR_OUT_OF_DATE_KHR) {
    if (!recreateSwapchain()) {
      std::printf("WARN: swapchain recreate failed (minimised?)\n");
      return true;
    }
    return true;
  }
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    std::printf("WARN: vkAcquireNextImageKHR = %d\n", r);
    return false;
  }

  // --- Record Command Buffer Dinamis ---
  vkResetCommandBuffer(m_cmdBufs[idx], 0);

  VkCommandBufferBeginInfo cbi{};
  cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cbi.flags = 0;
  vkBeginCommandBuffer(m_cmdBufs[idx], &cbi);

  VkClearValue clear{{{0.0f, 0.0f, 0.0f, 1.0f}}};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = m_renderPass;
  rp.framebuffer = m_framebufs[idx];
  rp.renderArea = {{0, 0}, m_swapExtent};
  rp.clearValueCount = 1;
  rp.pClearValues = &clear;
  vkCmdBeginRenderPass(m_cmdBufs[idx], &rp, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(m_cmdBufs[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

  VkViewport viewport{};
  viewport.width = (float)m_swapExtent.width;
  viewport.height = (float)m_swapExtent.height;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(m_cmdBufs[idx], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = m_swapExtent;
  vkCmdSetScissor(m_cmdBufs[idx], 0, 1, &scissor);

  VkBuffer vb[] = {m_vertBuf};
  VkDeviceSize off[] = {0};
  vkCmdBindVertexBuffers(m_cmdBufs[idx], 0, 1, vb, off);

  vkCmdBindDescriptorSets(m_cmdBufs[idx], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

  vkCmdDraw(m_cmdBufs[idx], 18, 1, 0, 0);

  // --- Render ImGui ---
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  if (m_window->window_buffer.showOption) {
      ImGui::Begin("Options", &m_window->window_buffer.showOption);
      ImGui::Text("List of Items:");
      for (const auto& item : m_window->window_buffer.itemList) {
          ImGui::BulletText("%s", item.c_str());
      }
      ImGui::End();
  }

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_cmdBufs[idx]);

  vkCmdEndRenderPass(m_cmdBufs[idx]);
  vkEndCommandBuffer(m_cmdBufs[idx]);

  VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &semImg;
  si.pWaitDstStageMask = &wait;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &m_cmdBufs[idx];
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &semRend;
  vkQueueSubmit(m_gfxQueue, 1, &si, fence);

  VkPresentInfoKHR pi{};
  pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &semRend;
  pi.swapchainCount = 1;
  pi.pSwapchains = &m_swapchain;
  pi.pImageIndices = &idx;

  r = vkQueuePresentKHR(m_presQueue, &pi);
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    if (!recreateSwapchain()) {
      std::printf("WARN: swapchain recreate failed after present\n");
    }
  }

  m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  return true;
}

// ===========================================================================
// cleanup
// ===========================================================================
void VulkanRenderer::cleanup() {
  if (!m_dev)
    return;
  vkDeviceWaitIdle(m_dev);

  cleanupImGui();

  // Sync objects
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (m_fences[i])
      vkDestroyFence(m_dev, m_fences[i], nullptr);
    if (m_semRender[i])
      vkDestroySemaphore(m_dev, m_semRender[i], nullptr);
    if (m_semImage[i])
      vkDestroySemaphore(m_dev, m_semImage[i], nullptr);
  }

  destroySwapchain();

  if (m_cmdPool) {
    vkDestroyCommandPool(m_dev, m_cmdPool, nullptr);
    m_cmdPool = VK_NULL_HANDLE;
  }

  // Descriptor
  if (m_descPool)
    vkDestroyDescriptorPool(m_dev, m_descPool, nullptr);
  if (m_descLayout)
    vkDestroyDescriptorSetLayout(m_dev, m_descLayout, nullptr);

  // Texture
  destroyTexture(m_dev, m_texture);

  // UBO
  if (m_ubo)
    vkDestroyBuffer(m_dev, m_ubo, nullptr);
  if (m_uboMem)
    vkFreeMemory(m_dev, m_uboMem, nullptr);

  // Vertex
  if (m_vertBuf)
    vkDestroyBuffer(m_dev, m_vertBuf, nullptr);
  if (m_vertMem)
    vkFreeMemory(m_dev, m_vertMem, nullptr);

  if (m_pipeline)
    vkDestroyPipeline(m_dev, m_pipeline, nullptr);
  if (m_pipeLayout)
    vkDestroyPipelineLayout(m_dev, m_pipeLayout, nullptr);
  if (m_renderPass)
    vkDestroyRenderPass(m_dev, m_renderPass, nullptr);

  vkDestroyDevice(m_dev, nullptr);
  m_dev = VK_NULL_HANDLE;

  if (m_instance) {
    m_window->destroySurface(m_instance);
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }
}

// ===========================================================================
// findMemoryType
// ===========================================================================
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
// ImGui Initialization
// ===========================================================================
bool VulkanRenderer::initImGui() {
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
  };
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
  pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  if (vkCreateDescriptorPool(m_dev, &pool_info, nullptr, &m_imguiPool) != VK_SUCCESS) {
      return false;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  ImGui::StyleColorsDark();

  ImGui_ImplWin32_Init(m_window->getHandle());

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = m_instance;
  init_info.PhysicalDevice = m_physDev;
  init_info.Device = m_dev;
  init_info.QueueFamily = m_gfxFamily;
  init_info.Queue = m_gfxQueue;
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = m_imguiPool;
  init_info.PipelineInfoMain.RenderPass = m_renderPass;
  init_info.PipelineInfoMain.Subpass = 0;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
  init_info.ImageCount = static_cast<uint32_t>(m_swapImages.size());
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = nullptr;
  ImGui_ImplVulkan_Init(&init_info);

  return true;
}

void VulkanRenderer::cleanupImGui() {
  if (m_imguiPool) {
      ImGui_ImplVulkan_Shutdown();
      ImGui_ImplWin32_Shutdown();
      ImGui::DestroyContext();
      vkDestroyDescriptorPool(m_dev, m_imguiPool, nullptr);
      m_imguiPool = VK_NULL_HANDLE;
  }
}
