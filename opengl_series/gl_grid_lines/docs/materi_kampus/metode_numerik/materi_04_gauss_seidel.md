# Metode Numerik 04: Iterasi Gauss-Seidel - Menebak Hingga Sempurna

Melanjutkan cerita dari Bab 03, kita sudah memiliki senjata bernama **Eliminasi Gauss** dan **Gauss-Jordan**. Keduanya sangat akurat untuk memecahkan Sistem Persamaan Linear (SPL). Tapi, tahukah Anda bahwa industri *game engine* modern seringkali **menolak** menggunakan Gauss-Jordan?

## 1. Alur Cerita: Kelemahan Sang Eksekutor Langsung
Bayangkan Anda sedang membuat simulasi air (*Fluid Dynamics*) atau tabrakan tumpukan kardus (*Rigid Body Physics*) di dalam *OpenGL engine* kita. Masalah komputasi fisika ini melibatkan **jutaan titik** yang saling bertabrakan.
Ini berarti matriks Anda berukuran $1.000.000 \times 1.000.000$.

Jika kita menggunakan Eliminasi Gauss-Jordan:
1. Waktu komputasi sangat lambat (membutuhkan iterasi $N^3$). Game Anda akan *lag* hingga 0.001 FPS.
2. Ingat "Galat Pembulatan" di Bab 02? Mengalikan dan mengurangi jutaan baris pecahan *float* secara berurutan akan menumpuk galat pembulatan hingga ke titik di mana **jawaban akhir Anda sepenuhnya salah / hancur**.

Di sinilah **Metode Gauss-Seidel** masuk sebagai pahlawan penyelamat. Alih-alih mencoba mencari jawaban dengan "pasti" dan "langsung" (metode eksak/langsung), Gauss-Seidel kembali ke filosofi suci metode numerik: **Menebak dan Mengoreksi (Iteratif).**

---

## 2. Filosofi: Jangan Dihitung, Tapi Ditebak!
Jika kita punya persamaan:
$$ a_{11}x_1 + a_{12}x_2 + a_{13}x_3 = b_1 $$
$$ a_{21}x_1 + a_{22}x_2 + a_{23}x_3 = b_2 $$
$$ a_{31}x_1 + a_{32}x_2 + a_{33}x_3 = b_3 $$

Langkah Gauss-Seidel sangat *hacker-mindset*:
1. "Pecahkan" paksa persamaan pertama supaya hanya menyisakan $x_1$ di sebelah kiri. Lakukan hal yang sama untuk $x_2$ dan $x_3$.
   $$ x_1 = \frac{b_1 - a_{12}x_2 - a_{13}x_3}{a_{11}} $$
   $$ x_2 = \frac{b_2 - a_{21}x_1 - a_{23}x_3}{a_{22}} $$
   $$ x_3 = \frac{b_3 - a_{31}x_1 - a_{32}x_2}{a_{33}} $$

2. Anggap saja kita tidak tahu jawabannya. Berikan tebakan ngawur: $x_1 = 0, x_2 = 0, x_3 = 0$.
3. Masukkan tebakan ngawur itu ke rumus $x_1$ yang baru. Anda akan dapat tebakan $x_1$ yang "sedikit lebih pintar".
4. **(Kunci Utama Gauss-Seidel):** Langsung gunakan tebakan $x_1$ yang baru ini untuk mencari $x_2$. Jangan pakai tebakan lama! Gunakan data paling *fresh* sebisa mungkin.
5. Ulangi proses ini terus menerus (Iterasi). Ajaibnya, angka-angka ini akan secara perlahan "bergerak" menuju jawaban yang benar (*Konvergen*).

---

## 3. Implementasi C++: Jauh Lebih Elegan
Kode untuk Gauss-Seidel terbukti jauh lebih pendek, tidak butuh alokasi memori yang masif, dan kebal terhadap penumpukan *round-off error*. Syarat berhentinya? Tentu saja kita gunakan rumus **Galat Relatif Hampiran ($\epsilon_{RA}$)** dari Bab 02!

```cpp
#include <iostream>
#include <cmath>
#include <vector>

using namespace std;

void IterasiGaussSeidel() {
    int n = 3;
    // Matriks A (Koefisien)
    float A[3][3] = {
        { 4, -1,  1},
        { 4, -8,  1},
        {-2,  1,  5}
    };
    // Vektor b (Hasil)
    float b[3] = {7, -21, 15};

    // Tebakan awal (semua 0)
    vector<float> x(n, 0.0f);
    vector<float> x_lama(n, 0.0f);
    
    float toleransi = 0.3f; // Di dunia nyata ini bisa 0.0001f
    int max_iterasi = 100;
    int iterasi = 0;
    bool selesai = false;

    while (!selesai && iterasi < max_iterasi) {
        float galat_terbesar = 0.0f; // Untuk mengecek galat tertinggi dari semua x

        // 1. Simpan x lama untuk menghitung galat nanti
        for (int i = 0; i < n; i++) x_lama[i] = x[i];

        // 2. Hitung tebakan x baru
        for (int i = 0; i < n; i++) {
            float sigma = 0.0f;
            
            // Masukkan tebakan x lain ke dalam perhitungan
            for (int j = 0; j < n; j++) {
                if (i != j) {
                    sigma += A[i][j] * x[j]; // Menggunakan nilai x PALING BARU yang ada di memori
                }
            }
            
            // Rumus isolasi x_i
            x[i] = (b[i] - sigma) / A[i][i];

            // 3. Hitung Galat Relatif Hampiran untuk variabel ini (Bab 02)
            float galat = std::abs((x[i] - x_lama[i]) / x[i]) * 100.0f;
            if (galat > galat_terbesar) galat_terbesar = galat;
        }

        // 4. Syarat Berhenti (Stopping Criteria)
        if (galat_terbesar <= toleransi) {
            selesai = true;
        }
        
        iterasi++;
    }

    cout << "Iterasi berhenti pada langkah ke-" << iterasi << endl;
    cout << "Solusi x1 = " << x[0] << ", x2 = " << x[1] << ", x3 = " << x[2] << endl;
}
```

## 4. Relevansi Ekstrem di Lingkungan *Game Development*
Mengapa metode ini diajarkan di ilmu komputer?
Dalam membangun *Physics Engine* seperti Box2D atau Havok (yang mendasari banyak sistem *game*), kalkulasi **Collision Resolution** (menyelesaikan tumpukan benda agar tidak saling tembus) menggunakan algoritma turunan yang disebut **Projected Gauss-Seidel (PGS)**.

Karena sifatnya yang *iteratif*, *Game Engine* bisa membatasi jumlah *loop*-nya:
*"Wah, frame rate game kita turun (*ngelag*)! CPU sedang sibuk. Tolong hentikan iterasi Gauss-Seidel di perulangan ke-10 saja, biarkan sedikit tidak akurat asalkan game tidak patah-patah!"*

Ini adalah trik cerdas yang mustahil dilakukan oleh Eliminasi Gauss-Jordan. Gauss-Jordan bersifat *All-or-Nothing* (harus diselesaikan sampai akhir atau tidak sama sekali). Gauss-Seidel bersifat dinamis dan fleksibel sesuai jatah *resource* CPU komputer yang tersedia. Luar biasa, bukan?
