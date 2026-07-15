# Alur Geometri (Geometry Flow)

Dokumen ini menjelaskan alur data geometri (Geometry Data Flow) dalam proyek ini, mulai dari input pengguna hingga menjadi struktur geometri yang siap di-render.

## 1. Input dan Interaksi (Point Acquisition)
Pada mode 2D, pengguna berinteraksi langsung melalui antarmuka layar (Canvas). Fungsi transformasi affine `WorldToScreen` (dan kebalikannya) digunakan untuk memetakan koordinat layar kursor menjadi koordinat *Virtual World* 2D.
Setiap kali pengguna mengklik layar, titik baru (tipe `Point2D`) diregistrasi ke dalam memori aplikasi.

## 2. Pembangkitan Pembatas Luar (Convex Hull)
Himpunan titik bebas (*point cloud*) yang dikumpulkan tidak memiliki topologi batas. Untuk membungkus semua titik menjadi sebuah poligon, kita menggunakan algoritma komputasional **Convex Hull** (yang direalisasikan dengan **Andrew's Monotone Chain** dalam berkas `geometry.cpp`).
- Fungsi ini mengurutkan titik secara leksikografis.
- Menggunakan *Cross Product* untuk mendeteksi belokan yang membentuk bagian atas (*upper hull*) dan bawah (*lower hull*).
- Hasilnya adalah sebuah sub-himpunan titik yang berurutan (melingkar/Counter-Clockwise) yang menjadi "kulit" atau batas terluar bidang.

## 3. Proses Inisialisasi Mesh 2D
Titik-titik dari *Convex Hull* ini kemudian dipindahkan menjadi topologi tepi (*edges*) di dalam struktur `Mesh2D`. 
`Mesh2D` akan menerima instruksi:
```cpp
mesh.addEdge(v1, v2);
```
Setiap kali tepi dimasukkan, *Mesh* akan melacak graf internal. Ketika himpunan tepi ini membentuk satu lingkaran tertutup (Polygon), *Mesh* akan mendeteksi siklus (*Cycle Detection*).

## 4. Pembentukan Bidang (Triangulasi)
Setelah batas luar terbentuk (menjadi sirkuit tertutup), `Mesh2D` otomatis mengubah lubang besar tersebut menjadi jaringan segitiga (*Triangle Faces*) agar dapat digambar oleh kartu grafis (GPU). (Detail tentang triangulasi dapat dibaca pada dokumen `mesh_processing.md`).

## 5. Integrasi Titik Interior (Hole Filling)
Karena Convex Hull hanya memproses batas terluar, maka titik-titik yang posisinya berada di dalam area (*interior points*) belum tersambung ke dalam jaringan. 
Oleh sebab itu, alur terakhir dari sistem geometri kita adalah melakukan pengecekan satu per satu pada titik yang tertinggal. Titik-titik ini dipaksa masuk dengan memecah segitiga besar yang menaungi titik tersebut menjadi 3 segitiga baru, menciptakan kepadatan jaring di bagian dalam (*Delaunay-like Splitting*).

Setelah tahapan 5 selesai, topologi `Mesh2D` (terdiri dari *Vertex Buffer* dan *Index/Face Buffer*) sudah final secara geometri dan diteruskan ke *renderer* (dalam hal ini `ImGui::AddTriangleFilled` atau OpenGL rasterizer).
