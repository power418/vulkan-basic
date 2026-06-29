// Ensure commctrl is loaded before anything else
#ifndef _WIN32_IE
#define _WIN32_IE 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <VersionHelpers.h>

#include <cstdio>

// Timer ID for deferred theme registry read (allow registry to settle)
#define THEME_TIMER_ID  1001

// ---- C++/WinRT ------------------------------------------------------------
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Core.h>

// ---- local header (after windows.h so WIN32_LEAN_AND_MEAN doesn't matter) -
#include "window_winrt.hpp"

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::ViewManagement;

// ===========================================================================
// Helper: read OS dark-mode preference from the registry
// ===========================================================================
static bool readDarkModeRegistry() {
    DWORD value = 1;      // 1 = light, 0 = dark; light is the safe default
    DWORD size  = sizeof(value);
    LSTATUS st = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_DWORD, nullptr, &value, &size);
    if (st == ERROR_SUCCESS) return value == 0;   // 0 → dark mode
    return false;
}

// ===========================================================================
// Apply dark / light title bar via DWM
// ===========================================================================
void WindowWinRT::applyTheme(bool dark) {
    m_isDarkMode = dark;
    m_winrtMenuBar.setDarkMode(dark);

    if (!m_hwnd) return;

    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(m_hwnd, 20, &value, sizeof(value));

    // Force DWM to immediately repaint the title bar without needing
    // to switch focus to another window:
    // 1) Toggle WM_NCACTIVATE to force DWM to re-render the NC area
    SendMessage(m_hwnd, WM_NCACTIVATE, FALSE, 0);
    SendMessage(m_hwnd, WM_NCACTIVATE, TRUE,  0);

    // 2) SWP_FRAMECHANGED to recalculate the NC area geometry
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_FRAMECHANGED | SWP_NOACTIVATE);

    // 3) Redraw both the NC frame and the client area
    RedrawWindow(m_hwnd, nullptr, nullptr,
                 RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW |
                 RDW_ALLCHILDREN);

    std::printf("Theme: %s\n", dark ? "DARK" : "LIGHT");
}

// ===========================================================================
// Check if the OS currently wants dark mode
// ===========================================================================
bool WindowWinRT::isSystemDarkMode() {
    return readDarkModeRegistry();
}

// ===========================================================================
// Constructor
// ===========================================================================
WindowWinRT::WindowWinRT() {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    m_winrtInited = true;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    std::printf("WinRT apartment initialized + comctl32 loaded\n");
}

// ===========================================================================
// Destructor
// ===========================================================================
WindowWinRT::~WindowWinRT() {
    destroy();

    if (m_winrtInited) {
        if (m_colorToken) {
            delete static_cast<winrt::event_token*>(m_colorToken);
            m_colorToken = nullptr;
        }
        winrt::uninit_apartment();
    }
}

// ===========================================================================
// Window procedure  (static)
// ===========================================================================
LRESULT CALLBACK WindowWinRT::wndProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    auto win = reinterpret_cast<WindowWinRT*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {

    case WM_CLOSE:
        win->m_shouldClose = true;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        win->m_shouldClose = true;
        win->m_hwnd = nullptr;
        KillTimer(hwnd, THEME_TIMER_ID);
        // Unsubscribe and clean up WinRT theme watcher
        if (win->m_colorToken) {
            try {
                auto tok = *static_cast<winrt::event_token*>(win->m_colorToken);
                win->m_uiSettings.ColorValuesChanged(tok);
            } catch (...) { /* ignore */ }
            delete static_cast<winrt::event_token*>(win->m_colorToken);
            win->m_colorToken = nullptr;
        }
        PostQuitMessage(0);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        win->m_width  = LOWORD(lParam);
        win->m_height = HIWORD(lParam);
        return 0;

    // ---- menu bar: delegate to WinRTMenuBar or fall through to default ----
    case WM_NCCALCSIZE:
        if (win->m_useCustomMenu && win->m_winrtMenuBar.onNcCalcSize(wParam, lParam))
            return 0;
        break;

    case WM_NCPAINT:
        if (win->m_useCustomMenu)
            return win->m_winrtMenuBar.onNcPaint(hwnd, wParam, lParam) ? 0 : -1;
        break;

    case WM_NCHITTEST: {
        LRESULT ht;
        if (win->m_useCustomMenu && win->m_winrtMenuBar.onNcHitTest(lParam, ht))
            return ht;
        break;
    }

    case WM_NCLBUTTONDOWN:
        if (win->m_useCustomMenu && win->m_winrtMenuBar.onNcLButtonDown(hwnd, wParam, lParam))
            return 0;
        break;

    // ---- OS theme change signals -----------------------------------------
    case WM_THEMECHANGE:
        SetTimer(hwnd, THEME_TIMER_ID, 150, nullptr);
        return 0;

    case WM_SETTINGCHANGE:
        if (wParam == 0 && lParam) {
            auto str = reinterpret_cast<const wchar_t*>(lParam);
            if (wcscmp(str, L"ImmersiveColorSet") == 0) {
                SetTimer(hwnd, THEME_TIMER_ID, 150, nullptr);
            }
        }
        return 0;

    // ---- deferred theme read (registry settled) ---------------------------
    case WM_TIMER:
        if (wParam == THEME_TIMER_ID) {
            KillTimer(hwnd, THEME_TIMER_ID);
            win->applyTheme(readDarkModeRegistry());
        }
        return 0;

    // ---- menu commands (both menu bar modes) ------------------------------
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

    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ===========================================================================
// create
// ===========================================================================
bool WindowWinRT::create(const WindowDesc& desc) {
    m_width  = desc.width;
    m_height = desc.height;
    m_useCustomMenu = desc.customMenuBar;
    m_hinst  = GetModuleHandle(nullptr);

    // Read theme BEFORE CreateWindowEx so the first WM_NCPAINT
    // (which fires during window creation) uses correct colors.
    m_isDarkMode = readDarkModeRegistry();

    const char* CLASS = "VulkanWinRTClass";

    WNDCLASS wc{};
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = m_hinst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = CLASS;

    if (!RegisterClass(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            std::printf("ERROR: RegisterClass failed (err=%lu)\n",
                        GetLastError());
            return false;
        }
    }

    RECT r{0, 0, m_width, m_height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowEx(
        0, CLASS, desc.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, m_hinst, this);

    if (!m_hwnd) {
        std::printf("ERROR: CreateWindowEx failed (err=%lu)\n",
                    GetLastError());
        return false;
    }

    // ---- apply OS theme preference to title bar ---------------------------
    applyTheme(m_isDarkMode);

    // Ensure the title is set explicitly (DWM dark mode can wipe the text)
    SetWindowTextA(m_hwnd, desc.title.c_str());

    // ---- create menu bar (delegate to the appropriate class) --------------
    if (m_useCustomMenu) {
        // aero-disable: WinRT custom NC menu bar (Ableton-style dark)
        m_winrtMenuBar.create(m_hwnd);
    } else {
        // aero-enable: classic Win32 SetMenu
        m_win32MenuBar.create(m_hwnd);
    }

    // ---- WinRT: watch for system colour/theme changes --------------------
    // UISettings must stay alive for the lifetime of the window, otherwise
    // the ColorValuesChanged callback is cancelled when the object is destroyed.
    try {
        m_uiSettings = winrt::Windows::UI::ViewManagement::UISettings();
        auto token = m_uiSettings.ColorValuesChanged(
            [hwnd = m_hwnd](auto&&...) {
                PostMessage(hwnd, WM_THEMECHANGE, 0, 0);
            });
        m_colorToken = new winrt::event_token(token);
        std::printf("WinRT: registered theme-change watcher\n");
    } catch (winrt::hresult_error const&) {
        std::printf("WinRT: theme watcher unavailable\n");
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // Re-apply theme AFTER ShowWindow so the DWM title bar renders
    // consistently on first display (no need to alt-tab / switch focus).
    applyTheme(m_isDarkMode);

    m_shouldClose = false;
    std::printf("WinRT window created (%dx%d), theme=%s, mode=%s\n",
                m_width, m_height,
                m_isDarkMode ? "dark" : "light",
                m_useCustomMenu ? "aero-disable-custom" : "aero-enable-win32");
    return true;
}

// ===========================================================================
// destroy
// ===========================================================================
void WindowWinRT::destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_useCustomMenu) {
        m_winrtMenuBar.destroy();
    } else {
        m_win32MenuBar.destroy();
    }
}

// ===========================================================================
// pollEvents
// ===========================================================================
bool WindowWinRT::pollEvents() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !m_shouldClose;
}

// ===========================================================================
// createSurface
// ===========================================================================
VkSurfaceKHR WindowWinRT::createSurface(VkInstance instance) {
    if (m_surface)
        return m_surface;

    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hinstance = m_hinst;
    ci.hwnd      = m_hwnd;

    if (vkCreateWin32SurfaceKHR(instance, &ci, nullptr, &m_surface)
        != VK_SUCCESS) {
        std::printf("ERROR: vkCreateWin32SurfaceKHR failed\n");
        return VK_NULL_HANDLE;
    }

    std::printf("Win32 surface created (via WinRT window)\n");
    return m_surface;
}

// ===========================================================================
// destroySurface
// ===========================================================================
void WindowWinRT::destroySurface(VkInstance instance) {
    if (m_surface) {
        vkDestroySurfaceKHR(instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

// ===========================================================================
// getFramebufferSize
// ===========================================================================
void WindowWinRT::getFramebufferSize(int& w, int& h) {
    if (!m_hwnd) {
        w = m_width;
        h = m_height;
        return;
    }
    RECT r;
    GetClientRect(m_hwnd, &r);
    w = r.right - r.left;
    h = r.bottom - r.top;
}

// ===========================================================================
// getRequiredInstanceExtensions
// ===========================================================================
std::vector<const char*> WindowWinRT::getRequiredInstanceExtensions() const {
    return {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
}
