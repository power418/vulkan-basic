#pragma once
#include "../window.hpp"
#include "menubar.hpp"
#include <windows.h>

class WindowWin32 : public Window {
public:
    WindowWin32() = default;
    ~WindowWin32() override { destroy(); }

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

    Win32MenuBar m_menuBar;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
};
