# Metode Numerik 01: Pengantar dan Pemodelan Komputasi

## 1. Konsep Dasar & Filosofi
Pada tahap awal, kita harus membedakan antara **Metode Analitik** (Matematika Murni) dan **Metode Numerik** (Matematika Komputasi):

1. **Metode Analitik (Eksak):** Mencari jawaban yang 100% sempurna dengan manipulasi aljabar atau kalkulus.
   - *Contoh:* Solusi akar $x^2 - 4 = 0 \rightarrow x = \pm 2$.
   - *Kelemahan:* Sangat sulit atau bahkan mustahil diterapkan pada sistem non-linear atau simulasi yang kompleks (seperti fisika asap, fluida, turbulensi).
2. **Metode Numerik (Aproksimasi):** Mencari jawaban **hampiran** (mendekati sempurna) menggunakan operasi aritmatika dasar (+, -, $\times$, $\div$) secara berulang-ulang (*iteratif*).
   - *Keunggulan:* Komputer sangat bodoh dalam kalkulus aljabar tingkat tinggi, tapi sangat jenius dalam berhitung aritmatika jutaan kali per detik.

## 2. Alur Pemecahan Masalah (Dari Fisika ke C++)
Proses mengubah dunia nyata menjadi *software* selalu melewati 4 gerbang ini. Mari kita terjemahkan ke dalam bahasa *programmer*:

### Tahap 1: Masalah Nyata (*Real World Problem*)
Misalnya dalam proyek Anda: Kita ingin mencari titik perpotongan (interseksi) antara kursor *mouse* dengan permukaan *mesh 3D* di layar.

### Tahap 2: Pemodelan Matematika (*Mathematical Model*)
Membuat rumus eksaknya. Dalam matematika, ini adalah persilangan antara persamaan garis (sinar *mouse*) dengan persamaan bidang (*plane/triangle*). Seringkali melibatkan kalkulus dan matriks yang rumit.

### Tahap 3: Formulasi Numerik (*Numerical Formulation*)
Mengubah rumus rumit menjadi sekumpulan aritmatika. Kita merancang **Algoritma**.
"Alih-alih mencari titik temu sempurna, kita akan menebak posisinya, lalu secara bertahap (iterasi) memperbaiki tebakan tersebut."

### Tahap 4: Pemrograman (Implementasi *Code*)
Di sinilah Anda menulis kodenya di C++ / OpenGL.

## 3. Bagaimana Anda Menulis Kode Berdasarkan Alur Ini?
Mari kita lihat *mindset* menulis kodenya. Di dunia nyata (metode analitik), sebuah proses itu berjalan linear A $\rightarrow$ B. Namun di metode numerik (pemrograman), proses itu selalu berbentuk **Loop (Perulangan)**.

Bentuk *skeleton code* (kerangka berpikir) untuk semua metode numerik di C++ Anda akan terlihat seperti ini:

```cpp
#include <iostream>
#include <cmath>

// Tahap 2 & 3: Kita punya fungsi yang menghitung tebakan baru
float TebakLangkahBerikutnya(float tebakanLama) {
    // Berisi aritmatika (+ - * /) rumusan numerik
    return tebakanLama / 2.0f + 1.5f; // (Ini hanya contoh manipulasi angka)
}

void HitungSimulasiNumerik() {
    float solusi_hampiran = 0.0f; // Tebakan awal
    float toleransi_error = 0.0001f;
    float galat = 1.0f; // Set awal > toleransi
    
    int iterasi = 0;
    int max_iterasi = 1000; // Mencegah Infinite Loop (Crash)

    // Tahap 4: Pemrograman (Proses Komputerisasi)
    // Komputer disuruh mengulang-ulang terus sampai jawabannya mendekati sempurna
    while (galat > toleransi_error && iterasi < max_iterasi) {
        float nilai_baru = TebakLangkahBerikutnya(solusi_hampiran);
        
        // Cek seberapa jauh melesetnya dari tebakan sebelumnya
        galat = std::abs(nilai_baru - solusi_hampiran);
        
        solusi_hampiran = nilai_baru; // Update tebakan
        iterasi++;
    }

    std::cout << "Solusi ketemu setelah " << iterasi << " iterasi. Nilai: " << solusi_hampiran << std::endl;
}
```
