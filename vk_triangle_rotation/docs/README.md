# Vulkan Triangle — Dokumentasi Lengkap

```
  ╔══════════════════════════════════════════════════════════════╗
  ║                   Vulkan Triangle Demo                      ║
  ║                                                              ║
  ║  3 vertices  →  Vertex Shader  →  Fragment Shader  →  Screen ║
  ║  (pos + rgb)     (posisi layar)    (warna)                   ║
  ╚══════════════════════════════════════════════════════════════╝
```

## Apa yang Akan Kamu Pelajari

| Topik | File | Konsep Kunci |
|-------|------|--------------|
| Arsitektur modular | `01-architecture.md` | Separation of concerns, interface-based design |
| Platform abstraction | `02-platform.md` | Runtime OS detection, factory pattern |
| Win32/WinRT window | `03-window.md` | Window class, message pump, WinRT integration, dark mode |
| Vulkan instance & device | `04-vulkan-init.md` | Instance creation, physical device selection, logical device |
| Swapchain | `05-swapchain.md` | Triple buffering, format negotiation, resize handling |
| Graphics pipeline | `06-pipeline.md` | Shader modules, vertex input, rasterization, render pass |
| Render loop | `07-rendering.md` | Command buffers, semaphores, fences, GPU-CPU sync |
| Shader compilation | `08-shaders.md` | HLSL, DXC, SPIR-V, build integration |
| Build system | `09-build.md` | CMake presets, multi-compiler, manifest embedding |
| CLI args | `10-cli-args.md` | Simple argument parsing |

## Cara Membaca

1. Mulai dari `01-architecture.md` — pahami dulu bagaimana kode dipisah
2. Lanjut `03-window.md` — bagaimana window dibuat dan event diproses
3. `04-vulkan-init.md` → `05-swapchain.md` → `06-pipeline.md` — Vulkan dari awal
4. `07-rendering.md` — bagaimana semuanya jalan tiap frame
5. Sisanya sesuai kebutuhan (build, shader, CLI)

## Peta File Source

```
src/
├── main.cpp                ← entry point
├── app.hpp / .cpp          ← orchestrator: gabung window + renderer
├── args.hpp / .cpp         ← parser command line
├── shader_compiler.hpp/.cpp← loader SPIR-V dari file
├── res/app.manifest        ← manifest comctl32 v6 + DPI
├── platform/
│   ├── window.hpp          ← abstract class Window
│   ├── platform.hpp / .cpp ← factory: pilih WinRT atau Win32
│   ├── win32/              ← implementasi Win32 (Win7/8.1)
│   └── winrt/              ← implementasi WinRT (Win10+)
└── renderer/
    ├── renderer.hpp        ← abstract class Renderer
    └── vulkan_renderer.hpp/.cpp ← semua Vulkan
```
