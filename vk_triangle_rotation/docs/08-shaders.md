# 8. Shader Compilation (HLSL → SPIR-V)

## 8.1 Apa Itu SPIR-V?

**SPIR-V** (Standard Portable Intermediate Representation) adalah format
binary universal untuk shader. Bayangkan seperti **assembly untuk GPU**.

```
HLSL / GLSL / Slang
    │
    ▼ kompilasi
SPIR-V  ←─ format yang bisa dibaca Vulkan
    │
    ▼
Driver GPU → Machine code (native GPU)
```

**Kenapa tidak langsung kirim HLSL ke Vulkan?**
- Vulkan tidak mengerti HLSL atau GLSL langsung.
- SPIR-V adalah **intermediate language** yang sudah distandarisasi.
- Driver GPU tinggal terjemahin SPIR-V ke native instruction GPU masing-masing.
- Ini memungkinkan bahasa shader apa pun (HLSL, GLSL, Slang, Rust GPU) asal
  bisa di-compile ke SPIR-V.

## 8.2 HLSL Shader

File: `shaders/triangle.hlsl`

```hlsl
// ── INPUT dari C++ vertex buffer ──────────────────────
struct VSInput {
    float2 position : POSITION;    // location 0, vec2
    float3 color    : COLOR;       // location 1, vec3
};

// ── OUTPUT ke fragment shader ──────────────────────────
struct VSOutput {
    float4 position : SV_Position;  // built-in: posisi layar
    float3 color    : COLOR;        // location 0: interpolated color
};

// ── VERTEX SHADER ─────────────────────────────────────
VSOutput vs_main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.color    = input.color;
    return output;
}

// ── FRAGMENT SHADER ───────────────────────────────────
float4 ps_main(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}
```

### Penjelasan Tiap Bagian

**VSInput.position (POSITION → Location 0)**
Dari C++: `attr[0].location = 0; attr[0].format = R32G32_SFLOAT;`

**VSInput.color (COLOR → Location 1)**
Dari C++: `attr[1].location = 1; attr[1].format = R32G32B32_SFLOAT;`

**SV_Position** adalah semantic khusus. Artinya: "nilai ini adalah posisi
vertex di layar." Vulkan pakai coordinate system:
```
(-1, -1) = pojok kiri bawah viewport
( 1,  1) = pojok kanan atas viewport
( 0,  0) = tengah
```

Kita kirim:
```cpp
Vertex 0: ( 0.0, -0.5) → setengah tinggi, di tengah horizontal
Vertex 1: ( 0.5,  0.5) → kanan atas
Vertex 2: (-0.5,  0.5) → kiri atas
```

Hasilnya: segitiga dengan puncak di (0.0, -0.5).
```
          (0.0, -0.5)
           /\
          /  \
         /    \
        /______\
(-0.5, 0.5)    (0.5, 0.5)
```

**SV_Target** = output fragment shader ke framebuffer (color attachment 0).

## 8.3 Compiler: DXC vs GLSLC

### DXC (DirectX Shader Compiler)

Compiler **resmi** Microsoft untuk HLSL. Bisa output SPIR-V sejak 2018.

```
dxc -T vs_6_0 -E vs_main -spirv triangle.hlsl -Fo triangle.vert.spv
    ↑          ↑         ↑      ↑                ↑
    target     entry     SPIR-V input          output
    profile    point     mode
```

**Target profile** = `vs_6_0` (vertex shader model 6.0) / `ps_6_0` (pixel).

### GLSLC (Google Shader Compiler)

Bagian dari Google `shaderc`. Meskipun namanya "GLSL", dia bisa compile
HLSL juga dengan flag `-x hlsl`.

```
glslc -fshader-stage=vertex -x hlsl --entry-point=vs_main triangle.hlsl -o vert.spv
```

### Perbandingan

| Aspek | DXC | GLSLC |
|-------|-----|-------|
| **Asal** | Microsoft | Google (shaderc) |
| **HLSL support** | Native | Experimental (`-x hlsl`) |
| **SPIR-V output** | Ya | Ya |
| **Kecepatan** | Cepat | Cepat |
| **Fitur Vulkan** | `-fvk-*` flags | Direct |

**Rekomendasi:** DXC untuk HLSL, GLSLC sebagai fallback.

## 8.4 Compile SPIR-V: Dari Mana Asalnya?

### Opsi 1: Build Time (CMake)

```cmake
add_custom_command(
    OUTPUT shaders_spv/triangle.vert.spv
    COMMAND dxc -T vs_6_0 -E vs_main -spirv
            shaders/triangle.hlsl -Fo shaders_spv/triangle.vert.spv
    DEPENDS shaders/triangle.hlsl
)

add_custom_target(compile_shaders
    DEPENDS shaders_spv/triangle.vert.spv
           shaders_spv/triangle.frag.spv)
add_dependencies(main compile_shaders)
```

Setiap kali `triangle.hlsl` berubah, CMake otomatis recompile shader.

### Opsi 2: Manual (compile.bat)

```bat
@echo off
"%VK_SDK%/Bin/dxc.exe" -T vs_6_0 -E vs_main -spirv triangle.hlsl -Fo shaders_spv/triangle.vert.spv
"%VK_SDK%/Bin/dxc.exe" -T ps_6_0 -E ps_main -spirv triangle.hlsl -Fo shaders_spv/triangle.frag.spv
```

### Opsi 3: Runtime Compilation (Future)

Bisa juga compile shader saat runtime pake library `shaderc`:

```cpp
shaderc::Compiler compiler;
auto result = compiler.CompileGlslToSpv(source, shaderc_glsl_vertex_shader, "shader");
// result → vector<uint32_t> siap dipake buat VkShaderModule
```

Ini berguna jika mau terima shader dari user atau format yang fleksibel.

## 8.5 Loading SPIR-V di C++

File: `src/shader_compiler.cpp`

```cpp
std::vector<uint32_t> loadSPIRV(const std::string& path) {
    // Coba dari current working directory
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        return bacaFile(file);
    }

    // Fallback: cari di direktori executable
    std::string exePath = exeDir() + "/" + path;
    std::ifstream file2(exePath, std::ios::binary | std::ios::ate);
    if (!file2.is_open()) {
        printf("ERROR: Tidak bisa buka: %s atau %s\n", path.c_str(), exePath.c_str());
        return {};
    }
    return bacaFile(file2);
}
```

**Dua jalur lookup** penting karena:
- User mungkin run dari folder build (`.\main.exe`)
- User mungkin run dari folder berbeda (`C:\> main.exe`)
- SPIR-V file selalu ikut dengan executable (di-copy oleh CMake POST_BUILD)

### Format Binary SPIR-V

SPIR-V file dimulai dengan **magic number** `0x07230203`:

```
Offset  | Value         | Arti
--------|---------------|------
0       | 0x07230203    | Magic number (SPIR-V)
4       | 0x00010000    | Version 1.0
8       | 0x00000000    | Generator magic
12      | 0x00000018    | Bound (max ID + 1)
16      | 0x00000000    | Reserved
20+     | Instructions  | OpCode + operands
```

Kita baca semua byte dan kasih ke Vulkan sebagai array `uint32_t`.
Vulkan tidak peduli magic number — dia percaya filenya valid.
(SPIR-V validation opsional via `spirv-val` tool).

## 8.6 Verifikasi SPIR-V

Kamu bisa cek isi SPIR-V dengan `spirv-dis`:

```bash
spirv-dis shaders_spv/triangle.vert.spv
```

Output:

```spirv
; SPIR-V
; Version: 1.0
; Generator: Google spiregg; 0
; Bound: 24
; Schema: 0
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %vs_main "vs_main"
                    %in_var_POSITION %in_var_COLOR
                    %gl_Position %out_var_COLOR
               OpDecorate %in_var_POSITION Location 0
               OpDecorate %in_var_COLOR Location 1
               OpDecorate %out_var_COLOR Location 0
               OpDecorate %gl_Position BuiltIn Position
      %float = OpTypeFloat 32
    ...
```

Perhatikan `OpDecorate %in_var_POSITION Location 0` — cocok dengan
`attr[0].location = 0` di C++. Ini kritis.

### Yang Harus Cocok (Pipeline ↔ SPIR-V):

| C++ | SPIR-V |
|-----|--------|
| `attr[0].location = 0` | `Location 0` = `POSITION` |
| `attr[1].location = 1` | `Location 1` = `COLOR` |
| `stages[0].pName = "vs_main"` | `OpEntryPoint Vertex %vs_main "vs_main"` |
| `stages[1].pName = "ps_main"` | `OpEntryPoint Fragment %ps_main "ps_main"` |
