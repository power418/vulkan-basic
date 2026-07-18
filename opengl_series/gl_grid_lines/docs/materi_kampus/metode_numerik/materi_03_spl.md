# Metode Numerik 03: Sistem Persamaan Linear (SPL) - Kisah Matriks dan Eliminasi

Bila Bab 01 dan 02 mengajari kita cara mencari *satu* kebenaran dari ketidaksempurnaan komputer, Bab 03 membawa kita pada teka-teki yang lebih kompleks. Bagaimana jika kita punya *banyak masalah* yang saling terhubung dan harus diselesaikan secara bersamaan?

Inilah yang dinamakan **Sistem Persamaan Linear (SPL)**.

## 1. Alur Cerita: Misteri Variabel yang Saling Terikat
Bayangkan Anda adalah seorang detektif komputer. Anda melihat bayangan objek 3D di layar (*OpenGL Engine* Anda). Titik kursor *mouse* menembak sebuah sinar masuk ke dalam layar (*Raycasting*). Anda bertanya pada program: *"Di koordinat X, Y, Z mana sinar saya membentur segitiga ini?"*

Satu persamaan saja tidak cukup. Sumbu X bergantung pada Y, Y bergantung pada Z, dan Z bergantung pada batas segitiga. Masalah ini **saling mengunci**. Di matematika, kita menulisnya sebagai:
$$ a_{11}x_1 + a_{12}x_2 + a_{13}x_3 = b_1 $$
$$ a_{21}x_1 + a_{22}x_2 + a_{23}x_3 = b_2 $$
$$ a_{31}x_1 + a_{32}x_2 + a_{33}x_3 = b_3 $$
Di mana $x_1, x_2, x_3$ adalah tersangka (variabel) yang dicari.

Trik pertama seorang *Computer Scientist* adalah melepaskan semua $x$ dan menaruh angkanya dalam sebuah **Penjara Matriks** (*Augmented Matrix*):
$$
\begin{bmatrix}
a_{11} & a_{12} & a_{13} & | & b_1 \\
a_{21} & a_{22} & a_{23} & | & b_2 \\
a_{31} & a_{32} & a_{33} & | & b_3 
\end{bmatrix}
$$

Sekarang, kita hanya perlu memanipulasi angka-angka di atas menggunakan bahasa mesin (C++).

---

## 2. Teknik Pemusnahan Pertama: Eliminasi Gauss

**Filosofi:**
Cara termudah menangkap 3 penjahat yang saling menutupi adalah dengan **mengeliminasi alibinya satu per satu**. Kita ingin mengubah matriks penuh menjadi **Matriks Segitiga Atas**. Kita membunuh (membuat nol) semua elemen di bawah garis miring (diagonal utama).

$$
\begin{bmatrix}
c_{11} & c_{12} & c_{13} & | & d_1 \\
0      & c_{22} & c_{23} & | & d_2 \\
0      & 0      & c_{33} & | & d_3 
\end{bmatrix}
$$

Setelah bagian bawah menjadi `0`, variabel $x_3$ kehilangan perlindungan. 
Dari baris terbawah, kita bisa tahu secara langsung:
$$ c_{33}x_3 = d_3 \quad \rightarrow \quad x_3 = \frac{d_3}{c_{33}} $$

Karena $x_3$ sudah ketahuan, kita **berjalan mundur** (*Substitusi Mundur* / *Back Substitution*) ke baris kedua untuk mencari $x_2$, lalu ke baris pertama mencari $x_1$.

### Rumus Operasi Baris Elementer (OBE)
Untuk membuat baris menjadi nol, kita memakai rumus Penggantian Baris:
$$ \text{Baris}_{target} = \text{Baris}_{target} - ( \text{Rasio} \times \text{Baris}_{pivot} ) $$
Dimana Rasio ($m$) adalah koefisien yang ingin dinolkan dibagi dengan koefisien diagonal di atasnya.

### Implementasi Eliminasi Gauss di C++

```cpp
#include <iostream>
#include <vector>

using namespace std;

// Anggap kita mencari x, y, z dari 3 persamaan
int n = 3;

// Augmented Matrix [A | b]
float matriks[3][4] = {
    { 2,  4, -2,  2 },
    { 4,  9, -3,  8 },
    {-2, -3,  7, 10 }
};

void EliminasiGauss() {
    // 1. TAHAP ELIMINASI MAJU (Membuat Segitiga Bawah menjadi 0)
    for (int pivot = 0; pivot < n - 1; pivot++) {
        for (int baris = pivot + 1; baris < n; baris++) {
            // Hitung rasio pembunuhan (m)
            float rasio = matriks[baris][pivot] / matriks[pivot][pivot];
            
            // Lakukan Operasi Baris Elementer (OBE) pada seluruh kolom di baris tersebut
            for (int kolom = 0; kolom <= n; kolom++) {
                matriks[baris][kolom] -= rasio * matriks[pivot][kolom];
            }
        }
    }

    // 2. TAHAP SUBSTITUSI MUNDUR (Mencari Jawaban dari Bawah ke Atas)
    vector<float> solusi(n);
    for (int i = n - 1; i >= 0; i--) {
        solusi[i] = matriks[i][n]; // Ambil hasil paling kanan (kolom b)
        for (int j = i + 1; j < n; j++) {
            solusi[i] -= matriks[i][j] * solusi[j]; // Kurangi dengan variabel yang sudah diketahui
        }
        solusi[i] = solusi[i] / matriks[i][i]; // Bagi dengan koefisien miliknya sendiri
    }

    // Cetak Solusi
    cout << "Solusi x1: " << solusi[0] << endl;
    cout << "Solusi x2: " << solusi[1] << endl;
    cout << "Solusi x3: " << solusi[2] << endl;
}
```

---

## 3. Teknik Pemusnahan Ultimate: Eliminasi Gauss-Jordan

**Filosofi:**
Seseorang bernama Wilhelm Jordan berkata pada Carl Friedrich Gauss: *"Kenapa kita harus repot-repot berjalan mundur (*Substitusi Mundur*) setelah membuat segitiga atas? Kenapa kita tidak sekalian saja bunuh (buat nol) angka-angka di ATAS diagonal juga?!"*

Gauss-Jordan mengubah matriks awal bukan hanya menjadi segitiga atas, tapi menjadi **Matriks Identitas**. Semua elemen selain diagonal utama ditembak mati menjadi nol, dan diagonal utama dipaksa menjadi angka `1`.

Hasil Akhirnya langsung berwujud jawaban telanjang:
$$
\begin{bmatrix}
1 & 0 & 0 & | & x_1 \\
0 & 1 & 0 & | & x_2 \\
0 & 0 & 1 & | & x_3 
\end{bmatrix}
$$

### Perubahan C++ untuk Gauss-Jordan
Kodenya lebih agresif karena *looping*-nya menyerang **semua baris** (baik di atas maupun di bawah pivot), kecuali baris pivot itu sendiri.

```cpp
void EliminasiGaussJordan() {
    for (int pivot = 0; pivot < n; pivot++) {
        // Normalisasi: Bagi seluruh baris pivot dengan koefisien diagonalnya agar menjadi 1
        float pembagi_utama = matriks[pivot][pivot];
        for (int kolom = 0; kolom <= n; kolom++) {
            matriks[pivot][kolom] /= pembagi_utama;
        }

        // Eliminasi SEMUA baris selain baris pivot agar elemen kolom pivotnya jadi 0
        for (int baris = 0; baris < n; baris++) {
            if (baris != pivot) {
                float rasio = matriks[baris][pivot];
                for (int kolom = 0; kolom <= n; kolom++) {
                    matriks[baris][kolom] -= rasio * matriks[pivot][kolom];
                }
            }
        }
    }

    // Solusi langsung tersedia di kolom terakhir! Tanpa Substitusi Mundur.
    cout << "Gauss-Jordan langsung dapat jawaban:" << endl;
    cout << "x1 = " << matriks[0][n] << endl;
    cout << "x2 = " << matriks[1][n] << endl;
    cout << "x3 = " << matriks[2][n] << endl;
}
```

## 4. Kapan Kita Memakainya di *Workspace Engine* Ini?
Jika Anda nantinya memprogram *Physics Collision* atau *Skeletal Animation* untuk benda 3D, seringkali Anda punya *matrix transformation* yang sangat besar. 
- Jika Anda **butuh efisiensi komputasi CPU murni**, eliminasi Gauss konvensional (Segitiga Atas + Substitusi mundur) sebenarnya sedikit lebih cepat.
- Namun jika Anda **menulisnya di GPU (Shader / CUDA)**, atau mencari invers matriks (Matrix Inversion) `mat4::inverse()`, algoritma modifikasi **Gauss-Jordan** adalah standar industri yang tak terkalahkan. Di dalam *library GLM / OpenGL Math* yang ada di dalam *engine* Anda, ada algoritma ini yang tertanam dalam-dalam untuk memecahkan rotasi kamera Anda!
