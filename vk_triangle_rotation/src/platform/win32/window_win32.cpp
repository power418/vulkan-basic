#include "window_win32.hpp"
#include <cstdio>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------------------
// Window procedure  (static)
// ---------------------------------------------------------------------------
LRESULT CALLBACK WindowWin32::wndProc(HWND hwnd, UINT msg,
                                      WPARAM wParam, LPARAM lParam) {
    // ---- NCCREATE: associate this with our C++ instance -------------------
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    auto win = reinterpret_cast<WindowWin32*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {

    // ---- close / destroy --------------------------------------------------
    case WM_CLOSE:
        if (win) win->m_shouldClose = true;
        DestroyWindow(hwnd);
        return 0;

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
        break; // Let DefWindowProc handle default cursor (arrow)

    case WM_DESTROY:
        win->m_shouldClose = true;
        win->m_hwnd = nullptr;          // mark gone, avoid double-destroy
        PostQuitMessage(0);             // still post so any GetMessage wakes
        return 0;

    // ---- paint: validate immediately to avoid repeat WM_PAINT -------------
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    // ---- modal loops (resize / menu) --------------------------------------
    // Menggunakan timer agar render tetap jalan (realtime) saat diresize / klik menu
    case WM_ENTERSIZEMOVE:
    case WM_ENTERMENULOOP:
        SetTimer(hwnd, 1, USER_TIMER_MINIMUM, nullptr);
        return 0;

    case WM_EXITSIZEMOVE:
    case WM_EXITMENULOOP:
        KillTimer(hwnd, 1);
        return 0;

    case WM_TIMER:
        if (wParam == 1 && win->m_renderCallback) {
            win->m_renderCallback();
        }
        return 0;

    // ---- size -------------------------------------------------------------
    case WM_SIZE:
        win->m_width  = LOWORD(lParam);
        win->m_height = HIWORD(lParam);
        return 0;

    // ---- menu commands (Win32 SetMenu) ------------------------------------
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

// ---------------------------------------------------------------------------
// create
// ---------------------------------------------------------------------------
bool WindowWin32::create(const WindowDesc& desc) {
    m_width  = desc.width;
    m_height = desc.height;
    m_hinst  = GetModuleHandle(nullptr);

    const char* CLASS = "VulkanWindowClass";

    WNDCLASS wc{};
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = m_hinst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;               // we paint every frame
    wc.lpszClassName = CLASS;

    if (!RegisterClass(&wc)) {
        // If class already exists (e.g. after re-creating a window in the same
        // process), that is fine – only the first call actually registers.
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            std::printf("ERROR: RegisterClass failed\n");
            return false;
        }
    }

    // TRUE for bMenu — we will attach a Win32 menu bar
    RECT r{0, 0, m_width, m_height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, TRUE);

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

    // ---- attach Win32 menu bar (classic SetMenu) --------------------------
    m_menuBar.create(m_hwnd);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    m_shouldClose = false;
    std::printf("Win32 window created (%dx%d), menubar=Win32-SetMenu\n",
                m_width, m_height);
    return true;
}

void WindowWin32::setCursorToHand(bool isHand) {
    if (m_isHandCursor != isHand) {
        m_isHandCursor = isHand;
        // Trigger WM_SETCURSOR update
        PostMessage(m_hwnd, WM_SETCURSOR, (WPARAM)m_hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    }
}

// ---------------------------------------------------------------------------
// destroy
// ---------------------------------------------------------------------------
void WindowWin32::destroy() {
    m_menuBar.destroy();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ---------------------------------------------------------------------------
// pollEvents  – returns false when the user requests to quit
// ---------------------------------------------------------------------------
bool WindowWin32::pollEvents() {
    MSG msg;

    // Pull all pending messages out of the queue
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return !m_shouldClose;
}

// ---------------------------------------------------------------------------
// createSurface
// ---------------------------------------------------------------------------
VkSurfaceKHR WindowWin32::createSurface(VkInstance instance) {
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

    std::printf("Win32 surface created\n");
    return m_surface;
}

// ---------------------------------------------------------------------------
// destroySurface
// ---------------------------------------------------------------------------
void WindowWin32::destroySurface(VkInstance instance) {
    if (m_surface) {
        vkDestroySurfaceKHR(instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// getFramebufferSize
// ---------------------------------------------------------------------------
void WindowWin32::getFramebufferSize(int& w, int& h) {
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

// ---------------------------------------------------------------------------
// getRequiredInstanceExtensions
// ---------------------------------------------------------------------------
std::vector<const char*> WindowWin32::getRequiredInstanceExtensions() const {
    return {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
}
