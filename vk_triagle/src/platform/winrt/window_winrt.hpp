#pragma once
#include "../window.hpp"
#include <windows.h>
#include <dwmapi.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include "menubar.hpp"           // WinRT client-area menu bar

// Custom message posted by the WinRT theme-change watcher
#define WM_THEMECHANGE (WM_APP + 1)

class WindowWinRT : public Window {
public:
    WindowWinRT();
    ~WindowWinRT() override;

    bool create(const WindowDesc& desc) override;
    void destroy() override;
    bool pollEvents() override;

    VkSurfaceKHR createSurface(VkInstance instance) override;
    VkSurfaceKHR getSurface() const override { return m_surface; }
    void destroySurface(VkInstance instance) override;

    void getFramebufferSize(int& w, int& h) override;
    std::vector<const char*> getRequiredInstanceExtensions() const override;

    bool isCloseRequested() const { return m_shouldClose; }

private:
    HWND         m_hwnd       = nullptr;   // main top-level window
    HWND         m_renderHwnd = nullptr;   // child HWND for Vulkan rendering
    HINSTANCE    m_hinst      = nullptr;
    VkSurfaceKHR m_surface    = VK_NULL_HANDLE;
    int  m_width  = 800;
    int  m_height = 600;
    bool m_shouldClose = false;
    bool m_winrtInited  = false;
    bool m_isDarkMode   = false;

    // WinRT UISettings – must stay alive for ColorValuesChanged to fire
    winrt::Windows::UI::ViewManagement::UISettings m_uiSettings{nullptr};
    void* m_colorToken = nullptr;

    // Menu bar — always WinRT client-area child HWND (full WinRT mode)
    WinRTMenuBar m_winrtMenuBar;

    void applyTheme(bool dark);
    static bool isSystemDarkMode();

    // WndProc for the render child (Vulkan surface target)
    static LRESULT CALLBACK renderWndProc(HWND, UINT, WPARAM, LPARAM);

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
};
