# Panduan Debugging Vulkan: "Layar Hitam tapi Program Berjalan"

Salah satu masalah paling umum (dan membuat frustrasi) dalam pemrograman grafis seperti Vulkan adalah fenomena **"Invisible Render" (Tak Kasat Mata)**. Aplikasi berjalan lancar, tidak ada *crash* atau *segmentation fault*, FPS tinggi, namun layar hanya menampilkan warna hitam (atau warna latar).

Berdasarkan *error log* dari proses yang sempat terjadi:
```
Validation Error: [ VUID-VkGraphicsPipelineCreateInfo-layout-07988 ]
vkCreateGraphicsPipelines(): ... shader [VK_SHADER_STAGE_FRAGMENT_BIT] uses descriptor [Set 0, Binding 0, variable "tex"] ... but the stageFlags was VK_SHADER_STAGE_VERTEX_BIT.
```
Ini berarti **Fragment Shader** mencoba membaca data (tekstur/warna), tetapi di kode C++ Anda hanya mengizinkan **Vertex Shader** yang membaca data tersebut. Akibatnya, Fragment Shader tidak mendapat warna apa-apa dan menggambar "ketiadaan" (hitam).

Bagaimana cara mendeteksi masalah "tak kasat mata" seperti ini di berbagai Sistem Operasi? Berikut adalah 2 lapis pertahanan utama yang wajib Anda gunakan.

---

## Lapis 1: Mengaktifkan Vulkan Validation Layers
Validation Layers adalah fitur bawaan Vulkan yang mengawasi setiap baris kode yang Anda kirim ke GPU. Jika Anda salah mengatur memori atau salah *binding*, lapisan ini akan mencetaknya ke Terminal/Console (seperti error di atas).

### Cara Mengaktifkan:
Di dalam C++, saat memanggil `vkCreateInstance`, pastikan Anda menyertakan _layer_ bernama `VK_LAYER_KHRONOS_validation`.
Jika Anda menggunakan *environment variable* dari command line (berguna jika aplikasinya sudah ter-compile), jalankan perintah berikut sebelum membuka eksekusinya:

**Di Windows (Command Prompt / PowerShell):**
```powershell
set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
.\build\clang-debug\main.exe
```

**Di Linux / macOS (Bash / Zsh):**
```bash
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
./build/clang-debug/main
```
*(Catatan: Anda harus sudah menginstal Vulkan SDK agar lapisan ini tersedia di komputer Anda).*

---

## Lapis 2: Menggunakan Frame Debugger (GPU Profiler)
Jika Validation Layers tidak mengeluarkan *error* apa-apa tapi gambar tetap tidak muncul, Anda harus "membedah" GPU untuk melihat apa yang sedang ia gambar di dalam memori VRAM.

### 1. Windows & Linux: Menggunakan **RenderDoc** (Sangat Direkomendasikan)
[RenderDoc](https://renderdoc.org/) adalah standar industri untuk men-debug Vulkan, DirectX, dan OpenGL.
*   **Cara kerja:** Anda membuka RenderDoc, memasukkan path ke eksekusi Anda, lalu klik **Launch**.
*   Tekan tombol `F12` saat aplikasi berjalan untuk menangkap (*capture*) 1 frame.
*   **Keuntungan:** RenderDoc akan memperlihatkan seluruh *Pipeline*! Anda bisa melihat apakah model Anda sebenarnya tergambar tapi ukurannya terlalu kecil, ada di belakang kamera, atau teksturnya gagal dimuat.

### 2. macOS: Menggunakan **Xcode Metal Frame Capture**
Di Apple Silicon (Mac), Vulkan berjalan di atas Metal menggunakan *layer* translasi bernama MoltenVK. Anda tidak bisa menggunakan RenderDoc di Mac.
*   **Cara kerja:** Buka proyek Anda menggunakan **Xcode**.
*   Jalankan program melalui Xcode. Di menu bawah (Debug Bar), klik ikon kamera kecil bertuliskan **Capture GPU Frame**.
*   **Keuntungan:** Sama seperti RenderDoc, Anda bisa memeriksa *Vertex Buffer*, *Fragment Shader*, dan melihat hasil *render pass* secara visual lapis demi lapis.

### 3. Alternatif macOS / Linux (Command Line Profiler)
Jika Anda membutuhkan data mentah lewat terminal di macOS, Anda bisa menggunakan perintah *profiling* Metal dari Apple:
```bash
MTLCapture -o hasil_capture.gputrace ./main
```
Kemudian file `hasil_capture.gputrace` bisa Anda buka di Xcode untuk dianalisis.

---

> **Checklist Singkat Jika Gambar Tidak Muncul:**
> 1. Apakah *Validation Layers* menyala? (Apakah ada tulisan merah di terminal?)
> 2. Apakah *Clear Color* bekerja? (Ganti warna background dari hitam ke merah, jika layarnya jadi merah, berarti present/swapchain aman).
> 3. Cek pembagian memori Shader (seperti masalah *stageFlags* yang Anda alami).
> 4. Gunakan RenderDoc (Windows/Linux) atau Xcode (Mac) untuk melihat 3D model yang tertangkap di luar jangkauan pandangan mata kamera.
