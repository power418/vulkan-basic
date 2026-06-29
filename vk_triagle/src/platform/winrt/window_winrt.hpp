#pragma once
#include "../window.hpp"
#include <windows.h>
#include <dwmapi.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include "menubar.hpp"           // WinRT custom NC menu bar
#include "../win32/menubar.hpp"  // Win32 classic SetMenu

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
    HWND         m_hwnd     = nullptr;
    HINSTANCE    m_hinst    = nullptr;
    VkSurfaceKHR m_surface  = VK_NULL_HANDLE;
    int  m_width  = 800;
    int  m_height = 600;
    bool m_shouldClose = false;
    bool m_winrtInited  = false;
    bool m_isDarkMode   = false;

    // WinRT UISettings – must stay alive for ColorValuesChanged to fire
    winrt::Windows::UI::ViewManagement::UISettings m_uiSettings{nullptr};
    void* m_colorToken = nullptr;

    // Menu bar — runtime switch via --mode
    bool m_useCustomMenu = false;  // true = WinRTMenuBar, false = Win32MenuBar
    WinRTMenuBar m_winrtMenuBar;
    Win32MenuBar m_win32MenuBar;

    void applyTheme(bool dark);
    static bool isSystemDarkMode();

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
};
