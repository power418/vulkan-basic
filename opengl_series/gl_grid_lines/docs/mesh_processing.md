# Pemrosesan Mesh dan Triangulasi (Mesh Processing)

Bagian ini memecah operasi algoritmik berat yang terjadi di dalam modul `Mesh2D` (`mesh.cpp`), terutama mengenai dekomposisi poligon.

## 1. Uji Orientasi (CCW Test) dan Area Geometri
Langkah terpenting di dalam geometri relasional adalah membedakan sisi "dalam" dan "luar" suatu bentuk. Kita menggunakan **Shoelace Formula** (Turunan diskrit dari Teorema Green).
Rumus ini akan mengakumulasi hasil silang dari seluruh titik pada batas luar poligon:
$$ Area = \frac{1}{2} \sum_{i=1}^{n} (x_i y_{i+1} - x_{i+1} y_i) $$
Melalui perhitungan ini, `Mesh2D` bisa dengan yakin mengetahui apakah himpunan titik berputar searah jarum jam atau sebaliknya. Untuk proses pemotongan (Clipping), bentuk poligon wajib dibalik (*reverse*) apabila ia memutar secara *Clockwise*.

## 2. Ear Clipping Algorithm (Triangulasi Poligon)
Sebuah poligon segi-$n$ selalu bisa dibagi rata menjadi $n-2$ segitiga tanpa saling tumpang tindih. Metode yang diadopsi adalah algoritma $\mathcal{O}(n^3)$ **Ear Clipping**.
- Algoritma akan melakukan iterasi pada tiga titik berurutan $A, B, C$.
- Cek Sudut: Menggunakan *Cross Product Determinant*, diperiksa apakah sudut di $B$ membentuk cembungan (Convex) atau cekungan (Reflex).
- Cek Kekosongan: Jika sudutnya cembung, harus dipastikan lagi tidak ada vertex "liar" lain (vertex lain dari sisa poligon) yang masuk dan terjebak di dalam segitiga $\triangle ABC$. (Proses ini memakai uji *Point-in-Triangle*).
- Jika berstatus valid (memenuhi kedua cek), maka segitiga $\triangle ABC$ adalah sebuah "Telinga" (Ear). Segitiga ini dipotong, disimpan menjadi wajah baru (*face*), dan titik $B$ dihapus dari rangkaian sirkuit poligon.
- Proses berulang sampai poligon habis menjadi 1 segitiga final.

## 3. Uji Bidang Titik (Half-Plane / Barycentric)
Untuk menguji apakah titik liar berada di dalam segitiga, `Mesh2D::isPointInTriangle` meminjam logika irisan setengah bidang (Half-Plane Intersection).
Sisi sebuah segitiga membagi ruang 2D secara memanjang tiada batas. Titik $P$ dinyatakan berada murni di "dalam" segitiga jika dan hanya jika $P$ berdiri di sisi yang seragam (misal: di kiri semua) terhadap tiga vektor sisi berarah: $\vec{AB}, \vec{BC}, \vec{CA}$. Konsistensi tanda pada kalkulasi **Cross Product** memastikan kebenaran lokasi spasial ini.

## 4. Partisi Mesh Rekursif (Penyisipan Interior)
Trik arsitektur topologis `Mesh2D::insertInteriorPoint` memungkinkan penyisipan titik (*point insertion*) setelah meshing konvensional selesai.
Ketika vertex $P$ diketahui terjatuh persis di pelukan segitiga $\triangle v_1 v_2 v_3$, segitiga itu dihapus, dan digantikan (di-partisi) menjadi tiga buah segitiga simetris:
1. $\triangle v_1 v_2 P$
2. $\triangle v_2 v_3 P$
3. $\triangle v_3 v_1 P$
Topologi ini merombak jaring tanpa memutus kesinambungan *manifold* dari struktur grafis mesh tersebut.
