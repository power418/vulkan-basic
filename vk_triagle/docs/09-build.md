# 9. Build System (CMake)

## 9.1 Kenapa CMake?

| Fitur | Manual (CLI) | CMake |
|-------|-------------|-------|
| Cross-platform | Compile beda-beda | 1 file untuk semua OS |
| Compiler detection | Manual setup | Auto-detect MSVC/Clang/GCC |
| Library finding | Cari .lib manual | `find_package()` |
| Multi-config | Double build | 1 config → Debug/Release |
| Presets | N/A | `--preset` untuk switch compiler |

## 9.2 Struktur CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(vulkan-triangle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 1. Find Vulkan SDK ─────────────────────────────────────
find_package(Vulkan REQUIRED)

# ── 2. Target executable ───────────────────────────────────
add_executable(main
    src/main.cpp
    src/app.cpp
    src/args.cpp
    src/shader_compiler.cpp
    src/platform/platform.cpp
    src/platform/win32/window_win32.cpp
    src/platform/winrt/window_winrt.cpp
    src/renderer/vulkan_renderer.cpp
    src/res/app.manifest              # ← manifest file
)

# ── 3. Libraries ───────────────────────────────────────────
target_link_libraries(main PRIVATE
    Vulkan::Vulkan
    runtimeobject.lib                 # WinRT
    comctl32.lib                      # Common Controls
    windowsapp.lib                    # WinRT C++/WinRT
    dwmapi.lib                        # DWM Dark Mode
)

# ── 4. Include dirs & defines ──────────────────────────────
target_include_directories(main PRIVATE src)
target_compile_definitions(main PRIVATE
    VK_USE_PLATFORM_WIN32_KHR         # enable Win32 surface
    WIN32_LEAN_AND_MEAN               # minimal windows.h
    NOMINMAX                          # jangan define min/max macro
)

# ── 5. Shader compilation ──────────────────────────────────
find_program(DXC dxc)
find_program(GLSLC glslc)

if(DXC)
    # ... compile shaders dengan DXC ...
elseif(GLSLC)
    # ... compile shaders dengan GLSLC ...
endif()

# ── 6. Copy SPIR-V ke output dir ──────────────────────────
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:main>/shaders_spv"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.vert.spv"
        "$<TARGET_FILE_DIR:main>/shaders_spv/triangle.vert.spv"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/shaders_spv/triangle.frag.spv"
        "$<TARGET_FILE_DIR:main>/shaders_spv/triangle.frag.spv"
)
```

### Penjelasan Tiap Bagian

**`target_compile_definitions(WIN32_LEAN_AND_MEAN)`**
Tanpa ini, `#include <windows.h>` akan meng-include RIBUAN header yang
sebagian besar tidak dipakai. `WIN32_LEAN_AND_MEAN` mengecualikan:
- COM definitions (kecuali butuh)
- Windows Sockets (kecuali butuh)
- Shell API (kecuali butuh)

**`target_compile_definitions(NOMINMAX)`**
`windows.h` punya macro `min` dan `max` yang bentrok dengan `std::min`
dan `std::max` dari C++ standard library.

**`$<TARGET_FILE_DIR:main>`** adalah **generator expression** —
CMake akan replace ini dengan path folder tempat executable berada:
- MSVC Debug: `build/Debug/`
- MSVC Release: `build/Release/`
- Ninja: `build/`

Ini memastikan SPIR-V selalu tercopy ke folder yang benar, apapun
compiler atau build type-nya.

## 9.3 CMake Presets

File: `CMakePresets.json`

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "msvc-release",
            "generator": "Visual Studio 18 2026",
            "architecture": "x64",
            "binaryDir": "${sourceDir}/build/msvc-release"
        },
        {
            "name": "clang-release",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/clang-release",
            "cacheVariables": {
                "CMAKE_C_COMPILER": "clang-cl",
                "CMAKE_CXX_COMPILER": "clang-cl"
            }
        },
        {
            "name": "mingw-release",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/mingw-release",
            "cacheVariables": {
                "CMAKE_C_COMPILER": "gcc",
                "CMAKE_CXX_COMPILER": "g++"
            }
        }
    ],
    "buildPresets": [
        { "name": "msvc-release", "configurePreset": "msvc-release", "configuration": "Release" },
        { "name": "clang-release", "configurePreset": "clang-release" },
        { "name": "mingw-release", "configurePreset": "mingw-release" }
    ]
}
```

### Cara Pakai Presets

```bash
# Configure pilih compiler
cmake --preset msvc-release
cmake --preset clang-release
cmake --preset mingw-release

# Build
cmake --build --preset msvc-release
cmake --build --preset clang-release
cmake --build --preset mingw-release
```

### Generator: Apa Bedanya?

| Generator | Platform | Kelebihan | Kekurangan |
|-----------|----------|-----------|------------|
| `Visual Studio 18 2026` | Windows | IDE integration, multi-config | Berat, dependensi VS |
| `Ninja` | Semua | Cepat, ringan | Single-config (butuh CMAKE_BUILD_TYPE) |
| `MinGW Makefiles` | Windows (MinGW) | No VS needed | Lebih lambat dari Ninja |

### Single-Config vs Multi-Config

| Aspek | Multi-Config (VS) | Single-Config (Ninja/MinGW) |
|-------|-------------------|---------------------------|
| **Build type** | Dipilih saat build (`--config Release`) | Ditentukan saat configure (`CMAKE_BUILD_TYPE=Release`) |
| **Direktori** | Sama, file .obj beda folder | Berbeda tiap config |
| **Flexibilitas** | Switch config tanpa reconfigure | Butuh reconfigure untuk ganti config |

## 9.4 App Manifest

File: `src/res/app.manifest`

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">

    <!-- Common Controls v6 (modern theming) -->
    <dependency>
        <dependentAssembly>
            <assemblyIdentity
                type="win32"
                name="Microsoft.Windows.Common-Controls"
                version="6.0.0.0"
                publicKeyToken="6595b64144ccf1df"
                language="*"/>
        </dependentAssembly>
    </dependency>

    <!-- DPI Awareness: Per-Monitor V2 -->
    <asmv3:application xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
        <asmv3:windowsSettings>
            <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">
                true
            </dpiAware>
            <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">
                PerMonitorV2, PerMonitor
            </dpiAwareness>
        </asmv3:windowsSettings>
    </asmv3:application>

</assembly>
```

### Kenapa Manifest Diperlukan?

**Tanpa manifest:**
- comctl32 v5 (gaya Windows XP/7) — kotak dialog look lama
- DPI Awareness = None — UI blur di monitor high-DPI

**Dengan manifest:**
- comctl32 v6 — modern theming (ikuti OS theme)
- PerMonitorV2 — UI tetap tajam di monitor 4K

### Embedding

Dengan menambahkan `.manifest` ke `add_executable(...)`, MSVC otomatis
meng-embed manifest sebagai resource. Kamu bisa verifikasi dengan:

```bash
mt.exe -inputresource:main.exe -manifest
```
