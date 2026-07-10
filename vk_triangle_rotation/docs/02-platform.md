# 2. Layer Abstraksi Platform

## 2.1 Konsep

Platform layer adalah jembatan antara aplikasi dan sistem operasi. Tugasnya:

1. **Membuat window** — register class, create window, tampilkan.
2. **Message pump** — proses event OS (keyboard, mouse, close, resize).
3. **Vulkan surface** — sediakan VkSurface untuk rendering.
4. **Theme integration** — dark/light mode sesuai OS.

Semua ini dibungkus dalam interface `Window`.

## 2.2 Interface Window

File: `src/platform/window.hpp`

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct WindowDesc {
    std::string title = "Vulkan";
    int width  = 800;
    int height = 600;
};

class Window {
public:
    virtual ~Window() = default;

    // ── Lifecycle ─────────────────────────────────────────
    virtual bool create(const WindowDesc& desc) = 0;
    virtual void destroy() = 0;
    virtual bool pollEvents() = 0;   // false = user minta tutup

    // ── Vulkan Surface ────────────────────────────────────
    virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;
    virtual VkSurfaceKHR getSurface() const = 0;
    virtual void destroySurface(VkInstance instance) = 0;

    // ── Info ──────────────────────────────────────────────
    virtual void getFramebufferSize(int& w, int& h) = 0;
    virtual std::vector<const char*> getRequiredInstanceExtensions() const = 0;
};
```

### Penjelasan Tiap Method

**`create(desc)`**
Membuat window sebenarnya. Parameter:
- `title`: judul di title bar.
- `width` / `height`: ukuran **client area** (bukan window termasuk border).
Return `false` jika gagal.

**`destroy()`**
Menghancurkan window. Dipanggil di destructor atau manual.

**`pollEvents()`**
Proses semua event yang menunggu di queue OS. Return `false` jika user
menutup window (klik X, Alt+F4). Ini adalah sinyal untuk keluar dari
render loop.

**`createSurface(instance)`**
Membuat `VkSurfaceKHR` yang menghubungkan Vulkan ke window. Setiap
platform punya cara beda:
- Win32: `vkCreateWin32SurfaceKHR(instance, &win32info, ...)`
- WinRT: `vkCreateWinrtSurfaceKHR(instance, &winrtinfo, ...)`
- X11: `vkCreateXcbSurfaceKHR(instance, &x11info, ...)`

Method ini **caching** — panggil sekali, panggil lagi return yang sama.

**`getSurface()`**
Return surface yang sudah dibuat. Dipanggil oleh renderer.

**`destroySurface(instance)`**
Hancurkan surface. Dipanggil saat renderer cleanup.

**`getFramebufferSize(w, h)`**
Ukuran **client area** saat ini. Berguna untuk viewport dan scissor.

**`getRequiredInstanceExtensions()`**
Extension yang diperlukan VkInstance agar bisa membuat surface di
platform ini:
- Win32: `["VK_KHR_surface", "VK_KHR_win32_surface"]`
- WinRT: `["VK_KHR_surface", "VK_KHR_winrt_surface"]`

## 2.3 Factory — Pemilihan Platform Runtime

File: `src/platform/platform.cpp`

```cpp
std::unique_ptr<Window> createPlatformWindow() {
    if (IsWindows10OrGreater()) {
        std::printf("Platform: Windows 10+ → WinRT window\n");
        return std::make_unique<WindowWinRT>();
    }
    std::printf("Platform: < Windows 10 → Win32 window\n");
    return std::make_unique<WindowWin32>();
}
```

### `IsWindows10OrGreater()` — Bagaimana Cara Kerjanya?

Fungsi ini dari `VersionHelpers.h` (header Windows SDK). Dia cek versi OS
dengan `RtlGetVersion()`:

```
Windows 10 build 10240+ → IsWindows10OrGreater() = true
Windows 8.1             → IsWindows10OrGreater() = false
Windows 7               → IsWindows10OrGreater() = false
```

**Kenapa runtime, bukan compile-time?**

| Approach | Kelebihan | Kekurangan |
|----------|-----------|------------|
| **Compile-time** (`#ifdef _WIN32`) | Sederhana | Satu binary hanya untuk satu platform |
| **Runtime** (`IsWindows10OrGreater`) | Satu binary untuk semua versi | Kode kedua platform ikut ter-compile |

Dengan runtime detection, kamu bisa compile satu `.exe` dan jalan di
Windows 7, 8.1, 10, 11 — masing-masing pake implementasi yang sesuai.

## 2.4 Perbandingan Implementasi

| Fitur | WindowWin32 | WindowWinRT |
|-------|-------------|-------------|
| **WinRT init** | `—` | `winrt::init_apartment()` |
| **comctl32** | Dari manifest | `InitCommonControlsEx()` langsung |
| **Window class** | `RegisterClass` | `RegisterClass` |
| **Create window** | `CreateWindowEx` | `CreateWindowEx` |
| **Event loop** | `PeekMessage` → `DispatchMessage` | Sama |
| **Surface** | `vkCreateWin32SurfaceKHR` | Sama (desktop app) |
| **Theme** | — | `DwmSetWindowAttribute(20)` + WinRT events |
| **Libraries** | `windows.h` | `winrt/*.h`, `dwmapi.h`, `comctl32.lib` |

> **Penting:** Keduanya tetap pakai `CreateWindowEx` + `vkCreateWin32SurfaceKHR`
> karena aplikasi desktop Windows selalu butuh HWND. Yang berbeda hanyalah
> **inisialisasi tambahan** (WinRT, comctl32, theme).
