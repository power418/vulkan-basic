// Ensure commctrl is loaded before anything else
#ifndef _WIN32_IE
#define _WIN32_IE 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <commctrl.h>

#include <cstdio>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

    // Force DWM to immediately repaint the title bar:
    SendMessage(m_hwnd, WM_NCACTIVATE, FALSE, 0);
    SendMessage(m_hwnd, WM_NCACTIVATE, TRUE,  0);

    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_FRAMECHANGED | SWP_NOACTIVATE);

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
// Render child WndProc — Vulkan presents directly, no GDI painting needed
// ===========================================================================
LRESULT CALLBACK WindowWinRT::renderWndProc(HWND hwnd, UINT msg,
                                             WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    auto win = reinterpret_cast<WindowWinRT*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;   // Vulkan handles all rendering
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (win && win->m_mouseMoveCallback) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            win->m_mouseMoveCallback(x, y);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (win && win->m_mouseClickCallback) {
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);
            win->m_mouseClickCallback(x, y);
        }
        return 0;
    case WM_SETCURSOR:
        if (win && win->m_isHandCursor && LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        break; // Let DefWindowProc handle default cursor
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ===========================================================================
// Main window procedure  (static)
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

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

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
        win->m_renderHwnd = nullptr;   // child is auto-destroyed with parent
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

    // ---- modal loops (resize / menu) --------------------------------------
    case WM_ENTERSIZEMOVE:
    case WM_ENTERMENULOOP:
        SetTimer(hwnd, 2, USER_TIMER_MINIMUM, nullptr);
        return 0;

    case WM_EXITSIZEMOVE:
    case WM_EXITMENULOOP:
        KillTimer(hwnd, 2);
        return 0;

    // ---- size: resize menubar + render child ------------------------------
    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        win->m_width  = w;
        win->m_height = h;

        int menuH = win->m_winrtMenuBar.getHeight();
        win->m_winrtMenuBar.resize(w);
        if (win->m_renderHwnd) {
            SetWindowPos(win->m_renderHwnd, nullptr,
                         0, menuH, w, (h > menuH) ? h - menuH : 0,
                         SWP_NOZORDER);
        }
        return 0;
    }

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

    // ---- timers (theme read & render during resize) -----------------------
    case WM_TIMER:
        if (wParam == THEME_TIMER_ID) {
            KillTimer(hwnd, THEME_TIMER_ID);
            win->applyTheme(readDarkModeRegistry());
        } else if (wParam == 2 && win && win->m_renderCallback) {
            win->m_renderCallback();
        }
        return 0;

    // ---- menu commands (WinRT menubar sends WM_COMMAND to parent) ---------
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
    m_hinst  = GetModuleHandle(nullptr);

    // Read theme BEFORE CreateWindowEx so the first paint uses correct colors.
    m_isDarkMode = readDarkModeRegistry();

    // ---- register main window class ---------------------------------------
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

    // ---- register render child class --------------------------------------
    const char* RENDER_CLASS = "VulkanRenderClass";

    WNDCLASS rwc{};
    rwc.lpfnWndProc   = renderWndProc;
    rwc.hInstance      = m_hinst;
    rwc.lpszClassName  = RENDER_CLASS;

    if (!RegisterClass(&rwc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            std::printf("ERROR: RegisterClass (render) failed (err=%lu)\n",
                        GetLastError());
            return false;
        }
    }

    // ---- create main window -----------------------------------------------
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

    // ---- apply OS theme preference to DWM title bar -----------------------
    applyTheme(m_isDarkMode);
    SetWindowTextA(m_hwnd, desc.title.c_str());

    // ---- create WinRT client-area menu bar (child HWND) -------------------
    m_winrtMenuBar.create(m_hwnd);
    m_winrtMenuBar.setDarkMode(m_isDarkMode);

    // ---- create render child HWND (Vulkan surface target) -----------------
    RECT clientRC;
    GetClientRect(m_hwnd, &clientRC);
    int menuH = m_winrtMenuBar.getHeight();

    m_renderHwnd = CreateWindowEx(
        0, RENDER_CLASS, "",
        WS_CHILD | WS_VISIBLE,
        0, menuH,
        clientRC.right - clientRC.left,
        (clientRC.bottom - clientRC.top) - menuH,
        m_hwnd, nullptr, m_hinst, this);

    if (!m_renderHwnd) {
        std::printf("ERROR: render child HWND creation failed (err=%lu)\n",
                    GetLastError());
        return false;
    }

    // ---- WinRT: watch for system colour/theme changes --------------------
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
    // consistently on first display.
    applyTheme(m_isDarkMode);

    m_shouldClose = false;
    std::printf("WinRT window created (%dx%d), theme=%s, menubar=client-area\n",
                m_width, m_height,
                m_isDarkMode ? "dark" : "light");
    return true;
}

// ===========================================================================
// destroy
// ===========================================================================
void WindowWinRT::destroy() {
    m_winrtMenuBar.destroy();
    // Note: child HWNDs (render + menubar) are auto-destroyed with parent.
    // But if we explicitly destroyed menubar above, just destroy parent.
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_renderHwnd = nullptr;
}

void WindowWinRT::setCursorToHand(bool isHand) {
    if (m_isHandCursor != isHand) {
        m_isHandCursor = isHand;
        if (m_renderHwnd) {
            PostMessage(m_renderHwnd, WM_SETCURSOR, (WPARAM)m_renderHwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        } else if (m_hwnd) {
            PostMessage(m_hwnd, WM_SETCURSOR, (WPARAM)m_hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        }
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
// createSurface — uses the render child HWND (not the main window)
// ===========================================================================
VkSurfaceKHR WindowWinRT::createSurface(VkInstance instance) {
    if (m_surface)
        return m_surface;

    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hinstance = m_hinst;
    ci.hwnd      = m_renderHwnd;   // Vulkan renders into the child HWND

    if (vkCreateWin32SurfaceKHR(instance, &ci, nullptr, &m_surface)
        != VK_SUCCESS) {
        std::printf("ERROR: vkCreateWin32SurfaceKHR failed\n");
        return VK_NULL_HANDLE;
    }

    std::printf("Vulkan surface created (render child HWND)\n");
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
// getFramebufferSize — returns the render child HWND's client area
// ===========================================================================
void WindowWinRT::getFramebufferSize(int& w, int& h) {
    if (!m_renderHwnd) {
        w = m_width;
        h = m_height;
        return;
    }
    RECT r;
    GetClientRect(m_renderHwnd, &r);
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
