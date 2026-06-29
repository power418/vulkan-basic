#include "native_window_win32.h"

LRESULT CALLBACK Window::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lparam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
    }

    auto win = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (win) {
        switch (message)
        {
            case WM_CLOSE:
                win->m_should_close = true;
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                win->m_should_close = true;
                win->hwnd = nullptr;
                PostQuitMessage(0);
                return 0;
            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_SIZE:
                win->m_width = LOWORD(lparam);
                win->m_height = HIWORD(lparam); // Fix: height is HIWORD(lparam) not wparam
                return 0;
        }
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

bool Window::create(const AppState& app)
{
    m_width = app.width;
    m_height = app.height;
    hinstance = GetModuleHandle(nullptr);

    LPCWSTR CLASS_NAME = L"VkWindowClass";

    WNDCLASSEXW wcx {};
    wcx.cbSize = sizeof(WNDCLASSEXW);
    wcx.lpfnWndProc = Window::window_proc;
    wcx.hInstance = hinstance;
    wcx.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcx.hbrBackground = nullptr;
    wcx.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wcx))
    {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            wchar_t buf[256];
            swprintf(buf, 256, L"RegisterClassExW failed: %lu", err);
            MessageBoxW(nullptr, buf, L"Error", MB_OK);
            return false;
        }
    }

    RECT rect {};
    rect.left = 0;
    rect.top = 0;
    rect.right = m_width;
    rect.bottom = m_height;

    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);

    hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        app.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hinstance,
        this
        );

    if (!hwnd) {
        DWORD err = GetLastError();
        wchar_t buf[256];
        swprintf(buf, 256, L"CreateWindowExW failed: %lu", err);
        MessageBoxW(nullptr, buf, L"Error", MB_OK);
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    m_should_close = false;
    std::cout << "Window craeted" << m_width << m_height << std::endl;
    return true;
}

void Window::destroy()
{
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

BOOL Window::poll_events() {
    MSG msg {};

    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return m_should_close == true;
}

VkSurfaceKHR Window::create_surface(VkInstance instance) {
    if (m_surface)
        return m_surface;

    VkWin32SurfaceCreateInfoKHR VkInfo {};
    VkInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    VkInfo.hinstance = hinstance;
    VkInfo.hwnd = hwnd;
    VkInfo.pNext = nullptr;

    if (vkCreateWin32SurfaceKHR(instance, &VkInfo, nullptr, &m_surface) != VK_SUCCESS) {
        std::cerr << "ERROR: vkCreateWin32SurfaceKHR() failed." << "\n";
        return VK_NULL_HANDLE;
    }

    std::cout << "Win32 surface created" << m_surface << std::endl;
    return m_surface;
}

void Window::destroy_surface(VkInstance instance) {
   if (hinstance != nullptr) {
       vkDestroySurfaceKHR(instance, m_surface, nullptr);
       m_surface = nullptr;
   }
}

void Window::get_framebuff_size(int& width, int& height) const {
    if (!hwnd) {
        width = m_width;
        height = m_height;
        return;
    }

    RECT rect {};
    GetClientRect(hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
}

std::vector<const char*> Window::get_required_instance_extensions() const {
    return { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
}