# 3. Window & Event Loop

## 3.1 Arsitektur Window Windows (Win32 & WinRT)

Kedua implementasi (`WindowWin32` dan `WindowWinRT`) menggunakan mekanisme
dasar yang sama:

```
┌─────────────────────────────────────────────┐
│               CreateWindowEx                 │
│   (fungsi Win32 yang membuat HWND)           │
├─────────────────────────────────────────────┤
│                                              │
│   Selama CreateWindowEx jalan:               │
│     → OS kirim WM_NCCREATE, WM_CREATE,       │
│       WM_SIZE, WM_SHOWWINDOW, dll            │
│     → wndProc() kita tangkap pesan-pesan ini │
│                                              │
├─────────────────────────────────────────────┤
│           Setelah CreateWindowEx:            │
│     → ShowWindow(hwnd, SW_SHOW)              │
│     → Window terlihat di layar               │
└─────────────────────────────────────────────┘
```

## 3.2 Window Class Registration

Sebelum bisa bikin window, kita harus daftarkan **window class** ke OS.

```cpp
const char* CLASS = "VulkanWinRTClass";

WNDCLASS wc{};
wc.lpfnWndProc   = wndProc;       // ← fungsi yang handle semua pesan
wc.hInstance     = m_hinst;       // ← handle ke executable kita
wc.hCursor       = LoadCursor(nullptr, IDC_ARROW); // ← cursor default
wc.hbrBackground = nullptr;       // ← kita render sendiri, gak perlu background
wc.lpszClassName = CLASS;         // ← nama class (identitas)

RegisterClass(&wc);
```

### Kenapa `hbrBackground = nullptr`?

Biasanya window class punya background brush (misal putih). Tapi karena
kita Vulkan, kita render tiap frame dengan clear color hitam. Background
brush cuma akan menimpa render kita.

## 3.3 Window Procedure (wndProc)

Ini adalah **jantung** window. Semua pesan dari OS lewat sini.

```cpp
LRESULT CALLBACK WindowWinRT::wndProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam) {
```

### Parameter:

| Nama | Arti |
|------|------|
| `hwnd` | Handle ke window yang menerima pesan |
| `msg` | Kode pesan (WM_CLOSE, WM_PAINT, dll) |
| `wParam` | Info tambahan (tergantung pesan) |
| `lParam` | Info tambahan (tergantung pesan) |

### Bagaimana `this` Sampai ke wndProc?

`wndProc` adalah **static function** — dia gak punya `this`. Tapi kita
 perlu akses anggota kelas (`m_shouldClose`, `m_hwnd`, dll).

Caranya: pasang `this` sebagai **user data** waktu `CreateWindowEx`:

```cpp
m_hwnd = CreateWindowEx(0, CLASS, title, ...,
                         nullptr, nullptr, m_hinst, this);
//                                                   ↑
//                                            parameter terakhir = lpParam
```

Lalu di `WM_NCCREATE`, simpan ke `GWLP_USERDATA`:

```cpp
if (msg == WM_NCCREATE) {
    auto cs = (CREATESTRUCT*)lParam;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    return TRUE;  // ← penting: return TRUE agar creation lanjut
}
```

Setelah itu, pesan-pesan berikutnya bisa ambil `this`:

```cpp
auto win = (WindowWinRT*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
if (win) {
    // Sekarang bisa akses win->m_shouldClose, win->m_hwnd, dll
}
```

### Diagram Alur Pesan

```
User klik X
  │
  ├→ OS kirim WM_CLOSE ke wndProc
  │    │
  │    ├→ Set m_shouldClose = true
  │    ├→ DestroyWindow(hwnd)
  │    │    │
  │    │    ├→ OS kirim WM_DESTROY
  │    │    │    │
  │    │    │    ├→ Set m_shouldClose = true
  │    │    │    ├→ m_hwnd = nullptr
  │    │    │    ├→ PostQuitMessage(0) → WM_QUIT masuk queue
  │    │    │    └→ return 0
  │    │    │
  │    │    └→ OS hancurkan window (free memory)
  │    │
  │    └→ return 0
  │
  └→ pollEvents() next iteration:
       PeekMessage dapat WM_QUIT
       → m_shouldClose sudah true
       → return false
       → render loop exit
```

## 3.4 Message Pump (pollEvents)

```cpp
bool WindowWinRT::pollEvents() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);   // terjemahkan keyboard
        DispatchMessage(&msg);    // kirim ke wndProc
    }
    return !m_shouldClose;
}
```

### Kenapa `PeekMessage`, bukan `GetMessage`?

| Fungsi | Perilaku |
|--------|----------|
| `GetMessage` | **Blokir** sampai ada pesan. Cocok untuk aplikasi yang idle. |
| `PeekMessage` | **Cek dan langsung balik.** Jika tidak ada pesan, return false. |

Untuk render loop, kita pake `PeekMessage` karena:
- Kita harus render 60+ fps
- Kalau pake `GetMessage`, render cuma jalan kalau ada event
- Dengan `PeekMessage`, render jalan terus, event diproses secepatnya

### Alur Satu Iterasi Loop

```
pollEvents() dipanggil
  │
  ├─ PeekMessage → ada WM_MOUSE?   → DispatchMessage → wndProc → return 0
  ├─ PeekMessage → ada WM_SIZE?    → DispatchMessage → wndProc → simpan size
  ├─ PeekMessage → ada WM_PAINT?   → DispatchMessage → wndProc → BeginPaint/EndPaint
  ├─ PeekMessage → ada WM_QUIT?    → loop berhenti
  └─ PeekMessage → tidak ada lagi  → exit while → return !m_shouldClose
       │
       └→ renderer->draw() jalan sekali
            │
            └→ pollEvents() lagi → repeat
```

## 3.5 Pesan-Pesan Penting

### WM_NCCREATE

Pesan **pertama** yang diterima window. Dikirim saat `CreateWindowEx`
masih jalan. Kita manfaatkan untuk menyimpan pointer `this`.

Harus return `TRUE` — kalau return `FALSE`, `CreateWindowEx` gagal.

### WM_CLOSE

Dikirim saat user:
- Klik tombol X di pojok kanan atas
- Tekan Alt+F4
- Panggil `DestroyWindow` dari luar

**Jangan panggil `PostQuitMessage` di sini!** Kenapa? Karena `DestroyWindow`
yang kita panggil akan mengirim `WM_DESTROY`, dan `WM_DESTROY`-lah yang
seharusnya memanggil `PostQuitMessage`.

### WM_DESTROY

Dikirim setelah `DestroyWindow`. Di sinilah kita:
1. Set `m_shouldClose = true`
2. Set `m_hwnd = nullptr` (mencegah double-destroy di destructor)
3. Panggil `PostQuitMessage(0)` — kirim WM_QUIT ke queue

### WM_PAINT

Dikirim saat OS ingin window merefresh diri. Kita panggil:

```cpp
PAINTSTRUCT ps;
BeginPaint(hwnd, &ps);   // ← validasi region yang perlu di-paint
EndPaint(hwnd, &ps);     // ← tandai bahwa kita sudah handle
```

Ini penting: tanpa `BeginPaint`/`EndPaint`, OS akan terus kirim `WM_PAINT`
 setiap kali loop, menyebabkan CPU 100%.

### WM_SIZE

Dikirim saat window di-resize. `lParam` mengandung:

```
LOWORD(lParam)  = lebar client area baru
HIWORD(lParam)  = tinggi client area baru
```

Kita simpan di `m_width` / `m_height` agar bisa dipakai renderer.

### WM_SETTINGCHANGE

Dikirim saat setting sistem berubah (termasuk tema dark/light).

```cpp
case WM_SETTINGCHANGE:
    if (wParam == 0 && lParam) {
        auto str = (const wchar_t*)lParam;
        if (wcscmp(str, L"ImmersiveColorSet") == 0) {
            // Jangan langsung baca registry — masih settling!
            SetTimer(hwnd, THEME_TIMER_ID, 150, nullptr);
        }
    }
    return 0;
```

## 3.6 Dark/Light Theme (WinRT Specific)

### Deteksi Awal (Registry)

```cpp
static bool readDarkModeRegistry() {
    DWORD value = 1;    // default light
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_DWORD, nullptr, &value, &size);
    return value == 0;   // 0 = dark, 1 = light
}
```

### Apply ke Title Bar

```cpp
void WindowWinRT::applyTheme(bool dark) {
    m_isDarkMode = dark;
    if (!m_hwnd) return;

    BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(m_hwnd,
        20,                           // DWMWA_USE_IMMERSIVE_DARK_MODE
        &value, sizeof(value));

    // Paksa title bar untuk repaint dengan theme baru
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    RedrawWindow(m_hwnd, nullptr, nullptr,
                 RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}
```

### Listen Perubahan (WinRT Event)

**Penting:** `UISettings` harus disimpan sebagai **member** kelas, bukan
local variable. Jika hanya dibuat sebagai local di `create()`, object
akan di-destroy saat `create()` return → event `ColorValuesChanged` mati
permanen dan tidak akan ke-trigger lagi.

```cpp
// Di header — UISettings harus hidup seumur window
winrt::Windows::UI::ViewManagement::UISettings m_uiSettings{nullptr};
void* m_colorToken = nullptr;

// Di create():
try {
    m_uiSettings = UISettings();   // ← disimpan sebagai member, bukan local
    auto token = m_uiSettings.ColorValuesChanged(
        [hwnd = m_hwnd](auto&&...) {
            PostMessage(hwnd, WM_THEMECHANGE, 0, 0);
        });
    m_colorToken = new winrt::event_token(token);
} catch (...) { }
```

**Kenapa pake `PostMessage`, bukan langsung `applyTheme()`?**
- Event `ColorValuesChanged` jalan di **background thread**.
- Window handle (`HWND`) hanya aman diakses dari **main thread** (thread
  tempat window dibuat).
- `PostMessage` mengirim pesan ke queue main thread.
- `wndProc` di main thread menerima `WM_THEMECHANGE` dan panggil
  `applyTheme()` dengan aman.

### Deferred Theme Read (SetTimer)

**Masalah:** `ColorValuesChanged` bisa kebakarnya sebelum registry
`AppsUseLightTheme` selesai di-update oleh Windows. Jadi
`readDarkModeRegistry()` masih baca nilai lama → tema gak berubah.

**Solusi:** Pakai `SetTimer` untuk nunda pembacaan registry ~150ms.
Pada saat timer kebakar, registry sudah settle.

```cpp
// Definisikan timer ID
#define THEME_TIMER_ID  1001

// WM_THEMECHANGE — dari ColorValuesChanged callback
case WM_THEMECHANGE:
    SetTimer(hwnd, THEME_TIMER_ID, 150, nullptr);  // deferred
    return 0;

// WM_SETTINGCHANGE — dari OS broadcast
case WM_SETTINGCHANGE:
    if (wParam == 0 && lParam) {
        auto str = (const wchar_t*)lParam;
        if (wcscmp(str, L"ImmersiveColorSet") == 0) {
            SetTimer(hwnd, THEME_TIMER_ID, 150, nullptr);  // deferred
        }
    }
    return 0;

// WM_TIMER — registry sudah settle, baru baca
case WM_TIMER:
    if (wParam == THEME_TIMER_ID) {
        KillTimer(hwnd, THEME_TIMER_ID);
        win->applyTheme(readDarkModeRegistry());
    }
    return 0;
```

**Kenapa 150ms?** Cukup lama agar registry di-update oleh Windows,
tapi cukup cepat agar user gak ngefek delay.

### Diagram Theme Change

```
OS Settings: Dark Mode ON
  │
  ├→ Registry: AppsUseLightTheme = 0
  │
  ├→ WinRT: UISettings::ColorValuesChanged event fires
  │    │
  │    └→ Background thread:
  │         PostMessage(hwnd, WM_THEMECHANGE)
  │
  ├→ (Fallback) WM_SETTINGCHANGE with "ImmersiveColorSet"
  │
  └→ Main thread (pollEvents):
       PeekMessage → WM_THEMECHANGE atau WM_SETTINGCHANGE
       → SetTimer(150ms)                    ← jangan langsung baca!
       │
       └→ WM_TIMER (150ms kemudian):
            → KillTimer
            → readDarkModeRegistry()         ← registry sudah settle
            → applyTheme(dark)
                 → DwmSetWindowAttribute(20, TRUE/FALSE)
                 → SetWindowPos + RedrawWindow
                 → Title bar berubah warna
```

### Kenapa Title Bar Harus di-Set Ulang?

`DwmSetWindowAttribute` dengan `DWMWA_USE_IMMERSIVE_DARK_MODE` bisa
menghapus text title bar pada beberapa versi Windows 10. Untuk
memastikan title tetap tampil, kita panggil `SetWindowTextA` setelah
`applyTheme()`:

```cpp
m_isDarkMode = readDarkModeRegistry();
applyTheme(m_isDarkMode);
SetWindowTextA(m_hwnd, desc.title.c_str());  // ← pastikan title tetap ada
```

### Cleanup Event Token

Event `ColorValuesChanged` harus di-unsubscribe sebelum object di-destroy.
Kita gunakan stored `UISettings` member (bukan local) untuk unsubscribe:

```cpp
case WM_DESTROY:
    // ...
    if (win->m_colorToken) {
        auto tok = *static_cast<winrt::event_token*>(win->m_colorToken);
        win->m_uiSettings.ColorValuesChanged(tok);  // unsubscribe
        delete static_cast<winrt::event_token*>(win->m_colorToken);
        win->m_colorToken = nullptr;
    }
```

## 3.7 Menu Bar — Aero Enable / Disable

Gaya window dikontrol oleh **runtime argument** `--mode`:

| Nilai | Mode | Tampilan |
|-------|------|----------|
| `win-aero-enable` (default) | Modern DWM (Win10+) | Title bar tebal + custom-drawn menu bar |
| `win-aero-disable` | Aero (klasik Win32) | Title bar tipis + menu bar `SetMenu()` |

Penggunaan:

```bash
main.exe                           # default: aero-enable (custom NC)
main.exe --mode=win-aero-disable   # classic Win32 SetMenu
main.exe -m win-aero-enable        # manual: custom NC menu bar
```

### Cara Kerja Runtime

Mode disimpan sebagai **member boolean** `m_customMenuBar` di `WindowWinRT`,
diisi dari `WindowDesc.customMenuBar` yang di-set oleh `App::init()` berdasarkan
argumen `Args::mode`.

`m_isDarkMode` dibaca dari registry **sebelum** `CreateWindowEx` agar
`WM_NCPAINT` pertama (yang dipicu saat window creation) langsung
mendapat warna theme yang benar — tidak perlu menunggu `applyTheme()`.

### Mode 1: Aero Enable (win-aero-enable) [default]

Tampilan modern DWM Win10+ — title bar tebal + **custom-drawn menu bar**
di non-client area menggunakan GDI (Ableton-inspired).

**Layout:**

```
┌─────────────────────────────────────┐
│ vk_demo_rendering            _ □ X  │  ← title bar tebal (DWM bawaan)
├╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┤
│ File  ░░░░  Help                    │  ← custom GDI, Segoe UI 11pt (Ableton-style)
├─────────────────────────────────────┤
│                                     │
│  Vulkan rendering (client area)     │
│                                     │
└─────────────────────────────────────┘
```

Menu bar digambar langsung di **non-client area** menggunakan GDI dengan
gaya **Ableton Live**: flat background, Segoe UI 11pt, spacing lega.

```cpp
// Layout constants
#define MENU_BAR_HEIGHT 32
#define MENU_PAD_LEFT   14   // padding kiri menu item pertama
#define MENU_ITEM_W     64   // lebar tiap item
#define MENU_GAP        20   // jarak antar item
```

**Non-client area sizing:**

`WM_NCCALCSIZE` menambah `MENU_BAR_HEIGHT` (32px) di bawah title bar:

```cpp
case WM_NCCALCSIZE:
    if (win->m_customMenuBar && wParam == TRUE) {
        DefWindowProc(hwnd, WM_NCCALCSIZE, TRUE, lParam);
        p->rgrc[0].top += MENU_BAR_HEIGHT;
        return 0;
    }
```

**Drawing (WM_NCPAINT) — Ableton-inspired:**

```cpp
case WM_NCPAINT: {
    if (win->m_customMenuBar) {
        DwmDefWindowProc(hwnd, WM_NCPAINT, wParam, lParam, &dwmRet);
        HDC hdc = GetWindowDC(hwnd);

        // Background — premium flat colors
        //   Dark:  #1E1E1E  (sangat gelap, premium)
        //   Light: #F5F5F5  (off-white, bersih)
        COLORREF bg = win->m_isDarkMode ? RGB(0x1E,0x1E,0x1E) : RGB(0xF5,0xF5,0xF5);
        FillRect(hdc, &rcMenu, CreateSolidBrush(bg));

        // Bottom separator — subtle
        COLORREF sep = win->m_isDarkMode ? RGB(0x38,0x38,0x38) : RGB(0xD0,0xD0,0xD0);
        // ... draw 1px line ...

        // Segoe UI 11pt — clean sans-serif (Ableton-style)
        HFONT hMenuFont = CreateFontW(
            -MulDiv(11, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, ..., L"Segoe UI");

        // Text: Dark #E8E8E8 / Light #282828
        SetTextColor(hdc, win->m_isDarkMode ? RGB(0xE8,0xE8,0xE8) : RGB(0x28,0x28,0x28));

        // File di x=14
        DrawTextW(hdc, L"File", -1, ..., DT_LEFT | DT_VCENTER);

        // Help di x=14+64+20=98
        DrawTextW(hdc, L"Help", -1, ..., DT_LEFT | DT_VCENTER);

        DeleteObject(hMenuFont);
        ReleaseDC(hwnd, hdc);
    }
}
```

**Hit-testing (WM_NCHITTEST):**

Intercept `HTCAPTION` → `HTMENU` jika kursor di area menu bar:

```cpp
case WM_NCHITTEST: {
    if (win->m_customMenuBar) {
        LRESULT ht = DefWindowProc(hwnd, WM_NCHITTEST, 0, lParam);
        if (ht == HTCAPTION) {
            int y  = pt.y - wr.top;
            int tb = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);
            if (y >= tb && y < tb + MENU_BAR_HEIGHT) return HTMENU;
        }
        return ht;
    }
}
```

**Klik menu (WM_NCLBUTTONDOWN):**

Popup via `TrackPopupMenu` — posisi mengikuti konstanta layout:

```cpp
case WM_NCLBUTTONDOWN: {
    if (win->m_customMenuBar && wParam == HTMENU) {
        int fileL = MENU_PAD_LEFT;
        int fileR = fileL + MENU_ITEM_W;
        int helpL = fileL + MENU_ITEM_W + MENU_GAP;
        int helpR = helpL + MENU_ITEM_W;

        if (x >= fileL && x < fileR)
            TrackPopupMenu(win->m_hMenuFile, ..., wr.left + fileL, ...);
        else if (x >= helpL && x < helpR)
            TrackPopupMenu(win->m_hMenuHelp, ..., wr.left + helpL, ...);
    }
}
```

`MENUINFO` dengan `MNS_AUTODISMISS` membuat popup otomatis tutup.

### Mode 2: Aero Disable (win-aero-disable)

Tampilan klasik Win32 — `SetMenu()` dengan menu bar tradisional.
Title bar tipis, cocok untuk Windows 7/8.1/10/11/12.

**Layout:**

```
┌─────────────────────────────────────┐
│ vk_demo_rendering            _ □ X  │  ← title bar tipis (OS bawaan)
├─────────────────────────────────────┤
│ File    Help                        │  ← menu bar via SetMenu()
├─────────────────────────────────────┤
│                                     │
│  Vulkan rendering (client area)     │
│                                     │
└─────────────────────────────────────┘
```

**Implementasi:**

```cpp
if (!m_customMenuBar) {
    // aero-disable: classic Win32 SetMenu
    m_hMenu = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, ID_FILE_EXIT, L"E&xit");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");
    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, ID_HELP_ABOUT, L"&About");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");
    SetMenu(m_hwnd, m_hMenu);
}
```

**Kelebihan:**
- Kompatibel dengan semua versi Windows (7/8.1/10/11/12)
- Keyboard shortcut built-in (`Alt+F`, `Alt+H`, `Alt+X`, `Alt+A`)
- Menu rendering ditangani oleh OS
- Tampilan klasik yang familiar

### Perbandingan

| Aspek | Aero Enable | Aero Disable |
|-------|-------------|--------------|
| Title bar | Tebal (DWM modern) | Tipis (Win32 klasik) |
| Menu bar | Custom GDI (Ableton-style) | `SetMenu()` (OS-rendered) |
| Client area | 800×568 (kurang 32px) | 800×568 (kurang 32px) |
| Font | Segoe UI 11pt | OS default |
| Keyboard shortcut | Manual | `Alt+F`, `Alt+H` (otomatis) |
| Theme support | Dark/light via GDI | Ikuti OS |

### Dampak ke Vulkan Rendering

Kedua mode menggunakan **32px** untuk menu bar.
`GetClientRect()` mengembalikan ukuran **setelah** pengurangan:

```
Window size:  800×600
Menu bar:     32px
Client area:  800×568   ← ini yang dipakai GetClientRect()
```

### Argumen CLI --mode

```
-m, --mode <mode>       Menu bar mode:
                          win-aero-enable   Modern DWM, custom NC menu bar
                          win-aero-disable  Classic Win32 with menu bar
                        (default: win-aero-enable)
```

### WM_COMMAND Handler

Kedua mode menggunakan handler yang sama:

```cpp
case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case ID_FILE_EXIT:
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;
    case ID_HELP_ABOUT:
        MessageBoxW(hwnd,
            L"vk_demo_rendering\n\n"
            L"Vulkan Triangle Demo\n"
            L"Built with Vulkan SDK + CMake",
            L"About", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    return 0;
```
