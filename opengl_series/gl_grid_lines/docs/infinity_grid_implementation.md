# Blueprint Ruang Grid Tak Terhingga (Infinity Grid)

Modul `infinity_grid.h` mendefinisikan sebuah prototipe/blueprint untuk merealisasikan sistem gerak ruang (*Grid Spatial System*) di dimensi 3D, tanpa ujung dan tanpa batas memori. Karena secara teknis komputer tidak mungkin merender jutaan garis (*lines*) hingga batas horison (infinity), metode komputasinya bersandar total pada kombinasi operasi Rasterisasi Fragment Shader dan Matematika Kamera.

## 1. Arsitektur Blueprint Dasar
Berkas ini membuat *Class* `EndlessGridBlueprint` yang dirancang modular agar mudah disambungkan ke antarmuka utama (`main.cpp`) menggunakan arsitektur pembaruan rutin UI: `UpdateAndRender()`.

Di dalam objek ini, terdapat state krusial untuk simulasi virtual:
- **`m_camera`** : Menggunakan struktur `Camera3D` dari `math3d.h`. Posisi awal diset mengambang di atas tanah, dengan tundukan sudut (*pitch*) tertentu ke bawah menuju grid.
- **`m_dirLight` & `m_pointLight`** : Model pencahayaan (Lighting) sederhana. Arah sorot cahaya (*WorldDirection*) dan intensitas baur/ambien akan diproyeksikan secara simulasi. Animasi melingkar otomatis diterapkan pada lampu (*point light*) dengan rumus orbit sirkular (Trigonometri dasar): 
  - $x = \sin(\text{count})$
  - $z = \cos(\text{count})$

## 2. Bagaimana Infinite Grid Bekerja?
Walau rendering 3D mentah OpenGL belum tersemat (hanya terdapat stub komentar `// draw call OpenGL murni`), metode render "Infinity Grid" di industri modern selalu mengandalkan **Shader Procedural**. Berikut adalah matematis di baliknya:

### a) Layar Sentuh Bidang Tanpa Henti (Full-Screen Quad)
Alih-alih menggambar garis-garis primitif OpenGL (`GL_LINES`) yang terbatas, teknik infinity grid akan me-render **sebuah persegi raksasa (quad)** atau cukup sekadar bidang layar penuh tanpa depth.

### b) Pembalikan Proyeksi (Un-Projection) di Fragment Shader
Di dalam Fragment Shader, perhitungan sinar (Raycast) diluncurkan dari Kamera 3D ke piksel di layar menggunakan inversi dari Matriks Proyeksi (*Projection Matrix*) dan Matriks Penampil (*View Matrix*) dari `math3d.h`.
Sinar (*Ray*) ini kemudian dicari titik potongnya (*Intersection*) dengan lantai dunia sejati (World Ground Plane, umumnya di bidang `y = 0.0`).

### c) Modulo untuk Penentuan Garis (Procedural Texturing)
Ketika titik potong (koordinat dunia 3D nyata) dari piksel didapatkan, kita menerapkan operasi Matematika *Fmod/Modulo*:
$$ 
P_{mod} = \text{mod}(\text{titik\_potong}_{xz}, \text{GridCellSize})
$$
Berdasarkan nilai `m_gridCellSize`, sistem akan memutuskan apakah koordinat spasial tersebut sejajar tebal garis atau sejajar ruang hampa. Jika ya, Shader mewarnainya dengan warna grid; jika tidak, ia transparan (*alpha* = 0). Itulah sebabnya Grid terlihat "tak terhingga"—karena grid itu hanya ilusi gambar di mana lantai ter-proyeksi, bukan batas poligon yang di-*load* ke VRAM.

## 3. Integrasi Kontrol ImGui
Modul ini juga mengaitkan ukuran jala/jaringnya (*Grid Cell Size*) dan posisi kamera menggunakan pustaka IMGUI (`ImGui::SliderFloat`).
Variabel state spasial dan rendering ini disatukan dalam satu fungsi render `RenderGUI()` yang bisa dihidup-matikan secara interaktif (*toggle*) melalui input tombol Spasi (Key Space).
