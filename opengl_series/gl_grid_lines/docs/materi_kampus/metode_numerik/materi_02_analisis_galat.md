# Metode Numerik 02: Analisis Galat (*Error Analysis*)

## 1. Filosofi Ketidaksempurnaan
Tidak ada solusi murni di dalam metode numerik, yang ada hanyalah solusi hampiran (*approximation*). Seberapa jauh solusi kita melenceng dari kenyataan eksaknya? Nilai kemelencengan itulah yang disebut **Galat** (*Error*, disimbolkan $\epsilon$).

Tugas seorang *Developer C++* bukanlah menghapus galat hingga nol (karena itu mustahil akibat keterbatasan memori RAM/CPU), melainkan **mengukur dan mengendalikan** galat agar berada di batas toleransi yang aman.

## 2. Tiga Jenis Rumus Galat & Kodenya di C++

Misalkan nilai sejati adalah $a$, dan nilai hampiran (hasil tebakan komputasi kita) adalah $\hat{a}$.

### A. Galat (Murni) dan Galat Mutlak
- **Rumus Matematika:** 
  $$\text{Galat } (\epsilon) = a - \hat{a}$$
  $$\text{Galat Mutlak } (|\epsilon|) = |a - \hat{a}|$$
- **Kenapa harus mutlak (positif)?** Karena di pemrograman, kita tidak peduli tebakannya kelebihan (positif) atau kurang (negatif), kita cuma peduli seberapa *jauh* selisih jaraknya.
- **Implementasi C++:**
  ```cpp
  #include <cmath> // Untuk std::abs

  float nilai_sejati = 10.45f;
  float nilai_hampiran = 10.50f;
  
  float galat = nilai_sejati - nilai_hampiran; // Hasil: -0.05
  float galat_mutlak = std::abs(galat);        // Hasil: 0.05 (Selalu positif)
  ```

### B. Galat Relatif Sejati
- **Rumus Matematika:** Menormalkan galat dengan membaginya dengan nilai aslinya.
  $$\epsilon_R = \frac{|\epsilon|}{a} \times 100\%$$
- **Kenapa ini penting?** Meleset 1 cm saat mengukur pensil adalah galat yang fatal (Galat = $10\%$). Tapi meleset 1 cm saat mengukur panjang jalan tol adalah galat yang luar biasa teliti (Galat = $0.00001\%$).
- **Implementasi C++:**
  ```cpp
  float galat_relatif = (galat_mutlak / nilai_sejati) * 100.0f;
  ```

### C. Galat Relatif Hampiran (Kondisi Berhenti / *Stopping Criteria*)
Di dunia pemrograman, kita **TIDAK PERNAH TAHU nilai sejati ($a$)**. Kalau kita tahu nilai sejatinya, untuk apa kita repot-repot bikin *software* tebakan? 
Maka dari itu, galat diukur dengan cara membandingkan **Tebakan Sekarang** dengan **Tebakan Sebelumnya**.
- **Rumus Matematika:**
  $$\epsilon_{RA} = \left| \frac{a_{sekarang} - a_{sebelumnya}}{a_{sekarang}} \right| $$
  Kita akan terus melakukan *loop* iterasi, hingga nilai $\epsilon_{RA}$ lebih kecil dari Toleransi ($E_s$).
- **Implementasi Skrip di C++:**
  ```cpp
  float tebakan_sebelumnya = 0.0f;
  float tebakan_sekarang = 0.5f;
  float epsilon = 0.0001f; // Toleransi kita: 0.01%
  bool berhenti = false;

  while (!berhenti) {
      // 1. Simpan tebakan lama
      tebakan_sebelumnya = tebakan_sekarang;
      
      // 2. Hitung tebakan baru (menggunakan rumus numerik)
      tebakan_sekarang = HitungTebakanBaru(tebakan_sebelumnya);

      // 3. Hitung Galat Relatif Hampiran
      float galat_RA = std::abs((tebakan_sekarang - tebakan_sebelumnya) / tebakan_sekarang);

      // 4. Syarat Berhenti (Stopping Condition)
      if (galat_RA < epsilon) {
          berhenti = true; // Mesin puas dengan tingkat ketelitian ini
      }
  }
  ```

## 3. Sumber Galat Bawaan (*Hardware/Software Limit*)

Ada dua alasan mendasar kenapa Anda selalu butuh logika epsilon `< 0.0001f` di atas saat memprogram C++/OpenGL:

1. **Galat Pembulatan (*Round-off Error*):** Komputer hanya punya bit terbatas. Angka `1/3` (0.333...) dipaksa masuk memori jadi `0.33333334`. Selisih paksaan ini akan menumpuk pelan-pelan di dalam loop.
   - *Cara Mengatasi di C++:* Jangan pakai `double` kalau cukup `float` untuk kecepatan, tapi kalau akurasi simulasi berantakan, naikkan *memory limit* dari `float` ke `double`. Jangan pernah membandingkan pecahan `if (x == y)`.
   
2. **Galat Pemotongan (*Truncation Error*):** Angka seperti Sinus, Tangen, Logaritma atau PI ($\pi$) tidak bisa dihitung komplit oleh CPU. CPU menghitung menggunakan *Deret Taylor*, yang panjangnya tak terhingga. CPU harus "memotong" deretnya di iterasi ke-5 atau ke-10 agar komputer tidak hang.
   - *Cara Mengatasi di C++:* Pemotongan ini sengaja kita lakukan untuk menghemat CPU. Pahami batasan dan jangan berdebat jika hasil trigonometri Anda meleset pada desimal ke-6.
