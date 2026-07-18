# Implementasi C++: Normalisasi Phong Shading (Integrasi 3D)

Gambar yang Mas lampirkan adalah penurunan matematis (Kalkulus Integral) untuk mencari **Faktor Normalisasi ($c$) pada Model Pencahayaan Phong 3D (*Phong Shading BRDF*)**. 

Dalam grafika komputer (OpenGL/Vulkan), kita harus memastikan bahwa cahaya pantulan (*specular*) tidak memancarkan energi lebih besar daripada cahaya yang diterima (Hukum Kekekalan Energi). Untuk itu, total pantulan di seluruh setengah bola (*hemisphere* $\Omega$) harus diintegralkan.

Berdasarkan gambar, hasil akhir dari integral ganda tersebut adalah:
$$ c = \frac{n + 2}{2\pi} $$
Di mana $n$ adalah *shininess* (tingkat kekilapan benda).

Ada dua cara seorang *Programmer* / *Engine Developer* menerjemahkan gambar matematika ini ke dalam kode:

---

## 1. Pendekatan *Analitik* (Solusi Langsung di Shader/C++)

Karena para matematikawan sudah bersusah payah mencari hasil akhir integralnya (seperti di gambar, $c = \frac{n+2}{2\pi}$), kita **TIDAK PERLU** menghitung integralnya lagi di dalam *loop* program kita. Kita cukup menuliskan rumus akhirnya saja.

Ini adalah kode yang biasanya berjalan di **Fragment Shader (GLSL)** atau di fungsi perhitungan pencahayaan C++:

```cpp
#include <iostream>

const float PI = 3.14159265359f;

// Fungsi untuk menghitung Konstanta Normalisasi Phong
float HitungNormalisasiPhong(float n_shininess) {
    // Berdasarkan rumus: c = (n + 2) / (2 * PI)
    return (n_shininess + 2.0f) / (2.0f * PI);
}

// Contoh fungsi saat me-render warna pixel (Specular)
float HitungCahayaSpecular(float n_shininess, float cos_theta) {
    float c = HitungNormalisasiPhong(n_shininess);
    
    // c * cos(theta)^(n)
    float specular_intensity = c * std::pow(cos_theta, n_shininess);
    return specular_intensity;
}

int main() {
    float shininess_plastik = 32.0f;
    float c = HitungNormalisasiPhong(shininess_plastik);
    std::cout << "Faktor Normalisasi (c) untuk shininess 32: " << c << std::endl;
    return 0;
}
```

---

## 2. Pendekatan *Numerik* (Jika Rumus Sulit Tidak Bisa Dipecahkan)

Nah, mari kita sambungkan dengan **Mata Kuliah Metode Numerik (Bab Integrasi Numerik)**. 

Bayangkan fungsi pencahayaan benda (*BRDF*) kita **sangat kompleks** (misalnya model *Cook-Torrance* dengan efek rambut atau kain) sehingga dosen/matematikawan tidak bisa menemukan rumus simpel seperti $\frac{n+2}{2\pi}$. 

Bagaimana cara komputer memecahkan integral berlapis ganda $\int_0^{2\pi} \int_0^{\pi/2}$ seperti di gambar?
Jawabannya adalah **Integrasi Numerik (Metode Pias / Riemann Sum / Trapezoidal)**. Komputer akan mencacah sudut $\theta$ (theta) dan $\phi$ (phi) menjadi potongan-potongan sangat kecil, lalu menjumlahkannya dengan *Nested Loop*.

Inilah kode C++ untuk **memecahkan integral secara numerik** tanpa mengetahui jawaban analitiknya:

```cpp
#include <iostream>
#include <cmath>

const float PI = 3.14159265359f;

// Fungsi f(theta) di dalam integral: cos(theta)^(n+1) * sin(theta)
// Catatan: c dikeluarkan dari integral sementara waktu
float FungsiDalamIntegral(float theta, float n) {
    return std::pow(std::cos(theta), n + 1.0f) * std::sin(theta);
}

// Mencari Integral Numerik Ganda (Double Integration) dengan Riemann Sum
void HitungIntegralNumerik(float n_shininess) {
    int jumlah_pias_theta = 1000; // Semakin besar, semakin akurat (dan lambat)
    int jumlah_pias_phi = 1000;
    
    float batas_bawah_theta = 0.0f;
    float batas_atas_theta = PI / 2.0f;
    
    float batas_bawah_phi = 0.0f;
    float batas_atas_phi = 2.0f * PI;
    
    float delta_theta = (batas_atas_theta - batas_bawah_theta) / jumlah_pias_theta;
    float delta_phi = (batas_atas_phi - batas_bawah_phi) / jumlah_pias_phi;
    
    float total_integral = 0.0f;

    // Menghitung Integral Ganda dengan Nested Loop (Loop Bersarang)
    for (int i = 0; i < jumlah_pias_phi; i++) {
        for (int j = 0; j < jumlah_pias_theta; j++) {
            // Titik tengah dari pias (Midpoint Rule)
            float theta = batas_bawah_theta + (j + 0.5f) * delta_theta;
            
            // Hitung luas kotak kecil (tinggi fungsi * lebar alas * panjang alas)
            float nilai_fungsi = FungsiDalamIntegral(theta, n_shininess);
            float luas_pias = nilai_fungsi * delta_theta * delta_phi;
            
            total_integral += luas_pias;
        }
    }

    std::cout << "Hasil Integral Numerik: " << total_integral << std::endl;
    
    // Berdasarkan gambar: Total Integral = 2*PI / (n+2)
    // Berarti c * Total Integral = 1  => c = 1 / Total Integral
    float c_numerik = 1.0f / total_integral;
    
    std::cout << "Maka nilai c (Konstanta Normalisasi): " << c_numerik << std::endl;
}

int main() {
    float n = 32.0f;
    
    // 1. Uji Hasil Analitik (Kalkulus di Kertas)
    float c_analitik = (n + 2.0f) / (2.0f * PI);
    std::cout << "Jawaban Analitik: " << c_analitik << std::endl;
    
    // 2. Uji Hasil Numerik (Aproksimasi Komputer)
    HitungIntegralNumerik(n);
    
    return 0;
}
```

### Kesimpulan Filosofis
Gambar yang Mas kirimkan menunjukkan betapa pentingnya matematika murni. Jika matematika bisa mencari persamaan akhirnya secara **analitik**, program C++ Mas akan berjalan secepat kilat (cuma `1 operasi bagi`). 

Tetapi, jika matematika murni menyerah menghadapi persamaan materi objek yang super aneh, Mas sebagai *Computer Scientist* akan turun tangan menulis *loop* **Integral Numerik** untuk "memaksa" komputer mencari jawabannya menggunakan jutaan kotak kecil!
