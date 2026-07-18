# Kehidupan Rahasia Foton: Simulasi Transportasi Cahaya 2D (Tantalum)
*Dokumen ini merupakan saduran dan implementasi GLSL dari publikasi "The Secret Life of Photons" oleh Benedikt Bitterli (Tantalum).*

---

## 1. Pendahuluan
Transportasi cahaya 3D dalam *Computer Graphics* biasanya menyembunyikan pola pergerakan cahaya yang sebenarnya. Kita hanya melihat cahaya saat ia memantul dari permukaan ke kamera. Dalam model 2D (seperti proyek *Tantalum*), kita mensimulasikan transportasi cahaya dalam ruang 2 dimensi (*Flatland*). Hal ini memungkinkan kita memvisualisasikan **Fluence** (rata-rata jumlah cahaya yang melewati suatu titik, tidak hanya di permukaan objek, tapi juga di ruang hampa), sehingga efek seperti terbentuknya kaustik (*caustics*) akibat lensa/kaca dapat terlihat melayang di udara.

Daripada memecahkan persamaan transportasi cahaya 3D yang sangat rumit dan mengubahnya ke "potongan" 2D, kita mendefinisikan ulang seluruh radiometri di ruang 2D (mengikuti paper *Theory, Analysis and Applications of 2D Global Illumination* oleh Jarosz dkk). Artinya, model material (BRDF) 3D tidak bisa langsung dipakai; mereka harus diturunkan ulang secara khusus untuk 2D.

---

## 2. Model Reflektansi (BRDF) 2D & Implementasi GLSL

Sebagian besar BRDF pada 2D memerlukan konstanta normalisasi yang berbeda akibat perubahan domain integrasi dari hemisfer (setengah bola 3D) menjadi hemisirkel (setengah lingkaran 2D). Berikut adalah implementasi lengkap fungsi *sampling* berbagai material dalam bahasa **GLSL**.

Kita asumsikan koordinat lokal di mana `vec2(0, 1)` adalah Normal Permukaan dan `vec2(1, 0)` adalah Tangen Permukaan. Semua fungsi `rand()` mengembalikan nilai *float* acak antara `[0, 1)`.

### A. Cermin Sempurna (*Smooth Mirror*)
Cermin sempurna sangat sederhana di 2D. Arah pantulan murni dikalkulasi berdasarkan hukum pemantulan sudut.

```glsl
// GLSL: Sampling Cermin Sempurna
// w_o = Arah datangnya sinar (di ruang lokal)
vec2 sampleMirror(vec2 w_o) {
    // Pantulan sederhana terhadap sumbu normal Y
    return vec2(-w_o.x, w_o.y);
}
```

### B. Kaca / Dielektrik Sempurna (*Smooth Dielectric*)
Kaca tidak hanya memantulkan, tapi juga membiaskan sinar bergantung pada Hukum Snellius dan Persamaan Fresnel.

```glsl
// Fungsi pembantu: Koefisien Fresnel (Aproksimasi / Eksak)
float dielectricReflectance(float eta, vec2 w_o) {
    float cosThetaI = abs(w_o.y);
    float sinThetaTSq = eta * eta * (1.0 - cosThetaI * cosThetaI);
    
    // Total Internal Reflection (TIR)
    if (sinThetaTSq > 1.0) return 1.0; 
    
    float cosThetaT = sqrt(1.0 - sinThetaTSq);
    float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    float Rp = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
    
    return (Rs * Rs + Rp * Rp) / 2.0;
}

// GLSL: Sampling Kaca
vec2 sampleDielectric(vec2 w_o, float ior) {
    // Mengecek apakah sinar datang dari luar atau dalam material
    float eta = (w_o.y < 0.0) ? ior : (1.0 / ior);
    float Fr = dielectricReflectance(eta, w_o);
    
    // Probabilistik: Pantulkan atau Biaskan?
    if (rand() < Fr) {
        // Pantulan (Reflection)
        return vec2(-w_o.x, w_o.y);
    } else {
        // Pembiasan (Refraction)
        float sinThetaT = -w_o.x * eta;
        float cosThetaT = sqrt(1.0 - eta * eta * w_o.x * w_o.x) * -sign(w_o.y);
        return vec2(sinThetaT, cosThetaT);
    }
}
```

### C. Permukaan Kasar Difus (*Lambertian*)
Di 3D, faktor normalisasi Lambertian adalah $1/\pi$. Di 2D, konstanta tersebut adalah $1/2$. Arah pantulan di-*sample* secara acak namun proporsional terhadap *cosine* sudut (Hukum Lambert).

```glsl
#define PI 3.14159265359
#define PI_HALF 1.57079632679

// GLSL: Sampling Material Difus (Matte)
vec2 sampleDiffuse(vec2 w_o) {
    float xi = rand();
    
    // Inversi CDF untuk 2D Lambertian
    float sinThetaI = 2.0 * xi - 1.0;
    float cosThetaI = sqrt(1.0 - sinThetaI * sinThetaI);
    
    // Arah yang di-sample secara cosine-weighted di ruang lokal
    return vec2(sinThetaI, cosThetaI);
}
```

### D. Cermin Kasar (*Microfacet Rough Mirror - Logistic Distribution*)
BRDF 3D seperti GGX atau Beckmann mustahil dibalik (diinversi) di 2D karena ketiadaan pengali `sin` pada elemen Jacobian-nya. Karenanya, Benedikt menggunakan **Distribusi Logistik (Logistic Distribution)** yang Gaussian-like tapi bisa diintegralkan.

```glsl
// GLSL: Mendapatkan normal mikro (microfacet normal) yang terlihat
float sampleVisibleNormal(float s, float xi, float thetaMin, float thetaMax) {
    float a = tanh(thetaMin / (2.0 * s));
    float b = tanh(thetaMax / (2.0 * s));
    return 2.0 * s * atanh(a + (b - a) * xi);
}

// GLSL: Sampling Cermin Kasar (Logam/Rough Metal)
vec2 sampleRoughMirror(vec2 w_o, float s) {
    // Mencari rentang sudut normal yang menghadap ke arah sinar datang
    float thetaMin = max(asin(w_o.x), 0.0) - PI_HALF;
    float thetaMax = min(asin(w_o.x), 0.0) + PI_HALF;
    
    // Dapatkan orientasi cermin kecil (microfacet)
    float thetaM = sampleVisibleNormal(s, rand(), thetaMin, thetaMax);
    vec2 m = vec2(sin(thetaM), cos(thetaM)); // Vektor Normal Microfacet (Half-Vector)
    
    // Pantulkan w_o terhadap normal mikro m
    return m * (dot(w_o, m) * 2.0) - w_o;
}
```

### E. Kaca Kasar (*Microfacet Rough Dielectric*)
Pengembangan langsung dari `sampleRoughMirror` yang digabung dengan perhitungan Fresnel dari `sampleDielectric`.

```glsl
// GLSL: Sampling Kaca Kasar / Frosted Glass
vec2 sampleRoughDielectric(vec2 w_o, float ior, float s) {
    float thetaMin = max(asin(w_o.x), 0.0) - PI_HALF;
    float thetaMax = min(asin(w_o.x), 0.0) + PI_HALF;
    
    float thetaM = sampleVisibleNormal(s, rand(), thetaMin, thetaMax);
    vec2 m = vec2(sin(thetaM), cos(thetaM)); // Microfacet Normal

    float woDotM = dot(w_o, m);
    float eta = (woDotM < 0.0) ? ior : (1.0 / ior);
    float Fr = dielectricReflectance(eta, w_o);
    float cosThetaT = sqrt(1.0 - eta * eta * w_o.x * w_o.x);

    // Probabilistik: Pantulan Mikro vs Pembiasan Mikro
    if (rand() < Fr) {
        return 2.0 * woDotM * m - w_o;
    } else {
        return (eta * woDotM - cosThetaT) * m - eta * w_o;
    }
}
```

---

## 3. Eksekusi Transportasi Cahaya di GPU (*Wavefront Tracing*)

Di CPU, algoritma transportasi cahaya (*Light Tracing*) ditulis dalam urutan konvensional (Mega-kernel):
1. *Loop* jumlah sinar (N)
2. *Loop* kedalaman pantulan (P)
3. Hitung *Raytrace* $\rightarrow$ Hitung BRDF $\rightarrow$ Gambar Garis.

Namun, implementasi GPU (WebGL / GLSL) sangat membenci struktur *nested loop* panjang dengan *branching* yang tidak terprediksi. Kompilator seringkali gagal (terkena *Instruction Limit*) karena mencoba men-*unroll* loop BRDF secara naif.

Sistem Tantalum memecahkan masalah ini dengan merombak alurnya menggunakan **Wavefront Tracing** (Berdasarkan paper *Megakernels Considered Harmful*, Laine et al.). Daripada satu *shader* mengeksekusi 1 sinar sampai mati (seluruh pantulannya), kita membaginya menjadi fase-fase paralel untuk seluruh sekumpulan sinar (*Batch M*):

```glsl
// PSEUDO-GLSL LOGIC (Arsitektur Ping-Pong Shader untuk Engine Anda)

// CPU mengorkestrasi shader secara berurutan:
for i = 1 to N/M:
    // 1. Eksekusi Compute Shader 1 (Generate Sinar Awal)
    gpuParallelFor(1 to M): x_i, w_i = sampleLe()
    
    for j = 1 to p:
        // 2. Eksekusi Compute Shader 2 (Raytracing & Interseksi)
        // Mengeksekusi Raycast paralel ke seluruh scene, menghasilkan titik baru x_i'
        gpuParallelFor(1 to M): x_i' = raytrace(x_i, w_i)
        
        // 3. Eksekusi Compute Shader 3 (Scattering / BRDF Sampling)
        // Shader ini memanggil GLSL functions yang kita definisikan di atas (sampleRoughMirror, dll)
        gpuParallelFor(1 to M): w_i' = sampleBrdf(x_i', w_i)
        
        // 4. Eksekusi Render Pass (Additive Blending)
        // Gambar segmen garis dari x_i ke x_i' menggunakan GL_LINES dan glBlendFunc(GL_ONE, GL_ONE)
        // PERHATIAN: Lihat bagian Rasterization Bias di bawah!
        gpuParallelFor(1 to M): drawLine(x_i, x_i')
        
        // 5. Swap Buffer (Ping-Pong)
        gpuParallelFor(1 to M): x_i = x_i', w_i = w_i'
```

Pemisahan logika (*loop inversion*) ini sangat penting jika Mas mengimplementasikannya di dalam Vulkan atau OpenGL C++ agar tidak terjadi kemacetan (*Register Pressure*) di dalam *Shader Unit*. Setiap *shader program* menjadi sangat pendek dan super efisien!

---

## 4. Mengatasi *Rasterization Bias* (Kelemahan GPU)

Saat mengimplementasikan *Light Tracing* menggunakan rasterisasi GPU (*drawing lines* menggunakan `GL_LINES`), ada artefak yang disebut **Rasterization Bias**. 

Secara matematis, garis cahaya seharusnya direpresentasikan sebagai garis dengan ketebalan infinitesimal (sangat tipis tak terhingga) yang disebarkan ke pixel-pixel. Namun, aturan rasterisasi GPU akan menyalakan pixel secara penuh jika dilewati oleh garis, dan cara GPU menggambar garis itu bias terhadap orientasi garisnya.
- Garis horizontal/vertikal murni hanya menyalakan jumlah pixel sesuai panjangnya.
- Garis diagonal menyalakan jumlah pixel yang sama dengan sisi terpanjangnya (`max(dx, dy)`).

Ini menyebabkan intensitas garis pada sudut diagonal terlihat lebih terang atau salah terakumulasi dibanding garis vertikal/horizontal saat di-*blend* secara aditif.

**Solusinya (Berdasarkan Tantalum):**
Kita harus mengalibrasi (menurunkan intensitas) *opacity/alpha* dari garis yang digambar di dalam *Fragment Shader* berdasarkan rasio panjang aslinya berbanding panjang rasterisasinya.

```glsl
// GLSL Fragment Shader: Kompensasi Rasterization Bias
void main() {
    // Vektor arah dari titik awal garis ke titik akhir
    vec2 dir = endPos - startPos; // Biasanya dilempar dari Vertex Shader
    
    // Panjang sebenarnya dari garis di layar (dalam hitungan pixel)
    float trueLength = length(dir);
    
    // Jumlah pixel yang benar-benar akan dinyalakan oleh GPU rasterizer
    float rasterizedLength = max(abs(dir.x), abs(dir.y));
    
    // Faktor koreksi intensitas
    float biasCorrection = trueLength / rasterizedLength;
    
    // Warna asli cahaya (misal putih, atau dihitung dari wavelength)
    vec3 lightColor = vec3(1.0, 1.0, 1.0); 
    
    // Output ke Framebuffer (Additive Blending)
    gl_FragColor = vec4(lightColor * biasCorrection, 1.0);
}
```

Dengan mengalikan warna dengan `trueLength / rasterizedLength`, garis-garis diagonal yang tadinya terlalu tebal (karena aturan rasterisasi) akan menjadi lebih redup secara proporsional. Hasil akhirnya akan kembali akurat secara matematis (*Physically Based*) saat ratusan ribu garis diakumulasikan!
