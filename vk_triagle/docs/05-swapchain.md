# 5. Swapchain

## 5.1 Konsep

**Apa itu swapchain?**

Kumpulan image buffer yang dirotasi: GPU render ke satu buffer, sementara
buffer lain ditampilkan ke layar. Ini mencegah **tearing** (gambar robek
karena GPU nulis ke buffer yang sedang ditampilkan).

```
Tanpa swapchain (single buffer):
  GPU:  [████████████████] render
  Layar: [████████████████] display
         ↑ GPU render + Layar display SAMA buffer → TEARING!

Dengan swapchain (double buffer):
  Frame 0:  GPU → [Image 0] render  |  Layar ← [Image 1] display
  Frame 1:  GPU → [Image 1] render  |  Layar ← [Image 0] display
            ↑ GPU render + Layar display BEDA buffer → NO TEARING!
```

### Double vs Triple Buffering

```
Double buffering:
  Image 0 → [ GPU render ] → [ Present → Layar ]
  Image 1 → [ GPU render ] → [ Present → Layar ]
             ↑ GPU nunggu present selesai

Triple buffering:
  Image 0 → [ GPU render   ] → [ Present → Layar ]
  Image 1 → [ GPU render   ] → [ Present → Layar ]
  Image 2 → [ GPU (idle)   ] → [ Menunggu ]
             ↑ GPU gak perlu nunggu, bisa langsung render ke Image 2
```

Triple buffering memberikan **latensi lebih rendah** dan **frame rate
lebih stabil**, terutama di beban GPU berat.

## 5.2 Membuat Swapchain

### Step 1: Pilih Format

```cpp
uint32_t fc = 0;
vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &fc, nullptr);
// fc = jumlah format yang didukung surface

std::vector<VkSurfaceFormatKHR> formats(fc);
vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &fc, formats.data());
```

Setiap `VkSurfaceFormatKHR` punya:

```
struct VkSurfaceFormatKHR {
    VkFormat           format;      // misal VK_FORMAT_B8G8R8A8_SRGB
    VkColorSpaceKHR    colorSpace;  // misal VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
};
```

Prioritas kita: `B8G8R8A8_SRGB` + `SRGB_NONLINEAR`:
- Paling umum di Windows
- Warna akurat (monitor modern pakai sRGB)
- Fallback ke format pertama jika tidak ada

### Step 2: Pilih Present Mode

```cpp
uint32_t mc = 0;
vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &mc, nullptr);
std::vector<VkPresentModeKHR> modes(mc);
vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &mc, modes.data());
```

| Mode | VSync | Tearing | Latensi | Wajib Ada? |
|------|-------|---------|---------|------------|
| `FIFO` | Ya | Tidak | Tinggi | **WAJIB** |
| `MAILBOX` | Ya | Tidak | Rendah | Opsional |
| `IMMEDIATE` | Tidak | Ya | Terendah | Opsional |

**Strategi:** Cari `MAILBOX` dulu. Jika tidak ada, pakai `FIFO`.

### Step 3: Pilih Extent (Ukuran)

Extent = ukuran image swapchain dalam pixel.

```cpp
VkSurfaceCapabilitiesKHR caps;
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, surface, &caps);

if (caps.currentExtent.width != UINT32_MAX) {
    // Window menentukan ukuran (paling umum)
    m_swapExtent = caps.currentExtent;
} else {
    // Kita bebas pilih (clamp ke batas hardware)
    int w, h;
    m_window->getFramebufferSize(w, h);
    m_swapExtent.width  = clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
    m_swapExtent.height = clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
}
```

### Step 4: Tentukan Jumlah Image

```cpp
uint32_t imgCount = caps.minImageCount + 1;  // minta 1 extra
if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount)
    imgCount = caps.maxImageCount;
```

- `minImageCount` = minimum (biasanya 2)
- `min + 1` = minta 3 (triple buffering)
- `maxImageCount = 0` = tidak ada batas maksimal

### Step 5: Buat Swapchain

```cpp
VkSwapchainCreateInfoKHR ci{};
ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
ci.surface          = m_window->getSurface();
ci.minImageCount    = imgCount;
ci.imageFormat      = m_swapFmt;
ci.imageExtent      = m_swapExtent;
ci.imageArrayLayers = 1;                    // 1 untuk non-VR
ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
ci.preTransform     = caps.currentTransform; // rotasi/resolusi saat ini
ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
ci.presentMode      = mode;
ci.clipped          = VK_TRUE;
// Jika oldSwapchain != VK_NULL_HANDLE, berarti kita recreate
ci.oldSwapchain     = VK_NULL_HANDLE;

vkCreateSwapchainKHR(m_dev, &ci, nullptr, &m_swapchain);
```

### Step 6: Ambil Image + Buat Image Views

```cpp
vkGetSwapchainImagesKHR(m_dev, m_swapchain, &imgCount, nullptr);
m_swapImages.resize(imgCount);
vkGetSwapchainImagesKHR(m_dev, m_swapchain, &imgCount, m_swapImages.data());

// Image view = bagaimana cara GPU membaca image
for (size_t i = 0; i < imgCount; ++i) {
    VkImageViewCreateInfo vi{};
    vi.image    = m_swapImages[i];
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = m_swapFmt;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    //                        aspect          mip  layer
    vkCreateImageView(m_dev, &vi, nullptr, &m_swapViews[i]);
}
```

**Image view diperlukan** karena Vulkan gak ngasih akses langsung ke
`VkImage`. Kamu harus buat `VkImageView` yang mendefinisikan:
- Bagian image mana yang diakses (mip level, array layer)
- Format interpretasi (bisa berbeda dari format asli)

## 5.3 Swapchain Recreation

### Kapan Diperlukan?

1. **Window di-resize** → extent baru
2. **Window di-minimize lalu di-restore** → swapchain invalid
3. **Monitor berganti** → format mungkin berubah
4. **Fullscreen toggle** → present mode mungkin berubah

### Deteksi

`draw()` mengecek return value:

```cpp
r = vkAcquireNextImageKHR(...);
if (r == VK_ERROR_OUT_OF_DATE_KHR) {
    if (!recreateSwapchain()) {
        return true;     // ← keep loop alive, try again next frame
    }
    return true;         // ← skip rendering; swapchain was just rebuilt
}
if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    return false;        // ← error fatal
}

r = vkQueuePresentKHR(...);
if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    if (!recreateSwapchain()) {
        // keep loop alive; next acquire will handle the stale state
    }
}
```

### Early Guard: Minimisation (Extent 0x0)

Saat window di-minimize, `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`
mengembalikan extent 0x0. Membuat swapchain dengan extent 0x0 akan
gagal (`vkCreateSwapchainKHR` error). Untuk mencegah ini,
`createSwapchain()` punya guard:

```cpp
// Di createSwapchain(), setelah menghitung m_swapExtent:
if (m_swapExtent.width == 0 || m_swapExtent.height == 0) {
    std::printf("WARN: swapchain extent is 0 (window minimised)\n");
    return false;
}
```

Dan di `App::render()`, kita skip `draw()` sama sekali jika framebuffer
size 0:

```cpp
void App::run() {
    while (m_window->pollEvents()) {
        int w = 0, h = 0;
        m_window->getFramebufferSize(w, h);
        if (w == 0 || h == 0) continue;   // ← skip draw saat minimize
        m_renderer->draw();
    }
    // ...
}
```

**Alur pas minimize:**
```
User minimize
  → WM_SIZE(0,0) → m_width=0, m_height=0
  → pollEvents() return true
  → getFramebufferSize → 0,0
  → continue → skip draw()
  → swapchain TIDAK di-touch (aman)

User restore
  → WM_SIZE(800,600) → m_width=800, m_height=600
  → pollEvents() return true
  → getFramebufferSize → 800,600
  → draw() jalan
  → vkAcquireNextImageKHR → VK_ERROR_OUT_OF_DATE_KHR
  → recreateSwapchain() → extent baru 800x600 → sukses
```

### Urutan Recreate

```cpp
void VulkanRenderer::destroySwapchain() {
    vkDeviceWaitIdle(m_dev);   // ← pastikan GPU selesai pakai yang lama

    // Hancurkan dalam urutan terbalik (child dulu, parent terakhir)
    vkFreeCommandBuffers(...);     // tergantung swapchain images
    vkDestroyFramebuffer(...);     // tergantung image views
    vkDestroyImageView(...);       // tergantung swapchain images
    vkDestroySwapchainKHR(...);    // parent-nya
}

bool VulkanRenderer::recreateSwapchain() {
    destroySwapchain();
    return createSwapchain()        // buat baru dengan extent terkini
        && createFramebuffers()     // createFramebuffers dipanggil manual
        && createCommandBuffers();  // record ulang command buffers
}
```

**Catatan:** Jika `createSwapchain()` gagal (misal extent 0x0 karena
minimize), `recreateSwapchain()` return `false`. `draw()` akan return
`true` (bukan false) agar app loop tetap hidup dan coba lagi frame
berikutnya.

### Apa yang Tidak Perlu di-Recreate?

| Objek | Perlu Recreate? | Kenapa? |
|-------|----------------|---------|
| VkInstance | Tidak | Bebas dari ukuran window |
| VkDevice | Tidak | Bebas dari ukuran window |
| VkRenderPass | Tidak | Format swapchain tidak berubah |
| VkPipeline | Tidak | Viewport bisa dynamic |
| VkBuffer (vertex) | Tidak | Data vertex tidak berubah |
| VkCommandPool | Tidak | Hanya container |
| **VkSwapchainKHR** | **Ya** | Ukuran baru |
| **VkImageView** | **Ya** | Image baru |
| **VkFramebuffer** | **Ya** | Image view baru |
| **VkCommandBuffer** | **Ya** | Framebuffer berbeda |

## 5.4 Visualisasi Swapchain

```
Init:
  Swapchain = [Image 0] [Image 1] [Image 2]
               640x480    640x480    640x480

Frame 1: acquire → Image 0  → submit + present → layar
Frame 2: acquire → Image 1  → submit + present → layar
Frame 3: acquire → Image 2  → submit + present → layar
Frame 4: acquire → Image 0  → (Image 0 sudah selesai di-present)

User resize window:

  Swapchain = [Image 0] [Image 1] [Image 2]   ← OUT OF DATE!
               800x600    800x600    800x600

  recreateSwapchain():
    → destroy yang lama
    → buat baru dengan ukuran 800x600

Frame 5: acquire → Image 0 (baru, 800x600) → render → present
```
