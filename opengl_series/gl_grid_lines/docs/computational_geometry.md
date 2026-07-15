# Analisis Matematika & Computational Geometry

Dokumen ini merupakan penjelasan komprehensif mengenai operasi Aljabar Linear, Kalkulus, dan Algoritma Geometri Komputasional yang telah diimplementasikan dalam kode proyek (pada `mesh.cpp` dan `main.cpp`). Penjelasan ini merujuk pada konsep-konsep dari buku referensi **Computational Geometry: Algorithms and Applications, 3rd Ed. (de Berg et al.)**.

---

## 1. Uji Orientasi (CCW Test) dan Cross Product 2D
Salah satu operasi paling fundamental dalam geometri komputasional (dibahas ekstensif di Bab 1 buku de Berg) adalah **Orientation Test** atau pengecekan arah belokan dari 3 buah titik berurutan: $p, q$, dan $r$.

Dalam kode, fungsi ini diimplementasikan sebagai `Mesh2D::crossProduct(a, b, c)`. 

### Formula Matematika
Untuk mengetahui apakah dari titik $p \rightarrow q \rightarrow r$ berbelok ke kiri (Counter-Clockwise/CCW) atau ke kanan (Clockwise/CW), kita menggunakan determinan dari matriks $3 \times 3$ yang melibatkan koordinat homogen:

$$
\Delta = \det \begin{pmatrix} 1 & p_x & p_y \\ 1 & q_x & q_y \\ 1 & r_x & r_y \end{pmatrix} 
= (q_x - p_x)(r_y - p_y) - (q_y - p_y)(r_x - p_x)
$$

Secara Aljabar Linear, $\Delta$ setara dengan komponen Z dari $Cross Product$ antara vektor $\vec{pq}$ dan $\vec{pr}$. Nilai $\Delta$ ini juga sama dengan **dua kali luas (signed area)** dari segitiga yang dibentuk oleh $p, q, r$.
- Jika $\Delta > 0$, maka urutan berbelok ke **Kiri (CCW)**.
- Jika $\Delta < 0$, maka urutan berbelok ke **Kanan (CW)**.
- Jika $\Delta = 0$, ketiga titik tersebut segaris (Collinear).

---

## 2. Convex Hull (Pembungkus Cembung)
Algoritma Convex Hull (diimplementasikan di `ComputeConvexHull`) dibahas pada **Bab 1 (de Berg)**. Convex Hull dari himpunan titik $P$ adalah poligon cembung terkecil yang mengandung seluruh titik di $P$.

Algoritma yang sering digunakan adalah **Graham Scan** atau **Andrew's Monotone Chain**. Inti kerjanya sangat bergantung pada **Cross Product / Uji Orientasi** di atas:
1. Urutkan titik-titik berdasarkan koordinat X (kemudian Y jika sama).
2. Bangun selubung atas (*Upper Hull*) dan selubung bawah (*Lower Hull*).
3. Saat menambahkan titik baru $p_i$ ke selubung, periksa tiga titik terakhir. Jika belokannya membentuk "sudut cekung" (berbelok ke kanan/CW untuk upper hull), maka titik tengahnya "dibuang". Ini dipastikan menggunakan syarat $\Delta \le 0$.

Proses ini dieksekusi terus hingga selubung sepenuhnya cembung, berjalan dengan kompleksitas waktu $\mathcal{O}(n \log n)$.

---

## 3. Titik di dalam Segitiga (Point-in-Triangle Test)
Fungsi `Mesh2D::isPointInTriangle(p, a, b, c)` digunakan untuk menentukan apakah sebuah titik $p$ berada di dalam segitiga $a, b, c$.

### Penjelasan Half-Plane
Berdasarkan geometri affine, setiap sisi segitiga membelah ruang 2D menjadi dua bidang parsial (*half-plane*). Segitiga adalah irisan dari 3 *half-plane* bagian dalam.
Untuk mengecek apakah $p$ di dalam $\triangle abc$, kita menghitung orientasinya terhadap tiga sisi berarah: $\vec{ab}, \vec{bc},$ dan $\vec{ca}$. 

$$
\Delta_1 = (b_x - a_x)(p_y - a_y) - (b_y - a_y)(p_x - a_x) \\
\Delta_2 = (c_x - b_x)(p_y - b_y) - (c_y - b_y)(p_x - b_x) \\
\Delta_3 = (a_x - c_x)(p_y - c_y) - (a_y - c_y)(p_x - c_x)
$$

Jika $\Delta_1, \Delta_2, \Delta_3$ semuanya ber-tanda sama (semua positif atau semua negatif), maka titik $p$ secara konsisten berada di "sebelah kiri" (atau "sebelah kanan") dari ketiga sisi, yang membuktikan bahwa $p$ **berada persis di dalam batas segitiga**.

---

## 4. Polygon Triangulation (Ear Clipping Method)
Bab 3 pada buku de Berg membahas secara spesifik tentang **Polygon Triangulation** dan kaitannya dengan Teorema Galeri Seni (*Art Gallery Theorem*). Poligon sederhana manapun yang memiliki $n$ titik selalu bisa dipecah menjadi tepat $n - 2$ segitiga.

Algoritma yang kita implementasikan adalah **Ear Clipping** (Pemotongan Telinga). 
Sebuah "Ear" (telinga) adalah segitiga yang dibentuk oleh tiga vertex berurutan $v_{i-1}, v_i, v_{i+1}$ sedemikian sehingga:
1. Sudut di $v_i$ bernilai **cembung** (Strictly Convex). Ini bisa kita tes dengan `crossProduct(v_{i-1}, v_i, v_{i+1}) > 0` asalkan poligon diurutkan secara CCW. Jika nilainya $\le 0$, maka ia cekung (Reflex) dan bukan "telinga".
2. **Tidak ada vertex lain** dari sisa poligon yang jatuh ke dalam area segitiga $\triangle v_{i-1} v_i v_{i+1}$. Ini dibuktikan dengan mengiterasi sisa vertex dan memasukkannya ke dalam `isPointInTriangle`.

Dalam `Mesh2D::processTriangulation`, kita mencari iteratif vertex mana yang memenuhi syarat sebagai "telinga", lalu kita "potong" (simpan segitiganya ke mesh, dan hapus $v_i$ dari sisa poligon). Proses ini diulang hingga hanya tersisa 3 vertex (1 segitiga terakhir). 
Secara matematis, setiap poligon sederhana dengan $n \ge 4$ dijamin minimal memiliki **dua "telinga"**, sehingga algoritma ini pasti akan konvergen. Kompleksitas algoritmanya secara naif (yang kita gunakan) adalah $\mathcal{O}(n^3)$ namun cukup secepat kilat untuk data interaktif UI konvensional.

---

## 5. Menghitung Luas Berarah (Kalkulus & Shoelace Formula)
Di awal fase Triangulasi, kita perlu memastikan orientasi urutan himpunan batas luar (*loop*) selalu dalam kondisi berlawanan arah jarum jam (CCW). Jika arahnya terbalik, algoritma "Ear Clipping" kita akan hancur dan gagal.

Untuk mengetahuinya, kita menghitung total **Signed Area** (Luas Berarah) menggunakan **Shoelace Formula**:
$$ Area = \frac{1}{2} \sum_{i=1}^{n} (x_i y_{i+1} - x_{i+1} y_i) $$

Formula geometrik/aljabar diskrit ini adalah representasi paling harafiah dari **Teorema Green (Green's Theorem)** di dalam Kalkulus Vektor, yang menyatakan keterkaitan integral luas dengan integral garis tertutup sepanjang batas area tersebut:
$$ \iint_D \left( \frac{\partial M}{\partial x} - \frac{\partial L}{\partial y} \right) dx dy = \oint_{\partial D} (L dx + M dy) $$
Dengan substitusi $L = -y$ dan $M = x$, kita mendapatkan luasan $A$:
$$ A = \frac{1}{2} \oint_{\partial D} (x dy - y dx) $$
Dimana, dalam bentuk diskrit sumasi poligon lurus, direalisasikan menjadi formula loop *Shoelace* di `Mesh2D::processTriangulation`. Jika hasilnya negatif (tergantung *axis alignment* layar, umumnya $y$ ke bawah), maka ia CCW, dan kebalikannya jika positif.

---

## 6. Penyisipan Titik Interior (Delaunay-like Face Splitting)
Tantangan terakhir yang diminta adalah memecah geometri yang sudah tergambar ketika ada titik-titik di bagian dalam area, agar semua titik terhubung menjadi jaring mesh utuh (Mirip konsep *Delaunay Triangulation* pada *point cloud*).

Diimplementasikan pada `Mesh2D::insertInteriorPoint`:
1. Poligon awal hanya dibatasi oleh himpunan sisi terluar (*Convex Hull*). Setelah di-*triangulasi*, kita mendapatkan wajah-wajah besar.
2. Untuk sebuah titik interior $P$, kita memanfaatkan pengecekan *Half-Plane* (`isPointInTriangle`) ke **setiap wajah segitiga yang sudah ada**.
3. Saat ditemukan bahwa $P \in \triangle abc$, aljabar topologi memecah segitiga $abc$ menjadi 3 ruang topologi independen:
   - Wajah baru 1: $\triangle abp$
   - Wajah baru 2: $\triangle bcp$
   - Wajah baru 3: $\triangle cap$

Pembelahan spasial rekursif iteratif (spatial subdivision) ini menggaransi kelangsungan kontinuitas jaring (*manifold mesh*), di mana tidak ada titik yang "menggantung" tanpa edge yang saling menghubung. Trik penyisipan (*incremental insertion*) inilah inti utama pembuatan jaring dari titik rambang di *Computational Geometry*.
