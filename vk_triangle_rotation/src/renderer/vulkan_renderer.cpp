#include "vulkan_renderer.hpp"
#include "../../utils/math3d.hpp" // Vec3, Vec4, Mat4, Vertex
#include "../args.hpp"
#include "../geometry_controller.hpp"
#include "../platform/window.hpp"
#include "../shader_compiler.hpp"
#include "vulkan_texture.hpp"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../utils/stb_truetype.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <set>
#include <utility>

// Resolve a filename relative to the executable's directory.
static std::string getExeRelativePath(const char *filename) {
  char buf[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH)
    return filename;
  char *sep = strrchr(buf, '\\');
  if (sep)
    *(sep + 1) = '\0';
  else
    buf[0] = '\0';
  return std::string(buf) + filename;
}

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

  bool ok = createInstance(exts) &&
            (window->createSurface(m_instance) != VK_NULL_HANDLE) &&
            pickPhysicalDevice() && createDevice() && createSwapchain() &&
            createDepthResources() && createRenderPass() &&
            createDescriptorSetLayout() // harus sebelum pipeline
            && createPipeline() && createFramebuffers() &&
            createVertexBuffer() && createUniformBuffer() &&
            createCommandPool() // command pool dulu, dibutuhkan texture upload
            && createTextureImage() // load texture sebelum descriptor
            && createDescriptorPool() && createDescriptorSet() &&
            createCommandBuffers() && createSyncObjects() && initImGui();

  if (ok) {
    updateScoreTexture();
  }
  return ok;
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

  auto it =
      std::find_if(devices.begin(), devices.end(), [&](VkPhysicalDevice d) {
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

  auto modeIt =
      std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
  VkPresentModeKHR mode =
      (modeIt != modes.end()) ? *modeIt : VK_PRESENT_MODE_FIFO_KHR;

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

  VkAttachmentDescription depthAtt{};
  depthAtt.format = m_depthFormat;
  depthAtt.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription sp{};
  sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sp.colorAttachmentCount = 1;
  sp.pColorAttachments = &ref;
  sp.pDepthStencilAttachment = &depthRef;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[2] = {att, depthAtt};

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 2;
  ci.pAttachments = attachments;
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

  // ===========================================================================
  // 1. VERTEX INPUT STATE (Pipeline Vertex Input)
  // Filosofi Vulkan: Berbeda dengan OpenGL yang menyimpan state di dalam VAO
  // (Vertex Array Object), Vulkan mewajibkan kita mendeklarasikan format vertex
  // (layout memori) ke dalam Pipeline secara statis/di awal. Secara harfiah:
  // `vi` (Vertex Input) mengikat deskripsi "bentuk" data vertex kita (seperti
  // berapa ukurannya/stride, dan tipe datanya seperti float3) kepada shader.
  // ===========================================================================
  VkPipelineVertexInputStateCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &bind; // Menjelaskan ukuran 1 Vertex (stride)
  vi.vertexAttributeDescriptionCount = 3;
  vi.pVertexAttributeDescriptions =
      attr; // Menjelaskan isi di dalam Vertex (Posisi, Warna, UV)

  // ===========================================================================
  // 2. INPUT ASSEMBLY STATE
  // Filosofi Vulkan: Menentukan bagaimana GPU merakit kumpulan titik (vertex)
  // tunggal menjadi bentuk primitif geometri. Secara harfiah:
  // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST (Macro) berarti setiap 3 titik (vertex)
  // akan membentuk 1 segitiga yang saling lepas/independen.
  // ===========================================================================
  VkPipelineInputAssemblyStateCreateInfo ia{};
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // ===========================================================================
  // 3. VIEWPORT & SCISSOR STATE
  // Filosofi Vulkan: Vulkan memisahkan konsep Viewport (transformasi ukuran
  // layar/resolusi) dan Scissor (pemotongan/clipping area piksel). Secara
  // harfiah: Kita membiarkannya `nullptr` karena kita menggunakan "Dynamic
  // State". Artinya kita bisa mengubah resolusi kapan saja saat me-record
  // Command Buffer tanpa harus menghancurkan dan membuat ulang seluruh Pipeline
  // ini.
  // ===========================================================================
  VkPipelineViewportStateCreateInfo vs{};
  vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vs.viewportCount = 1;
  vs.pViewports = nullptr; // Akan di-set secara dinamis via vkCmdSetViewport
  vs.scissorCount = 1;
  vs.pScissors = nullptr; // Akan di-set secara dinamis via vkCmdSetScissor

  // ===========================================================================
  // 4. RASTERIZATION STATE
  // Filosofi Vulkan: Ini adalah inti pengubah geometri 3D matematika menjadi
  // piksel 2D. Macro yang dipakai di sini sangat berpengaruh pada performa dan
  // hasil visual. Secara harfiah:
  // - VK_POLYGON_MODE_FILL: Mengisi penuh area segitiga dengan warna (bukan
  // wireframe).
  // - VK_CULL_MODE_NONE: Tidak membuang/menyembunyikan sisi belakang (Backface)
  // maupun depan geometri.
  //   Penting untuk objek yang bisa dilihat dari segala arah secara bebas.
  // - VK_FRONT_FACE_CLOCKWISE: Mendefinisikan bahwa vertex yang diurutkan
  // searah jarum jam (Clockwise) adalah sisi depan.
  // ===========================================================================
  VkPipelineRasterizationStateCreateInfo rs{};
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE; // Matikan Culling agar mesh terlihat
                                   // menyeluruh saat dirotasi ekstrim
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.lineWidth =
      1.0f; // Tebal garis jika menggunakan polygonMode = LINE (Wireframe)

  // ===========================================================================
  // 5. MULTISAMPLE STATE (Anti-Aliasing)
  // Filosofi Vulkan: Konfigurasi untuk menghaluskan pinggiran objek (Edge
  // Smoothing / MSAA). Secara harfiah: VK_SAMPLE_COUNT_1_BIT berarti kita
  // menonaktifkan MSAA (1 sample per pixel). Ini paling ringan untuk performa,
  // cocok untuk rendering pixel-art / retro murni.
  // ===========================================================================
  VkPipelineMultisampleStateCreateInfo ms{};
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // ===========================================================================
  // 6. COLOR BLEND STATE
  // Filosofi Vulkan: Setelah Fragment Shader menghitung warna piksel, Vulkan
  // perlu tau bagaimana cara menimpa atau mencampur (Blend) piksel baru
  // tersebut ke layar/framebuffer lama. Secara harfiah: `cbAtt` menargetkan 1
  // attachment layar. `colorWriteMask` (Macro R_BIT | G_BIT | B_BIT | A_BIT)
  // mengizinkan GPU menulis seluruh channel warna (Red, Green, Blue, Alpha) ke
  // layar.
  // ===========================================================================
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

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  VkPipelineDepthStencilStateCreateInfo ds{};
  ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  ds.depthTestEnable = VK_TRUE;
  ds.depthWriteEnable = VK_TRUE;
  ds.depthCompareOp = VK_COMPARE_OP_LESS;
  ds.depthBoundsTestEnable = VK_FALSE;
  ds.stencilTestEnable = VK_FALSE;

  VkGraphicsPipelineCreateInfo gi{};
  gi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  gi.stageCount = 2;
  gi.pStages = stages;
  gi.pVertexInputState = &vi;
  gi.pInputAssemblyState = &ia;
  gi.pViewportState = &vs;
  gi.pRasterizationState = &rs;
  gi.pMultisampleState = &ms;
  gi.pDepthStencilState = &ds;
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
    VkImageView attachments[] = {m_swapViews[i], m_depthImageView};
    VkFramebufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = m_renderPass;
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
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
// Vertex buffer  –  dibuat dari GeometryController (polygon N-gon)
// ===========================================================================
bool VulkanRenderer::createVertexBuffer() {
  auto verts = GeometryController::generatePolygon(m_polygonSides, 1.5f);
  m_vertexCount = static_cast<uint32_t>(verts.size());
  VkDeviceSize size = verts.size() * sizeof(Vertex);

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
  std::memcpy(data, verts.data(), size);
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
    const std::string candidates[] = {
        getExeRelativePath("wowo_pencinta_sawit.png"),
        "src/res/wowo_pencinta_sawit.png",
        "../src/res/wowo_pencinta_sawit.png",
        "../../src/res/wowo_pencinta_sawit.png",
        "res/wowo_pencinta_sawit.png",
    };

    auto candIt = std::find_if(std::begin(candidates), std::end(candidates),
                               [](const std::string &c) {
                                 if (FILE *f = std::fopen(c.c_str(), "rb")) {
                                   std::fclose(f);
                                   return true;
                                 }
                                 return false;
                               });
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

  if (m_depthImageView)
    vkDestroyImageView(m_dev, m_depthImageView, nullptr);
  if (m_depthImage)
    vkDestroyImage(m_dev, m_depthImage, nullptr);
  if (m_depthImageMem)
    vkFreeMemory(m_dev, m_depthImageMem, nullptr);
  m_depthImageView = VK_NULL_HANDLE;
  m_depthImage = VK_NULL_HANDLE;
  m_depthImageMem = VK_NULL_HANDLE;

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
  return createSwapchain() && createDepthResources() && createFramebuffers() &&
         createCommandBuffers();
}

// ===========================================================================
// draw  –  update MVP, submit, present
// ===========================================================================
bool VulkanRenderer::draw() {
  // --- Handle Window Resize (mencegah gambar gepeng) ---
  int w = 0, h = 0;
  m_window->getFramebufferSize(w, h);
  if (w == 0 || h == 0)
    return true; // minimized

  if (static_cast<uint32_t>(w) != m_swapExtent.width ||
      static_cast<uint32_t>(h) != m_swapExtent.height) {
    if (!recreateSwapchain())
      return false;
  }

  // --- update sudut rotasi berdasarkan waktu nyata (time-based animation) ---
  static auto startTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  // Kecepatan putaran otomatis
  m_angle = time * m_rotationSpeed;

  // --- Siapkan ImGui Frame Baru ---
  // Memanggil NewFrame di awal sangat penting agar state input (seperti
  // io.MouseWheel) diperbarui untuk frame ini sebelum kita membacanya.
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  // --- Input Mouse (Middle Click Drag untuk Rotasi) ---
  ImGuiIO &io = ImGui::GetIO();
  if (io.MouseDown[2]) { // 2 = Middle Mouse Button
    m_rotY -=
        io.MouseDelta.x * 0.01f; // Drag horizontal -> putar di sumbu Y (Yaw)
    m_rotX += io.MouseDelta.y * 0.01f; // Drag vertikal (Inverted/Terbalik)
  }

  // Batasi (clamp) rotasi vertikal (Pitch) agar tidak flip terbalik (Maksimal
  // +/- ~85 derajat) Ini penting agar perputaran arah Yaw tidak terasa terbalik
  // (inverted) saat melewati kutub
  const float PITCH_LIMIT = 1.5f;
  if (m_rotX > PITCH_LIMIT)
    m_rotX = PITCH_LIMIT;
  if (m_rotX < -PITCH_LIMIT)
    m_rotX = -PITCH_LIMIT;

  // --- Input Mouse (Scroll Wheel untuk Zoom) ---
  // Menggunakan roda mouse (Scroll Wheel) untuk mendekatkan/menjauhkan kamera
  // (translasi sumbu Z).
  // - Jika Scroll ke depan (io.MouseWheel > 0) -> Jarak berkurang -> Zoom In
  // - Jika Scroll ke belakang (io.MouseWheel < 0) -> Jarak bertambah -> Zoom
  // Out
  if (io.MouseWheel != 0.0f) {
    m_cameraDistance -=
        io.MouseWheel * 0.5f; // Pengali 0.5 adalah kecepatan/sensitivitas zoom

    // Batasi (clamp) jarak kamera agar tidak tembus ke dalam jeroan objek,
    // maupun terbang terlalu jauh
    if (m_cameraDistance < 1.5f)
      m_cameraDistance = 1.5f; // Batas maksimal Zoom In
    if (m_cameraDistance > 20.0f)
      m_cameraDistance = 20.0f; // Batas maksimal Zoom Out
  }

  // --- hitung matriks MVP ---
  float aspect = (float)m_swapExtent.width / (float)m_swapExtent.height;

  // Proyeksi Kamera (Field of View ~60 derajat, Near Clip 0.1, Far Clip
  // ditingkatkan ke 50.0 agar objek tidak terpotong saat di-zoom out jauh)
  Mat4 proj = Mat4::perspective(1.0472f, aspect, 0.1f, 50.0f);

  // Kombinasi matriks model:
  // 1. Mat4::translation -> Memposisikan objek sejauh m_cameraDistance dari
  // layar
  // 2. Mat4::rotationX   -> Rotasi Pitch (dari drag vertical)
  // 3. Mat4::rotationY   -> Rotasi Yaw (gabungan drag horizontal + rotasi
  // otomatis)
  Mat4 model = Mat4::translation(0.0f, 0.0f, m_cameraDistance) *
               Mat4::rotationX(m_rotX) * Mat4::rotationY(m_rotY + m_angle);

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
      ImGui::EndFrame(); // <--- HARUS ADA jika NewFrame sudah dipanggil
      return true;
    }
    ImGui::EndFrame(); // <--- HARUS ADA jika NewFrame sudah dipanggil
    return true;
  }
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    std::printf("WARN: vkAcquireNextImageKHR = %d\n", r);
    ImGui::EndFrame(); // <--- HARUS ADA
    return false;
  }

  // --- Record Command Buffer Dinamis ---
  vkResetCommandBuffer(m_cmdBufs[idx], 0);

  VkCommandBufferBeginInfo cbi{};
  cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cbi.flags = 0;
  vkBeginCommandBuffer(m_cmdBufs[idx], &cbi);

  VkClearValue clearValues[2]{};
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = m_renderPass;
  rp.framebuffer = m_framebufs[idx];
  rp.renderArea = {{0, 0}, m_swapExtent};
  rp.clearValueCount = 2;
  rp.pClearValues = clearValues;
  vkCmdBeginRenderPass(m_cmdBufs[idx], &rp, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(m_cmdBufs[idx], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline);

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

  vkCmdDraw(m_cmdBufs[idx], m_vertexCount, 1, 0, 0);

  // --- Render ImGui ---
  // (NewFrame sudah dipanggil di awal fungsi draw())

  if (m_window->window_buffer.showOption) {
    ImGui::Begin("Options", &m_window->window_buffer.showOption);

    if (ImGui::Checkbox("Mute BGM", &m_bgmMuted)) {
      if (m_bgmMuteCallback) {
        m_bgmMuteCallback(m_bgmMuted);
      }
    }
    ImGui::Spacing();

    // --- Rotation Speed Bar ---
    ImGui::Text("Rotation Speed");
    ImGui::SliderFloat("##speed", &m_rotationSpeed, 0.0f, 10.0f, "%.2f");
    ImGui::Spacing();

    // --- Polygon Sides Slider ---
    ImGui::Text("Polygon Sides");
    int prevSides = m_polygonSides;
    ImGui::SliderInt("##sides", &m_polygonSides, 3, 36);
    if (m_polygonSides != prevSides) {
      rebuildVertexBuffer(m_polygonSides);
    }
    ImGui::Spacing();

    // --- Info ---
    const char *shapeLabel = "Polygon";
    if (m_polygonSides == 3)
      shapeLabel = "Segitiga";
    else if (m_polygonSides == 4)
      shapeLabel = "Segi Empat";
    else if (m_polygonSides == 5)
      shapeLabel = "Pentagon";
    else if (m_polygonSides == 6)
      shapeLabel = "Hexagon";
    else if (m_polygonSides >= 32)
      shapeLabel = "Lingkaran";
    ImGui::Text("Shape: %s (%d sisi)", shapeLabel, m_polygonSides);
    ImGui::Text("Vertices: %u", m_vertexCount);
    ImGui::Text("Edges: %d", m_polygonSides);

    ImGui::End();
  }

  // Select Texture Window
  ImGui::Begin("Select Characters");

  float windowWidth = ImGui::GetContentRegionAvail().x;
  // Calculate button width for 3 buttons, accounting for spacing
  float spacing = ImGui::GetStyle().ItemSpacing.x;
  float buttonWidth = (windowWidth - spacing * 2.0f) / 3.0f;

  if (ImGui::Button("Prabowo", ImVec2(buttonWidth, 0))) {
    changeTexture("wowo_pencinta_sawit.png");
    if (m_characterAudioCallback) {
      m_characterAudioCallback("hidup_jokowi.mp3");
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Gibran", ImVec2(buttonWidth, 0))) {
    changeTexture("gibran.png");
    if (m_characterAudioCallback) {
      m_characterAudioCallback("19-juta-lapangan-pekerjaan.mp3");
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Jokowi", ImVec2(buttonWidth, 0))) {
    changeTexture("jokowi.png");
    if (m_characterAudioCallback) {
      m_characterAudioCallback("mulyono_raja_tipu_tipu.mp3");
    }
  }
  ImGui::End();

  // Score Overlay at Top-Right
  ImGui::SetNextWindowPos(ImVec2(m_swapExtent.width - 20.0f, 20.0f),
                          ImGuiCond_Always, ImVec2(1.0f, 0.0f));
  if (ImGui::Begin(
          "ScoreOverlay", nullptr,
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
              ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
              ImGuiWindowFlags_NoBackground)) {
    if (m_scoreFont)
      ImGui::PushFont(m_scoreFont);
    // Orange color for score
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "SCORE: %d", m_score);
    if (m_scoreFont)
      ImGui::PopFont();
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
// rebuildVertexBuffer – rebuild vertex buffer saat polygon sides berubah
// ===========================================================================
bool VulkanRenderer::rebuildVertexBuffer(int sides) {
  vkDeviceWaitIdle(m_dev);

  // Destroy vertex buffer lama
  if (m_vertBuf) {
    vkDestroyBuffer(m_dev, m_vertBuf, nullptr);
    m_vertBuf = VK_NULL_HANDLE;
  }
  if (m_vertMem) {
    vkFreeMemory(m_dev, m_vertMem, nullptr);
    m_vertMem = VK_NULL_HANDLE;
  }

  m_polygonSides = sides;
  return createVertexBuffer();
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
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
  pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  if (vkCreateDescriptorPool(m_dev, &pool_info, nullptr, &m_imguiPool) !=
      VK_SUCCESS) {
    return false;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  // Load default font first so normal ImGui uses it
  io.Fonts->AddFontDefault();
  // Load custom font at 64px for the score specifically
  {
    auto fontPath = getExeRelativePath("pixantiqua.ttf");
    m_scoreFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 64.0f);
  }

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

void VulkanRenderer::handleMouseClick(int x, int y) {
  int w = m_swapExtent.width;
  int h = m_swapExtent.height;
  if (w == 0 || h == 0)
    return;

  // Convert mouse to NDC
  float ndcX = (2.0f * x) / w - 1.0f;
  float ndcY = (2.0f * y) / h - 1.0f; // Vulkan Y-down

  // View/Proj matrices (same as draw)
  float aspect = (float)w / (float)h;
  Mat4 proj = Mat4::perspective(1.0472f, aspect, 0.1f, 10.0f);
  Mat4 model = Mat4::translation(0.0f, 0.0f, 3.5f) * Mat4::rotationY(m_angle);
  Mat4 mvp = proj * model;

  // Center is (0,0,0) in local space
  float clipX = mvp.m[0][3];
  float clipY = mvp.m[1][3];
  float clipW = mvp.m[3][3];

  float cx = clipX / clipW;
  float cy = clipY / clipW;

  float dx = ndcX - cx;
  float dy = ndcY - cy;
  float distSq = dx * dx + dy * dy;

  // hit threshold ~0.2 in NDC
  if (distSq < 0.2f) {
    m_score++;
    updateScoreTexture();
  }
}

void VulkanRenderer::setCharacterAudioCallback(
    std::function<void(const std::string &)> callback) {
  m_characterAudioCallback = std::move(callback);
}

void VulkanRenderer::setBgmMuteCallback(std::function<void(bool)> callback) {
  m_bgmMuteCallback = std::move(callback);
}

void VulkanRenderer::updateScoreTexture() {
  if (m_texture.cpuPixels.empty() || m_texture.width == 0)
    return;

  // We will copy the original pixels, write the text, and upload it.
  std::vector<uint8_t> buffer = m_texture.cpuPixels;

  // Load raylib example font
  auto fontPath = getExeRelativePath("pixantiqua.ttf");
  FILE *f = fopen(fontPath.c_str(), "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> fontBuffer(size);
    fread(fontBuffer.data(), 1, size, f);
    fclose(f);

    stbtt_fontinfo font;
    if (stbtt_InitFont(&font, fontBuffer.data(), 0)) {
      char text[32];
      snprintf(text, sizeof(text), "SCORE: %d", m_score);

      float scale = stbtt_ScaleForPixelHeight(&font, 64.0f);
      int ascent, descent, lineGap;
      stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
      ascent = (int)(ascent * scale);

      int text_width = 0;
      for (int i = 0; text[i]; ++i) {
        int ax;
        stbtt_GetCodepointHMetrics(&font, text[i], &ax, 0);
        text_width += (int)(ax * scale);
      }

      int padding = 40;
      int x_pos = m_texture.width - text_width - padding;
      int y_pos = padding + ascent;

      for (int i = 0; text[i]; ++i) {
        int w, h, xoff, yoff;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(
            &font, 0, scale, text[i], &w, &h, &xoff, &yoff);
        if (bitmap) {
          for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
              int px = x_pos + x + xoff;
              int py = y_pos + y + yoff;
              if (px >= 0 && px < (int)m_texture.width && py >= 0 &&
                  py < (int)m_texture.height) {
                int idx = (py * m_texture.width + px) * 4;
                uint8_t alpha = bitmap[y * w + x];
                if (alpha > 0) {
                  // simple alpha blend (orange text)
                  buffer[idx + 0] = 255; // R
                  buffer[idx + 1] = 165; // G
                  buffer[idx + 2] = 0;   // B
                  buffer[idx + 3] = 255; // A
                }
              }
            }
          }
          stbtt_FreeBitmap(bitmap, nullptr);
        }
        int ax;
        stbtt_GetCodepointHMetrics(&font, text[i], &ax, 0);
        x_pos += (int)(ax * scale);
      }
    } else {
      std::printf("Failed to init stbtt font\n");
    }
  } else {
    std::printf("ERROR: Failed to open font file at: %s\n", fontPath.c_str());
  }

  // Upload to GPU
  updateTextureSubresource(m_dev, m_physDev, m_cmdPool, m_gfxQueue, m_texture,
                           buffer.data(), m_texture.width, m_texture.height);
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

void VulkanRenderer::changeTexture(const std::string &texturePath) {
  vkDeviceWaitIdle(m_dev);

  // Destroy old texture
  destroyTexture(m_dev, m_texture);

  // Resolve path relative to executable directory
  std::string fullPath = getExeRelativePath(texturePath.c_str());

  // Load new texture
  if (!loadTextureFromFile(m_dev, m_physDev, m_cmdPool, m_gfxQueue,
                           fullPath.c_str(), m_texture)) {
    std::printf("Failed to load new texture: %s\n", fullPath.c_str());
    return;
  }
  m_texturePath = fullPath;

  // Update descriptor set with new texture
  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = m_texture.view;
  imageInfo.sampler = m_texture.sampler;

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = m_descSet;
  descriptorWrite.dstBinding = 1;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(m_dev, 1, &descriptorWrite, 0, nullptr);

  // Re-apply score text to new texture
  updateScoreTexture();
}

// ===========================================================================
// Depth Resources
// ===========================================================================
VkFormat VulkanRenderer::findDepthFormat() {
  VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                           VK_FORMAT_D24_UNORM_S8_UINT};
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_physDev, format, &props);
    if ((props.optimalTilingFeatures &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ==
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      return format;
    }
  }
  std::printf("ERROR: Failed to find supported depth format!\n");
  return VK_FORMAT_D32_SFLOAT;
}

bool VulkanRenderer::createDepthResources() {
  m_depthFormat = findDepthFormat();

  VkImageCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ici.imageType = VK_IMAGE_TYPE_2D;
  ici.extent.width = m_swapExtent.width;
  ici.extent.height = m_swapExtent.height;
  ici.extent.depth = 1;
  ici.mipLevels = 1;
  ici.arrayLayers = 1;
  ici.format = m_depthFormat;
  ici.tiling = VK_IMAGE_TILING_OPTIMAL;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  ici.samples = VK_SAMPLE_COUNT_1_BIT;
  ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(m_dev, &ici, nullptr, &m_depthImage) != VK_SUCCESS) {
    std::printf("ERROR: vkCreateImage for depth failed\n");
    return false;
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(m_dev, m_depthImage, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(m_dev, &allocInfo, nullptr, &m_depthImageMem) !=
      VK_SUCCESS) {
    std::printf("ERROR: vkAllocateMemory for depth failed\n");
    return false;
  }

  vkBindImageMemory(m_dev, m_depthImage, m_depthImageMem, 0);

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_depthImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = m_depthFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_dev, &viewInfo, nullptr, &m_depthImageView) !=
      VK_SUCCESS) {
    std::printf("ERROR: vkCreateImageView for depth failed\n");
    return false;
  }
  return true;
}
