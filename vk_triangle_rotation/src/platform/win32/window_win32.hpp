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

    void* getHandle() const override { return m_hwnd; }

    void getFramebufferSize(int& w, int& h) override;
    std::vector<const char*> getRequiredInstanceExtensions() const override;

    void setRenderCallback(std::function<void()> cb) override { m_renderCallback = cb; }

    void setMouseMoveCallback(std::function<void(int x, int y)> cb) override { m_mouseMoveCallback = cb; }
    void setMouseClickCallback(std::function<void(int x, int y)> cb) override { m_mouseClickCallback = cb; }

    void setCursorToHand(bool isHand) override;

    bool isCloseRequested() const { return m_shouldClose; }

private:
    HWND         m_hwnd     = nullptr;
    HINSTANCE    m_hinst    = nullptr;
    VkSurfaceKHR m_surface  = VK_NULL_HANDLE;
    int  m_width  = 800;
    int  m_height = 600;
    bool m_shouldClose = false;

    Win32MenuBar m_menuBar;
    std::function<void()> m_renderCallback;
    std::function<void(int x, int y)> m_mouseMoveCallback;
    std::function<void(int x, int y)> m_mouseClickCallback;
    bool m_isHandCursor = false;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
};
