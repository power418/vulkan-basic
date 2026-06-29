# 10. Command-Line Arguments

## 10.1 Konsep

`main()` menerima dua parameter dari OS:

```cpp
int main(int argc, char* argv[])
```

- **`argc`** = argument count (jumlah argumen, minimal 1 = nama program)
- **`argv`** = argument vector (array string, `argv[0]` = nama program)

Contoh: `main.exe -v -w 1024`
```
argc = 4
argv[0] = "main.exe"
argv[1] = "-v"
argv[2] = "-w"
argv[3] = "1024"
```

## 10.2 Parser

File: `src/args.cpp`

```cpp
Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {   // mulai dari 1 (skip program name)
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            a.help = true;
        }
        else if (strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-w") == 0) {
            if (i + 1 < argc)
                a.width = std::atoi(argv[++i]);   // ambil nilai berikutnya
        }
        else if (strcmp(argv[i], "--vert") == 0) {
            if (i + 1 < argc)
                a.vertShader = argv[++i];
        }
        else {
            std::printf("Unknown: %s\n", argv[i]);
            a.help = true;   // tampilkan help untuk argumen tidak dikenal
        }
    }
    return a;
}
```

### Kenapa Manual, Bukan Library?

| Library | Kelebihan | Kekurangan |
|---------|-----------|------------|
| Manual (strcmp) | Zero dependency, sederhana | Fitur terbatas |
| getopt (POSIX) | Standard | Tidak di Windows |
| argparse (Python-like) | Kaya fitur | Dependency eksternal |
| CLI11 | Header-only, modern | 2000+ line include |

Untuk proyek kecil dengan 5-6 argumen, manual `strcmp` sudah cukup.
Tidak perlu library kompleks.

## 10.3 Argumen Dengan Nilai

Argumen seperti `--width` punya **nilai** (parameter kedua):

```cpp
else if (strcmp(argv[i], "--width") == 0) {
    if (i + 1 < argc)                    // ← pastikan ada nilai
        a.width = std::atoi(argv[++i]);  // ← consume nilai
    // i sekarang menunjuk ke nilai, loop akan increment ke argumen berikutnya
}
```

**Kenapa `++i` bukan `i+1`?** Kita naikkan `i` agar loop selanjutnya
`i++` dari `for` tidak melewatkan argumen.

## 10.4 Help

```cpp
void printHelp(const char* name) {
    std::printf("Usage: %s [options]\n\n", name);
    std::printf("Options:\n");
    std::printf("  -h, --help              Show this help message\n");
    std::printf("  -v, --validation        Enable Vulkan validation layers\n");
    std::printf("  -w, --width <px>        Window width  (default: 800)\n");
    std::printf("  -H, --height <px>       Window height (default: 600)\n");
    std::printf("  --vert     <path>       Vertex shader SPIR-V path\n");
    std::printf("  --frag     <path>       Fragment shader SPIR-V path\n");
}
```

Output:

```
Usage: main.exe [options]

Options:
  -h, --help              Show this help message
  -v, --validation        Enable Vulkan validation layers
  -w, --width <px>        Window width  (default: 800)
  -H, --height <px>       Window height (default: 600)
  --vert     <path>       Vertex shader SPIR-V path
  --frag     <path>       Fragment shader SPIR-V path
```

## 10.5 Argumen Dari Default ke Final

```
main.exe -w 1024 -H 768 --validation
  │
  ├─ parseArgs()
  │    ├─ arg[0] = "main.exe" (skip)
  │    ├─ arg[1] = "-w"       → width = 1024
  │    ├─ arg[2] = "1024"     → consumed oleh -w
  │    ├─ arg[3] = "-H"       → height = 768
  │    ├─ arg[4] = "768"      → consumed oleh -H
  │    └─ arg[5] = "--validation" → validation = true
  │
  ├─ Args{ width=1024, height=768, validation=true }
  │
  ├─ WindowDesc{ width=1024, height=768 }
  │    └─ window->create(wdesc) → window 1024x768
  │
  └─ VulkanRenderer::init(..., args.validation=true)
       └─ createInstance(extensions, layer="VK_LAYER_KHRONOS_validation")
```

## 10.6 Tabel Argumen

| Argumen | Di Mana Dipakai | Efek |
|---------|----------------|------|
| `--help` | `main.cpp` | Print help, exit 0 |
| `--validation` | `VulkanRenderer` | Enable validation layer |
| `--width` / `-w` | `WindowDesc.width` | Lebar window |
| `--height` / `-H` | `WindowDesc.height` | Tinggi window |
| `--vert` | `Renderer.m_vertPath` | Path vertex SPIR-V |
| `--frag` | `Renderer.m_fragPath` | Path fragment SPIR-V |
