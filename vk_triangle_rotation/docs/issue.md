# Isu: WinRT Custom Menubar menyebabkan Bug "Windows 7 Classic Frame"

## Akar Permasalahan
Pada implementasi awal, arsitektur kode aplikasi ini mencampuradukkan logika Win32 dan WinRT di dalam satu *class* window. 

Ketika mode WinRT digunakan secara penuh (`--mode=win-aero-disable`), kode mencoba menggambar menu buatan (Custom Menu) di area **Non-Client (NC)** yaitu area *title bar* dan *frame* pinggiran window. Untuk dapat menggambar di area sistem tersebut, implementasi awal mematikan sistem komposisi *Desktop Window Manager* (DWM) menggunakan *flag* `DWMNCRP_DISABLED` melalui fungsi `DwmSetWindowAttribute`.

## Penyebab Bug "Classic Frame"
Mematikan DWM (`DWMNCRP_DISABLED`) pada area NC ternyata menimbulkan efek samping yang tidak diinginkan. 

Ketika DWM dimatikan, Windows kehilangan instruksi tentang bagaimana cara menggambar *frame* modern (gaya Windows 10/11). Sebagai mekanisme pertahanan (fallback), sistem operasi jatuh ke mode kompatibilitas lama, yang menyebabkan pinggiran window berubah wujud menjadi gaya klasik ala Windows 7 Basic atau Windows 98. Itulah yang menyebabkan *bug* visual di mana window terlihat usang.

## Solusi dan Refaktor Arsitektur
Untuk memperbaiki masalah ini tanpa mengorbankan desain menubar yang modern dan *customizable*, dilakukan perombakan arsitektur (Refactor) secara total:

1. **Pemisahan Mode Murni:** Membelah class menjadi dua bagian yang berdiri sendiri murni:
   - `WindowWin32`: Untuk *native* Win32 dengan menu bawaan OS (`SetMenu`).
   - `WindowWinRT`: Untuk implementasi C++/WinRT dengan menu kustom.

2. **Mengaktifkan Kembali DWM:** Menghapus penggunaan `DWMNCRP_DISABLED` pada WinRT window agar DWM kembali beroperasi secara normal (default `USEWINDOWSTYLE`). Hasilnya, *title bar* (tombol close, minimize, maximize) kembali dirender secara modern oleh OS, termasuk dukungan Dark/Light mode bawaan.

3. **Pendekatan *Client-Area Child HWND*:** Ketimbang memaksa membajak area sistem (NC) untuk menggambar menu, menubar kustom dipindahkan ke dalam **Client Area** (area kerja utama aplikasi).
   - Dibuat satu *Child Window (HWND)* kecil di bagian paling atas *client area* khusus untuk menggambar menubar bergaya *flat* (Ableton-style).
   - Dibuat satu *Child Window (HWND)* kedua di bawah menubar, khusus didedikasikan sebagai target *surface* untuk *render* Vulkan.

4. **Penyesuaian Estetika:** Desain menubar kustom diperkecil dimensinya (font `9pt`, tinggi bar `24-28px`) dan disesuaikan warna *hex*-nya agar tampil minimalis, padat, dan *premium* menyerupai antarmuka Ableton Live 12.

### Kesimpulan
Dengan memisahkan area *rendering* antara sistem DWM (untuk *title bar*) dan aplikasi (untuk menubar sebagai child HWND, serta Vulkan sebagai child HWND terpisah), *bug* fallback ke *Classic Frame* berhasil diselesaikan. Aplikasi sekarang memiliki *title bar* OS yang modern dipadukan dengan menubar kustom *WinRT* yang *aesthetic* dan fleksibel.

---

## Contoh Kode: Permasalahan (Bad Practice) vs Penanganan (Good Practice)

Berikut adalah perbandingan antara kode yang menyebabkan *bug* dengan penanganan arsitektur yang benar:

### ❌ Yang Salah (Menyebabkan Bug Windows 7 Classic Frame)
Mencoba mematikan DWM di area *Non-Client* agar bisa menggambar *custom menubar*. Ini adalah praktik yang **salah** karena memaksa Windows mematikan komposisi *frame* modern.

```cpp
// ❌ JANGAN LAKUKAN INI: Mematikan NC Rendering DWM
DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
DwmSetWindowAttribute(m_hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));

// Akibatnya:
// - Title bar OS modern (Win 10/11) menghilang.
// - Sistem fallback ke tema Windows Classic (pinggiran kaku ala Win 98/7 Basic).
```

Selain itu, mencegat pesan *Non-Client* untuk menggambar *custom UI* berisiko tinggi bertabrakan dengan *behavior* sistem operasi:
```cpp
// ❌ JANGAN LAKUKAN INI: Membajak area gambar sistem (NC PAINT)
case WM_NCPAINT: {
    // Berusaha menggambar menubar sendiri di area sistem (Non-Client)
    HDC hdc = GetWindowDC(hwnd);
    // ... melakukan GDI drawing ...
    ReleaseDC(hwnd, hdc);
    return 0;
}
```

---

### ✅ Penanganan yang Benar (Client-Area Child HWND)
Biarkan DWM beroperasi secara *default* agar *frame* window tetap terlihat modern, dan letakkan semua elemen UI buatan sendiri ke area kerja utama (*Client Area*) sebagai *Child Window*.

**Langkah 1: Biarkan DWM Mengurus Main Window**
```cpp
// ✅ DWM dibiarkan beroperasi secara default (DWMNCRP_USEWINDOWSTYLE)
// Window akan otomatis memiliki title bar modern Win 10/11 dengan dukungan Dark Mode
m_hwnd = CreateWindowEx(
    0, "MainWindowClass", "Judul Window",
    WS_OVERLAPPEDWINDOW, // <-- Flag standar agar DWM merender frame dengan benar
    CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
    nullptr, nullptr, hinst, this);
```

**Langkah 2: Gambar Custom Menubar di Client Area**
```cpp
// ✅ Buat menubar sebagai Child HWND, letakkan di (x=0, y=0) di dalam Client Area
m_menubarHwnd = CreateWindowEx(
    0, "CustomMenuBarClass", "",
    WS_CHILD | WS_VISIBLE, // <-- Penting: Jadikan sub-window (child) dari Main Window
    0, 0, clientWidth, 24, // Tinggi menubar kustom = 24px
    m_hwnd, nullptr, hinst, nullptr);
```

**Langkah 3: Pisahkan Target Render Vulkan/DirectX**
```cpp
// ✅ Karena 24px bagian atas layar sudah dipakai menubar, 
//    buat Child HWND terpisah di bawahnya khusus untuk target rendering API grafis
m_renderHwnd = CreateWindowEx(
    0, "VulkanRenderClass", "",
    WS_CHILD | WS_VISIBLE, 
    0, 24,                 // <-- Geser (offset) ke bawah menubar (Y = 24)
    clientWidth, clientHeight - 24,
    m_hwnd, nullptr, hinst, nullptr);
```

Dengan penanganan arsitektur seperti ini, Sistem Operasi (OS) tetap mengurus interaksi *title bar* (warna, *minimize/maximize*, *snap layout*), sementara *developer* bebas berkreasi menggambar *custom UI* dan grafis 3D (Vulkan) di *Client Area* tanpa khawatir terjadi bentrok (*glitch*) visual.
