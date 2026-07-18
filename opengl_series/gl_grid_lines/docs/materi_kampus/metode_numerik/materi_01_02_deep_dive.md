# Penyelaman Mendalam (Deep Dive): Metode Numerik 01 & 02
*Sebuah Perspektif Filosofis, Akademis, dan Praktis (Level S3)*

Selamat datang di "kelas S3" kita. Di dokumen ini, kita tidak akan sekadar menghafal rumus. Kita akan membedah **MENGAPA** metode numerik itu ada, **BAGAIMANA** pola pikir seorang *Computer Scientist* bekerja dalam ruang simulasi, dan **KAPAN** kita harus menerapkan pedoman ini di dunia nyata (*development*).

---

## Bagian I: Filosofi Metode Numerik (Materi 01)
### 1. Kematian Metode Analitik dan Lahirnya Komputasi
Di bangku sekolah, kita diajarkan **Metode Analitik** (Matematika Murni). Jika kita diberi persamaan kuadrat $x^2 - 4 = 0$, kita tahu pasti solusi *eksak*-nya adalah $x = 2$ atau $x = -2$.

Namun, di dunia nyata (Rekayasa Sipil, Fisika Kuantum, atau Grafika Komputer 3D), alam semesta tidak bekerja seindah rumus di kertas. Bagaimana Anda mencari solusi dari sistem miliaran persamaan yang mendeskripsikan turbulensi air (*Navier-Stokes*), atau men-simulasikan miliaran pantulan cahaya pada *Ray Tracing*? Metode analitik menemui jalan buntu. Komputasi murni tidak bisa diselesaikan dengan selembar kertas.

Di sinilah **Metode Numerik** lahir. Metode numerik adalah **seni berkompromi dengan ketidaksempurnaan**. Kita melepaskan obsesi untuk mencari "jawaban yang 100% sempurna", dan menggantinya dengan "jawaban hampiran (*approximation*) yang cukup baik dan bisa dihitung dengan sangat cepat oleh mesin".

### 2. Alur Pikir Pemodelan (The Grand Process)
Sebagai *Software Engineer*, Anda harus memahami hirarki penciptaan solusi:
1. **Real-World Problem (Masalah Nyata):** Mobil menabrak dinding, atau cahaya memantul dari permukaan kaca.
2. **Pemodelan Matematika:** Mengubah fisika menjadi persamaan diferensial.
3. **Formulasi Numerik:** Mengubah persamaan kalkulus yang tak terhingga menjadi operasi Aritmatika biasa (+, -, *, /) agar prosesor (CPU) bisa mencernanya. CPU hanya mengerti biner dan gerbang logika dasar!
4. **Pemrograman (Algoritma):** Menerjemahkan aritmatika tersebut ke dalam *loop* iterasi di C++ / OpenGL.

**Filosofi Utamanya:** Metode Numerik adalah jembatan bahasa antara "Alam Semesta (Fisika/Matematika)" dengan "Keterbatasan Mesin Komputer (Biner/Transistor)".

---

## Bagian II: Analisis Galat / Error Analysis (Materi 02)
### 1. Anatomi Ketidaksempurnaan Komputer
Mengapa kita perlu mempelajari Galat (Error)? Karena **komputer adalah entitas yang cacat**. 

Dalam matematika analitik, $1 \div 3 = 0.333333...$ (berlanjut tanpa batas hingga keabadian). Tapi di dalam memori RAM komputer Anda, ketidakterbatasan tidak eksis. Memori komputer itu terbatas (misalnya, tipe data `float` di C++ hanya punya 32-bit). Akibatnya, memori akan memotong memori tersebut menjadi angka terbatas: $0.33333334$.

### 2. Dua Musuh Besar Komputasi (Dua Jenis Galat)
Dalam level doktoral, Anda harus bisa mengidentifikasi sumber kehancuran program Anda. Ada dua jenis monster (Galat) yang selalu mengintai kalkulasi *Engine* Anda:

#### A. Galat Pembulatan (*Round-off Error*)
Ini murni **cacat perangkat keras (Hardware Limit)** (standar IEEE 754). Komputer bekerja di basis 2 (Biner), bukan basis 10 (Desimal). Pecahan sederhana seperti $0.1$ di dunia manusia, menjadi pecahan desimal tak berujung di dunia biner: $0.0001100110011...$
Karena dipaksa masuk ke dalam kotak 32-bit, ujungnya akan dibulatkan secara kasar.
- **Kapan ini berbahaya?** Saat menjumlahkan angka yang luar biasa besar dengan angka yang sangat kecil. Atau saat mengecek kesamaan `if (a == b)`.
- **Kasus Nyata Workspace:** Saat mengecek apakah kursor mengenai titik pada *grid* OpenGL Anda, jangan pernah menggunakan `if (titik_x == kursor_x)`. Pasti gagal karena *round-off*. Ubah *mindset* Anda menjadi toleransi epsilon: `if (abs(titik_x - kursor_x) < 0.001)`.

#### B. Galat Pemotongan (*Truncation Error*)
Ini murni **kompromi algoritma (Software Choice)**. Beberapa fungsi seperti Sinus, Cosinus, Logaritma sejatinya menggunakan Deret Taylor yang panjangnya hingga titik tak terhingga ($\infty$).
$$ \cos(x) = 1 - \frac{x^2}{2!} + \frac{x^4}{4!} - \frac{x^6}{6!} + \dots \text{sampai } \infty $$
Komputer tidak bisa menghitung iterasi tak terhingga, maka kita "MEMOTONG" deretnya hanya sampai 4 suku pertama saja. Potongan yang kita buang itulah yang menjadi *Truncation Error*.
- **Kapan ini berguna?** Ini adalah seni *performance vs accuracy* (trade-off). Dalam simulasi grafis *real-time* 60 FPS, kita mungkin berani mentolerir galat pemotongan yang besar (grafik sedikit kurang akurat tapi game berjalan super mulus). Namun, dalam *software navigasi Roket SpaceX*, galat pemotongan ini ditekan seminimal mungkin (suku iterasi diperbanyak) agar roket tidak salah orbit.

### 3. Galat Mutlak vs Galat Relatif
- **Galat Mutlak:** "Penggaris saya meleset 1 cm."
- **Galat Relatif:** Membandingkan seberapa signifikan 1 cm itu? Jika Anda mengukur gedung 100 meter, meleset 1 cm tidak terasa (Galat relatif 0.01%). Tapi jika Anda mengukur ukuran jantung, meleset 1 cm berakibat kematian (Galat relatif luar biasa besar!).
- **Pola Pikir Iterasi (Stopping Condition):** Di komputasi, iterasi pencarian solusi akan terus berjalan `while (true)` dan BARU AKAN BERHENTI (berhenti *looping*) jika $| \text{Galat Relatif} | < \text{Toleransi Maksimal (Epsilon)} $.

---

## Kesimpulan Filosofis (Mindset Development)
Ketika Anda masuk ke tahap *development* dengan pola pikir ini, Anda bukan lagi "tukang ketik kode (Coder)". Anda berubah menjadi **Arsitek Mesin Simulasi**.

**Prinsip yang Harus Dipegang Mulai Sekarang:**
1. **Never trust floating point:** Selalu asumsikan setiap operasi koma di C++ mengandung "racun" *Round-off Error*. Jangan gunakan `==`, gunakan `< epsilon`.
2. **Accept Imperfection:** Dalam memproses algoritma yang rumit (seperti *Mesh Triangulation* atau Pencahayaan *Raycast*), jangan cari rumus analitik eksak yang menyiksa CPU. Gunakan hampiran iteratif! Selama mata manusia tidak bisa melihat galatnya (*Galat Relatif* sangat kecil), maka algoritma tersebut **SUKSES**.
3. **Master the Loop:** Semua komputasi grafis dan *physics* di OpenGL Anda pada dasarnya hanyalah satu buah iterasi tak terhingga (`while (!glfwWindowShouldClose)`), yang secara perlahan menebak (*guessing*) kondisi alam semesta buatan Anda, mengoreksi galatnya di setiap detik, dan menggambarnya di layar monitor. 

Itulah esensi sejati dari *Metode Numerik*. Tarian iteratif untuk menjinakkan matematika tak terhingga ke dalam perangkat keras yang fana.
