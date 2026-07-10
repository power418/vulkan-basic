# 4. Vulkan Initialization

## 4.1 Prasyarat Vulkan

Sebelum bisa render, kita harus setup:

```
Vulkan Instance  →  koneksi ke driver Vulkan
  ├─ Physical Device  →  GPU yang tersedia
  │    ├─ Queue Family  →  graphics queue (render)
  │    └─ Queue Family  →  present queue (tampilkan ke layar)
  └─ Logical Device  →  interface ke GPU
       ├─ Queue  →  perintah dikirim lewat sini
       ├─ Swapchain  →  kumpulan image untuk ditampilkan
       ├─ Render Pass  →  deskripsi bagaimana render
       ├─ Pipeline  →  shader + state
       ├─ Framebuffers  →  render target per image swapchain
       ├─ Command Buffers  →  perintah render yang sudah direkam
       └─ Sync Objects  →  koordinasi CPU ↔ GPU
```

## 4.2 VkInstance

**Apa itu?** Objek pertama yang harus dibuat. Menghubungkan aplikasi ke
Vulkan loader. Setiap aplikasi biasanya cuma punya 1 instance.

```cpp
bool VulkanRenderer::createInstance(const std::vector<const char*>& exts) {
    VkApplicationInfo app{};
    app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName   = "Vulkan Triangle";
    app.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app.apiVersion         = VK_API_VERSION_1_3;     // minimal API

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &app;
    ci.enabledExtensionCount   = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();

    if (m_validation) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &layers[0];  // "VK_LAYER_KHRONOS_validation"
    }

    vkCreateInstance(&ci, nullptr, &m_instance);
}
```

### Extension vs Layer

| Istilah | Arti | Contoh |
|---------|------|--------|
| **Extension** | Fitur tambahan yang mungkin atau mungkin tidak ada | `VK_KHR_win32_surface` |
| **Layer** | Middleware antara aplikasi dan driver | `VK_LAYER_KHRONOS_validation` |

Extensions **diperlukan** untuk fungsi dasar (surface).
Layers **opsional** untuk debugging.

### Validasi: Apa yang Dicek?

Dengan `--validation`, layer akan memeriksa:

| Pelanggaran | Contoh Error |
|-------------|-------------|
| Handle null | `VkBuffer must not be VK_NULL_HANDLE` |
| Resource in use | `pSignalSemaphores is being signaled but still in use` |
| Format mismatch | `VkImage format does not match VkImageView format` |
| Memory leak | `VkDeviceMemory was not freed` |

## 4.3 VkSurfaceKHR

**Apa itu?** Jembatan antara Vulkan dan window system.

```
   Vulkan               OS
 ┌────────┐          ┌────────┐
 │ Instance │──surface──→│ Window  │
 │          │          │        │
 │  Render  │←present──│  Layar  │
 └────────┘          └────────┘
```

Setiap platform punya extension surface sendiri:

| Platform | Extension | Create Function |
|----------|-----------|-----------------|
| Win32 | `VK_KHR_win32_surface` | `vkCreateWin32SurfaceKHR` |
| WinRT (UWP) | `VK_KHR_winrt_surface` | `vkCreateWinrtSurfaceKHR` |
| X11 | `VK_KHR_xlib_surface` | `vkCreateXlibSurfaceKHR` |
| Wayland | `VK_KHR_wayland_surface` | `vkCreateWaylandSurfaceKHR` |

Di kode kita:

```cpp
VkSurfaceKHR WindowWinRT::createSurface(VkInstance instance) {
    if (m_surface) return m_surface;  // ← caching

    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hinstance = m_hinst;           // ← handle executable
    ci.hwnd      = m_hwnd;            // ← handle window

    vkCreateWin32SurfaceKHR(instance, &ci, nullptr, &m_surface);
    return m_surface;
}
```

**Kenapa surface dibuat oleh window, bukan renderer?** Karena pembuatan
surface butuh informasi platform-specific (HWND, HINSTANCE, X11 display, dll)
yang hanya diketahui oleh window.

## 4.4 VkPhysicalDevice

**Apa itu?** Representasi GPU di sistem. Bisa ada lebih dari satu
(integrated + discrete).

```cpp
bool VulkanRenderer::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    // count = jumlah GPU yang support Vulkan

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (auto d : devices) {
        // Cari queue family yang support graphics
        // Cari queue family yang support present ke surface
        // Cek format dan present mode
        if (semua_kriteria_terpenuhi) {
            m_physDev = d;
            break;
        }
    }
}
```

### Kriteria Pemilihan

1. **Graphics Queue** — GPU harus bisa menjalankan perintah render.
2. **Present Queue** — GPU harus bisa menampilkan gambar ke window.
   (Bisa satu queue untuk kedua fungsi, atau dua queue terpisah.)
3. **Swapchain Formats** — Minimal ada 1 format yang didukung.
4. **Swapchain Present Modes** — Minimal ada 1 mode present.

### Queue Family

Queue family adalah **kelompok** queue yang punya kemampuan sama.
Satu GPU bisa punya beberapa queue family.

```
GPU: Intel Iris Xe
  ├─ Queue Family 0: VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT
  │    └─ Bisa render + compute
  │
  └─ Queue Family 1: VK_QUEUE_TRANSFER_BIT
       └─ Bisa copy data aja
```

Kita cari family yang punya `VK_QUEUE_GRAPHICS_BIT` dan juga support
present ke surface.

## 4.5 VkDevice

**Apa itu?** Logical device — interface yang kita pakai untuk berinteraksi
dengan GPU. Dibuat dari `VkPhysicalDevice`.

```cpp
float priority = 1.0f;

VkDeviceQueueCreateInfo qci{};
qci.queueFamilyIndex = m_gfxFamily;   // family yang dipilih
qci.queueCount = 1;                   // minta 1 queue
qci.pQueuePriorities = &priority;     // prioritas (0.0 - 1.0)

const char* devExts[] = { "VK_KHR_swapchain" };

VkDeviceCreateInfo ci{};
ci.queueCreateInfoCount = (families_unique.size());
ci.pQueueCreateInfos    = queueCreateInfos;
ci.enabledExtensionCount = 1;
ci.ppEnabledExtensionNames = devExts;
ci.pEnabledFeatures = nullptr;        // pakai fitur default

vkCreateDevice(m_physDev, &ci, nullptr, &m_dev);

// Ambil queue yang sudah dibuat
vkGetDeviceQueue(m_dev, m_gfxFamily, 0, &m_gfxQueue);   // render
vkGetDeviceQueue(m_dev, m_presFamily, 0, &m_presQueue);  // present
```

### Queue Priority

Antara 0.0 (rendah) dan 1.0 (tinggi). Mempengaruhi alokasi waktu GPU
jika ada beberapa queue yang aktif.

### Queue Count: 1 atau 2?

Jika graphics family == present family, kita cukup buat 1 queue.
Jika beda, perlu 2 queue (1 untuk render, 1 untuk present).

Kode kita handle ini dengan `std::set<uint32_t>`:

```cpp
std::set<uint32_t> uniqueFamilies = {m_gfxFamily, m_presFamily};
// Kalau sama, size = 1 → 1 queue
// Kalau beda, size = 2 → 2 queue
```

## 4.6 Urutan Init — Kenapa Urutan Ini Penting?

```
 1. createInstance      ← perlu extensions dari window
 2. window->createSurface ← perlu instance
 3. pickPhysicalDevice  ← perlu surface untuk cek present support
 4. createDevice        ← perlu GPU
 5. createSwapchain     ← perlu surface + device
 6. createRenderPass    ← perlu swapchain format
 7. createPipeline      ← perlu render pass + swapchain extent
 8. createFramebuffers  ← perlu swapchain image views + render pass
 9. createVertexBuffer  ← perlu device + queue
10. createCommandBuffers← perlu framebuffers + pipeline
11. createSyncObjects   ← perlu device
```

Setiap langkah bergantung pada hasil langkah sebelumnya. Kamu gak bisa
buat swapchain sebelum punya surface, gak bisa buat render pass sebelum
tahu format swapchain, dst.
