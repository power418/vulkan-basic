# 7. Render Loop

## 7.1 Konsep Sinkronisasi CPU ↔ GPU

CPU dan GPU adalah dua prosesor yang jalan **independen** (asynchronous).
CPU bisa kirim perintah ke GPU, dan GPU akan menjalankannya sementara CPU
lanjut mengerjakan hal lain.

```
CPU timeline:     Submit ──── Prepare next frame ──── Submit ──── ...
                   │                                      │
                   ▼                                      ▼
GPU timeline:      ──── Render ──── Present ──────── Render ──── Present
```

**Masalah:** GPU mungkin masih render frame N, sementara CPU sudah
mau kirim frame N+1. Kita perlu sinkronisasi.

### 3 Mekanisme Sinkronisasi Vulkan:

| Mekanisme | Siapa yang Nunggu | Untuk Apa |
|-----------|-------------------|-----------|
| **Fence** | **CPU** nunggu GPU | Frame advance, memory cleanup |
| **Semaphore** | **GPU** nunggu GPU | Pipeline dalam GPU (acquire → submit → present) |
| **Barrier** | **GPU** nunggu GPU | Resource transition (layout, write→read) |

```
Fence:     CPU ──→ [kirim perintah] ──→ [bisa nunggu] ──→ CPU lanjut
                             ↑                   ↑
                        vkQueueSubmit      vkWaitForFences

Semaphore: GPU ──→ [Stage A] ──semaphore─→ [Stage B]
                 vkAcquireNext     signal     wait     vkQueuePresent
```

## 7.2 Per-Frame Sync Objects

Untuk menghindari error `VUID-vkQueueSubmit-pSignalSemaphores-00067`
(semaphore dipake ulang sebelum sinyal sebelumnya selesai), kita punya
**array** sync objects — satu set per frame yang boleh in-flight:

```cpp
static constexpr int MAX_FRAMES_IN_FLIGHT = 3;

std::array<VkSemaphore, 3> m_semImage;    // image available
std::array<VkSemaphore, 3> m_semRender;   // render finished
std::array<VkFence, 3>     m_fences;      // CPU-side wait

uint32_t m_currentFrame = 0;   // index yang dipake frame ini
```

### Visualisasi 3 Frame In-Flight

```
Frame 0: CPU → acquire Image 0 → submit [semImage[0]] → present [semRender[0]]
                     fence[0] ↑      ↑
                               │      GPU mulai render Image 0
                               │
Frame 1: CPU → acquire Image 1 → submit [semImage[1]] → present [semRender[1]]
                     fence[1] ↑      ↑
                               │      GPU render Image 1 (bareng Image 0)
                               │
Frame 2: CPU → acquire Image 2 → submit [semImage[2]] → present [semRender[2]]
                     fence[2] ↑      ↑
                               │      GPU render Image 2 (bareng Image 0 & 1)
                               │
Frame 3: CPU → vkWaitForFences(fence[0])  ← nunggu frame 0 selesai
              → reset fence[0]
              → acquire Image 0 (udah siap)
              → submit [semImage[0]] → present [semRender[0]]
```

## 7.3 Draw Function Step-by-Step

```cpp
bool VulkanRenderer::draw() {
```

### Step 1: Tunggu Fence Frame Ini

```cpp
    auto& fence = m_fences[m_currentFrame];
    vkWaitForFences(m_dev, 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_dev, 1, &fence);
```

**Kenapa nunggu?** Frame ini mungkin masih dipake GPU dari siklus
sebelumnya (karena MAX_FRAMES_IN_FLIGHT = 3).

### Step 2: Acquire Next Image

```cpp
    auto& semImg = m_semImage[m_currentFrame];
    uint32_t idx;
    VkResult r = vkAcquireNextImageKHR(m_dev, m_swapchain,
                                        UINT64_MAX,        // timeout
                                        semImg,            // signal ketika image siap
                                        VK_NULL_HANDLE,    // fence (opsional)
                                        &idx);             // index image
```

**Fungsi:** Minta swapchain kasih kita image buffer untuk di-render.

**Perilaku:**
- Jika ada image yang siap → return segera, signal semaphore
- Jika semua image lagi dipake (100% GPU load) → block sampai `UINT64_MAX`
  atau timeout

**Error handling:**
- `VK_ERROR_OUT_OF_DATE_KHR` → swapchain harus di-recreate (resize/minimize)
- `VK_SUBOPTIMAL_KHR` → masih bisa render, tapi idealnya recreate
- Lainnya → fatal error

### Step 3: Submit Command Buffer

```cpp
    auto& semRend = m_semRender[m_currentFrame];

    VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{};
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &semImg;      // nunggu image siap
    si.pWaitDstStageMask    = &waitStage;   // tunggu sampai stage ini
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &m_cmdBufs[idx];  // pakai cmd buffer untuk image ini
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &semRend;     // signal ketika render selesai

    vkQueueSubmit(m_gfxQueue, 1, &si, fence);
```

### Step 4: Present

```cpp
    VkPresentInfoKHR pi{};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &semRend;       // nunggu render selesai
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &m_swapchain;
    pi.pImageIndices      = &idx;

    r = vkQueuePresentKHR(m_presQueue, &pi);
```

### Step 5: Error Handling

```cpp
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        if (!recreateSwapchain()) {
            std::printf("WARN: swapchain recreate failed (minimised?)\n");
            // Keep loop alive; next acquire will handle the stale state
        }
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}
```

**Catatan penting:** Jika `recreateSwapchain()` gagal (misal saat
minimize, extent 0x0), kita tetap return `true` agar app loop hidup.
Return `false` akan menyebabkan crash di frame berikutnya karena
swapchain sudah di-destroy tapi tidak di-recreate.

## 7.4 Command Buffer

Command buffer adalah **daftar perintah yang sudah direkam** dan bisa
dikirim ke GPU berulang kali.

### Recording (Sekali, Saat Init)

```cpp
vkBeginCommandBuffer(cmdBuf[i], &beginInfo);

VkClearValue clear = {{{0.0f, 0.0f, 0.0f, 1.0f}}};  // hitam

vkCmdBeginRenderPass(cmdBuf[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
vkCmdBindPipeline(cmdBuf[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
vkCmdBindVertexBuffers(cmdBuf[i], 0, 1, &m_vertBuf, &offset);
vkCmdDraw(cmdBuf[i], 3, 1, 0, 0);    // 3 vertices, 1 instance
vkCmdEndRenderPass(cmdBuf[i]);

vkEndCommandBuffer(cmdBuf[i]);
```

### vkCmdDraw Parameters

```cpp
vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
//                   3             1            0            0
```

- **vertexCount = 3:** Tiga vertex (segitiga).
- **instanceCount = 1:** Satu segitiga (tidak ada instancing).
- **firstVertex = 0:** Mulai dari vertex ke-0 di buffer.
- **firstInstance = 0:** Instance pertama (hanya 1).

### Kenapa Bisa di-Record Sekali?

Command buffer kita record 1x saat init, lalu submit berulang-ulang
setiap frame. Ini bisa dilakukan karena:

1. **Vertex data statis** — segitiga tidak bergerak.
2. **Clear color tetap** — selalu hitam.
3. **Viewport/scissor** — saat ini hardcoded ke ukuran swapchain
   (akan error pas resize, tapi kita recreate swapchain + cmd buffer).

Untuk aplikasi dinamis (misal: segitiga berputar), command buffer harus
di-record ulang setiap frame atau pake **dynamic state**.

## 7.5 Timeline Diagram — 1 Full Frame

```
CPU: draw()
  │
  ├─ vkWaitForFences           ──→ tunggu frame sebelumnya selesai
  │
  ├─ vkResetFences             ──→ reset fence untuk frame ini
  │
  ├─ vkAcquireNextImageKHR     ──→ minta image dari swapchain
  │    └─ semImage signal      ──→ image siap dipake
  │
  ├─ vkQueueSubmit             ──→ kirim cmd buffer ke GPU
  │    ├─ wait: semImage       ──→ GPU tunggu image siap
  │    ├─ execute command buffer  → GPU render triangle
  │    └─ signal: semRender    ──→ render selesai
  │
  ├─ vkQueuePresentKHR         ──→ minta tampilkan image ke layar
  │    └─ wait: semRender      ──→ GPU tunggu render selesai
  │
  └─ m_currentFrame++          ──→ pindah ke slot sync berikutnya
```

## 7.6 Render Pass

```cpp
VkRenderPassBeginInfo rp{};
rp.renderPass  = m_renderPass;
rp.framebuffer = m_framebufs[idx];     // ← image swapchain idx
rp.renderArea  = {{0, 0}, m_swapExtent};
rp.clearValueCount = 1;

VkClearValue clear = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
rp.pClearValues = &clear;

vkCmdBeginRenderPass(cmdBuf, &rp, VK_SUBPASS_CONTENTS_INLINE);
// ... gambar ...
vkCmdEndRenderPass(cmdBuf);
```

### Clear Value

Setiap frame, framebuffer di-**clear** ke warna hitam:

```cpp
{0.0f, 0.0f, 0.0f, 1.0f}
  R=0   G=0   B=0   A=1
```

Ini terjadi otomatis karena kita set `loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR`
di render pass.

## 7.7 Full Main Loop

```cpp
void App::run() {
    while (m_window->pollEvents()) {       // ← proses event
        int w = 0, h = 0;
        m_window->getFramebufferSize(w, h);
        if (w == 0 || h == 0) continue;    // ← skip draw saat minimize
        m_renderer->draw();                // ← render 1 frame
    }

    // Cleanup: renderer dulu (masih butuh window), baru window
    m_renderer.reset();
    m_window.reset();
}
```

**1 iterasi = 1 frame:**
```
pollEvents() → proses semua event OS
    ↓
getFramebufferSize → 0? → skip draw
    ↓                    ↓
    └→ w > 0 → draw() → fence wait → acquire → submit → present
    ↓
pollEvents() → lagi...
```

**Kenapa skip saat minimize?**
- Surface extent jadi 0x0 → `vkCreateSwapchainKHR` akan gagal
- Swapchain sudah di-destroy di `destroySwapchain()`, tapi create baru
  gagal → state corrupted → crash di frame berikutnya
- Dengan skip, swapchain tetap valid (walaupun out-of-date), dan akan
  di-recreate otomatis saat window di-restore
