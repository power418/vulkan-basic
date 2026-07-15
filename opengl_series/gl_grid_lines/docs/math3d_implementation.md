# Implementasi Matematika 3D (Math3D)

Modul `math3d.h` menopang seluruh fondasi rendering 3D dan simulasi kamera di dalam lingkungan Vulkan/OpenGL. Struktur matematika yang direalisasikan dibangun dari nol (scratch) tanpa dependensi eksternal standar seperti GLM (OpenGL Mathematics).

## 1. Operasi Ruang Vektor (Vec3 & Vec4)
Struktur dasar dari fisika 3D dan rendering grafis bersandar pada aljabar ruang Euclid (Euclidean Space).
- **Penjumlahan/Pengurangan**: Digunakan untuk mentranslasi objek atau menghitung vektor perpindahan antar posisi.
- **Perkalian Skalar**: Mengekspansi (*scaling*) atau menyusutkan jarak vektor.
- **Normalisasi (Normalized)**: Sangat penting dalam rendering optikal. Vektor arah pandang (*Look/Forward Vector*) wajib dinormalisasi (menjadi unit vektor bernilai panjang = 1) melalui pembagian tiap komponen kordinat (x, y, z) dengan fungsi magnitud $\sqrt{x^2 + y^2 + z^2}$.
- **Dot Product & Cross Product**: Produk silang (*Cross Product*) di 3D menghasilkan vektor tegak lurus orthogonal (biasa digunakan untuk mencari vektor Samping / *Right Vector* dan Atas / *Up Vector* pada kalkulasi kamera).

## 2. Struktur Matriks 4x4 (Column-Major)
Komputasi transformasi skala, rotasi, translasi, dan proyeksi tidak bisa dilanjutkan secara praktis jika hanya mengandalkan matriks 3x3 (karena tidak mendukung translasi ruang linier/kordinat homogen). Karenanya, `math3d.h` menggunakan Matriks `4x4`.
Sesuai konvensi kartu grafis (OpenGL API Layout), `Mat4` diimplementasikan secara **Column-Major Array** `m[kolom][baris]`.
Operasi Perkalian Matriks (Matrix Multiplication $\mathcal{O}(n^3)$) juga dilakukan khusus secara *column-major*. Matriks Identitas menjamin bahwa objek yang dikalikan tidak mengalami perubahan apa pun.

## 3. Matriks Transformasi (Model Matrix)
Fungsi `Translate()`, `RotateX()`, dan `RotateY()` membentuk apa yang di grafis komputer disebut *Model Matrix*.
Misalnya fungsi `RotateX(angle)` menyuntikkan fungsi trigonometri `cos(\theta)` dan `sin(\theta)` ke dalam matriks sumbu putar rotasi $X$. Matriks ini dikalikan pada koordinat vertex primitif (titik mesh murni) sebelum masuk ke ruang dunia virtual (*World Space*).

## 4. Matriks Kamera (View Matrix / LookAt)
Implementasi kamera menggunakan rumus **LookAt**:
1. Hitung unit vektor depan $F$ (Forward) dengan mengurangkan target pandangan (*Center*) terhadap mata (*Eye*).
2. Hitung unit vektor samping $S$ (Right) melalui *Cross Product* dari vektor depan $F$ terhadap sumbu absolut dunia $Up$ `(0, 1, 0)`.
3. Hitung unit vektor atas sejati kamera $U$ (Up Camera) dengan melakukan *Cross Product* dari $S$ dan $F$.
Ketiga vektor Basis Orthogonal ini ($S, U, -F$) disusun rapi menjajaki indeks matriks translasi yang diinverskan memakai *Dot Product* (`-Dot(s, eye)`, dll.), memproyeksikan seluruh objek ke koordinat lokal mata kamera (*View Space*).

## 5. Matriks Proyeksi (Perspective Projection)
Transformasi terhebat di grafis 3D adalah pemetaan objek jauh menjadi kecil dan objek dekat menjadi besar (Efek Paralaks/Perspektif).
Fungsi `Perspective(fovY, aspect, nearZ, farZ)` mendefinisikan Piramida Frustum:
- Elemen *Field of View (FOV)* mengubah kemiringan kerucut tangen penglihatan: $f = \frac{1}{\tan(fovY / 2)}$.
- Rentang kedalaman (Near/Far Z-Clipping planes) di-*mapping* agar termuat rapi di dalam ruang kubus semu bernama NDC (Normalized Device Coordinates $[-1, 1]$).
- Trik manipulasi di titik `[2][3] = -1.0f` dan w-division nantinya membuat koordinat $W$ dari vektor grafis menampung nilai $Z$, lalu membaginya (Perspective Divide) oleh sistem perangkat keras GPU untuk distorsi sejati matematis.
