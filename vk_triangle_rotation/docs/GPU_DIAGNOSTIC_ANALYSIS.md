# Catatan Analisa Diagnostik Hardware & Sinkronisasi Vulkan (RTX 3050 Laptop)

## 1. Ringkasan Kasus (Executive Summary)
* **Gejala yang dialami:** Visual grafis mendadak membeku (*hang/freeze*), namun proses engine/game masih hidup (BGM/suara tetap bersuara), terkadang disertai suara glitch dengungan *"ttrrrrttt"*.
* **Gejala Serupa di Game Lain:** Terjadi pola yang persis sama pada *Genshin Impact* (Unity Engine / DirectX 11).
* **Kode Error Vulkan:**
  ```text
  [ERROR] [RTX-DIAG] vkQueueSubmit failed with code: -4
  [WARN ] [RTX-DIAG] vkWaitForFences returned error/timeout code: -4
  ```

---

## 2. Analisa Hardware: Mengapa Thermal Paste & Putty BUKAN Masalahnya?
* **Mekanisme Termal Silikon Modern (TjMax / PROCHOT):**
  Jika pasta termal pada GPU core atau thermal putty pada modul VRAM GDDR6 tidak menempel / kering:
  1. Suhu silikon akan langsung menembus batas kritis ($T_j > 100^\circ\text{C} - 105^\circ\text{C}$).
  2. Proteksi hardware internal akan langsung memicu **Emergency Thermal Shutdown** (laptop mati mendadak dalam 1 detik) atau Kernel BSOD (`WHEA_UNCORRECTABLE_ERROR`).
* **Fakta:** Laptop tidak mati, OS Windows tetap stabil, proses game tetap berjalan di background. Ini membuktikan bahwa suhu hardware dan pasta/putty masih dalam batas normal yang aman.

---

## 3. Arti Sebenarnya dari Kode `-4` (`VK_ERROR_DEVICE_LOST`)
* Sesuai spesifikasi resmi Khronos Vulkan:
  $$\text{VK\_ERROR\_DEVICE\_LOST} = -4$$
* Di DirectX 11 (Genshin Impact), ekuivalennya adalah **`DXGI_ERROR_DEVICE_REMOVED`** atau **`DXGI_ERROR_DEVICE_RESET`**.
* Ini adalah intervensi kernel Windows bernama **TDR (Timeout Detection and Recovery)**. Jika GPU Hardware Command Processor terhenti atau mendeteksi instruksi tidak valid lebih dari 2 detik (default), Windows me-reset logical device context agar seluruh OS tidak blue screen.

---

## 4. Tiga Akar Masalah di Tingkat Kode & Solusinya

### A. Tabrakan Indeks Command Buffer (`idx` vs `m_currentFrame`)
* **Masalah:** Command buffer di-reset menggunakan indeks gambar swapchain (`m_cmdBufs[idx]`), sedangkan fence CPU-GPU disinkronkan berdasarkan frame in-flight (`m_fences[m_currentFrame]`). Karena urutan swapchain dari DWM Windows tidak selalu sejalan dengan urutan frame, CPU mereset command buffer yang **masih aktif dieksekusi oleh hardware NVIDIA RTX**.
* **Solusi:** Menyamakan alokasi dan perekaman command buffer per frame in-flight (`cmdBuf = m_cmdBufs[m_currentFrame]`).

### B. Urutan Eksekusi UI vs Render Pass (Hazard Slider Segi-9)
* **Masalah:** Slider *Polygon Sides* dan tombol ganti karakter sebelumnya diproses di dalam ImGui setelah `vkCmdDraw()` dipanggil. Saat slider digeser ke 9 sisi, memori vertex ditimpa dan `m_vertexCount` diubah saat draw call sudah terlanjur direkam dengan data lama.
* **Solusi:** Seluruh input mouse, slider poligon, dan ganti karakter dipindahkan ke awal fungsi `draw()` sebelum render pass direkam. Saat `vkCmdDraw` dipanggil, buffer vertex dan jumlah titik sudah 100% mutakhir.

### C. Fence Deadlock pada `vkResetFences`
* **Masalah:** `vkResetFences` dipanggil sebelum `vkAcquireNextImageKHR`. Jika swapchain menghasilkan `VK_ERROR_OUT_OF_DATE_KHR` (misal saat resize/minimize), fence terlanjur di-reset tapi submit GPU dibatalkan, menyebabkan frame berikutnya menunggu selamanya (*deadlock*).
* **Solusi:** `vkResetFences` hanya dipanggil setelah acquire gambar sukses dan submit dipastikan berjalan.

---

## 5. Mengapa Muncul Suara Glitch "ttrrrrttt"?
* Suara tersebut **bukan suara fisik GPU / coil whine**, melainkan suara dari **speaker laptop**.
* Saat Windows me-reset driver grafis (TDR), kernel Windows mengalami lonjakan **DPC (Deferred Procedure Call) Latency**.
* Driver audio WASAPI (yang memiliki buffer kecil 10–20 ms) kehabisan data (*buffer starvation / underrun*).
* Chip DAC sound card mengulang potongan buffer sample terakhir (~128 sample) secara looping super cepat (~100–300 Hz), menghasilkan dengungan glitch *"ttrrrrttt"*.
* **Solusi:** Buffer audio WASAPI dinaikkan ke 25ms dan mutex-nya dibuat non-blocking (`try_to_lock`).

---

## 6. Korelasi dengan Genshin Impact & Windows TDR
* Di Genshin Impact, game memisahkan **Render Thread** (DirectX) dan **Audio Thread** (FMOD). Saat TDR terjadi, render thread mati (grafis freeze), sementara audio thread tetap memutar musik latar.
* **Faktor Pemicu pada RTX 3050 Laptop:**
  1. **VRAM 4 GB Terbatas:** Genshin pada setting High membutuhkan $\ge 3.8\text{ GB}$. Saat VRAM penuh, pemindahan tekstur ke RAM sistem via PCIe memakan waktu lebih dari ambang TDR.
  2. **TdrDelay Default Windows (2 Detik):** Windows terlalu agresif memotong proses GPU.
* **Solusi Windows:** Menaikkan `TdrDelay` di Registry (`HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers`) menjadi **10** (Decimal), dan mengatur *Power Management Mode* di NVIDIA Control Panel ke *Prefer maximum performance*.
