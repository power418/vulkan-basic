# 1. Arsitektur Modular

## 1.1 Kenapa Modular?

Bayangkan kamu punya 3 file dalam 1 folder:

```
app.cpp   ← window code + vulkan code + event loop + argument parsing
```

Kalau semua ada di satu file:
- 1000+ baris susah dibaca
- Mau ganti windowing library? Ganti semua
- Mau ganti ke DirectX? Ganti semua
- Mau testing? Gak bisa pisahin komponen

**Solusi: Modular Architecture = Pisah berdasarkan tanggung jawab.**

```
src/
├── main.cpp              ← entry point (siapa yang pertama jalan)
├── app.hpp / .cpp        ← orchestrator (siapa yang ngatur)
├── args.hpp / .cpp       ← parsing CLI
├── shader_compiler.hpp/.cpp ← baca file SPIR-V
├── platform/              ← semua yang berhubungan dengan OS
│   ├── window.hpp        ← kontrak: "Window harus bisa apa saja"
│   ├── platform.hpp/.cpp ← pabrik: buat window sesuai OS
│   ├── win32/            ← resep untuk Windows < 10
│   └── winrt/            ← resep untuk Windows 10+
└── renderer/
    ├── renderer.hpp      ← kontrak: "Renderer harus bisa apa saja"
    ├── vulkan_renderer/  ← resep untuk Vulkan (/ .cpp)
```

## 1.2 Interface = Kontrak

Interface adalah kelas yang **hanya mendefinisikan method tanpa implementasi**.
Dia seperti kontrak: "siapa pun yang mewarisi aku, WAJIB punya method ini".

```cpp
// src/platform/window.hpp
class Window {
public:
    virtual ~Window() = default;

    virtual bool create(const WindowDesc& desc) = 0;     // buat window
    virtual void destroy() = 0;                           // hancurkan
    virtual bool pollEvents() = 0;                        // proses event (false = tutup)
    virtual VkSurfaceKHR createSurface(VkInstance) = 0;  // buat surface buat Vulkan
    // ... method abstract lainnya
};
```

Tanda `= 0` artinya **pure virtual** — method ini WAJIB diimplement oleh
anak kelas. Ini adalah kontrak.

### Siapa yang Implement?

```
Window  (abstract)
  ├── WindowWin32  → implementasi pakai Win32 API (CreateWindowEx, dll)
  └── WindowWinRT  → implementasi pakai WinRT + comctl32
```

Keduanya **harus** punya method `create()`, `destroy()`, `pollEvents()`, dll.
Tapi cara kerjanya boleh beda.

## 1.3 Factory Pattern

```cpp
// src/platform/platform.cpp
std::unique_ptr<Window> createPlatformWindow() {
    if (IsWindows10OrGreater())
        return std::make_unique<WindowWinRT>();
    else
        return std::make_unique<WindowWin32>();
}
```

Ini disebut **factory** — fungsi yang memutuskan kelas mana yang akan dibuat.
`App::init()` tinggal panggil `createPlatformWindow()` tanpa peduli detailnya:

```cpp
bool App::init() {
    m_window = createPlatformWindow();       // ← factory
    m_window->create(wdesc);                 // ← polimorfisme
    m_renderer = std::make_unique<VulkanRenderer>();
    m_renderer->init(m_window.get(), args);  // ← window sebagai interface
    return true;
}
```

## 1.4 Aliran Data

```
                     App
                   /      \
                  /        \
            Window         Renderer
         (platform)       (Vulkan)
              │               │
              │  createSurface│
              │  getSurface   │
              ├───────────────┤
              │  pollEvents   │
              │  getSize      │
              └───────────────┘
```

- `Window` menyediakan: HWND, message pump, surface Vulkan.
- `Renderer` memakai: surface, ukuran window, event loop.
- `App` menghubungkan: bikin window, kasih ke renderer, jalankan loop.

**Tidak ada yang saling include kode implementasi.** `WindowWinRT` tidak
pernah dipanggil langsung oleh renderer. Renderer cuma lihat `Window*`.

## 1.5 Kenapa Ini Penting?

**Contoh 1: Ganti platform**

Suatu saat kamu mau bikin versi Linux. Tinggal buat:

```
src/platform/linux/
  ├── window_x11.hpp
  └── window_x11.cpp
```

Update factory:

```cpp
std::unique_ptr<Window> createPlatformWindow() {
#ifdef __linux__
    return std::make_unique<WindowX11>();
#elif _WIN32
    if (IsWindows10OrGreater()) return std::make_unique<WindowWinRT>();
    else return std::make_unique<WindowWin32>();
#endif
}
```

**Semua kode Vulkan tetap sama.** Renderer tidak disentuh.

**Contoh 2: Ganti ke DirectX 12**

Buat:

```
src/renderer/d3d12_renderer.hpp
src/renderer/d3d12_renderer.cpp
```

Update App:

```cpp
// m_renderer = std::make_unique<VulkanRenderer>();
m_renderer = std::make_unique<D3D12Renderer>();
```

**Semua kode window tetap sama.** Window tidak disentuh.

## 1.6 Lifecycle Diagram

```
main()
  │
  ├── parseArgs()                     ← baca CLI
  │
  ├── App::init()
  │    ├── createPlatformWindow()     ← factory
  │    ├── window->create()           ← OS-specific
  │    └── renderer->init(window)     ← Vulkan setup
  │         ├── createInstance()
  │         ├── window->createSurface()
  │         ├── pickPhysicalDevice()
  │         ├── createDevice()
  │         ├── createSwapchain()
  │         ├── createRenderPass()
  │         ├── createPipeline()
  │         ├── createFramebuffers()
  │         ├── createVertexBuffer()
  │         ├── createCommandBuffers()
  │         └── createSyncObjects()
  │
  ├── App::run()
  │    └── while (window->pollEvents())
  │         └── renderer->draw()
  │
  └── ~App()
       ├── m_renderer.reset()         ← Vulkan cleanup
       └── m_window.reset()           ← window cleanup
```

**Urutan cleanup penting:**

1. `m_renderer.reset()` — Vulkan masih butuh window untuk destroy surface.
2. `m_window.reset()` — baru window di-destroy.
