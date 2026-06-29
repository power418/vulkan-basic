/**
 *  
 *
 **/
#ifndef NATIVE_WINDOW_WIN32_H
#define NATIVE_WINDOW_WIN32_H

#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include <Windows.h>
// #include <consoleapi.h>

#include <vulkan/vulkan.h>

class Window 
{
public:
    struct AppState 
    {
        std::wstring title = L"Hello, Win32 for Rendering with Vulkan!";

        int width  = 800;
        int height = 600;

        bool menubar = true;
        bool resizable = true;
    };
public:
    Window() = default;
    virtual ~Window() = default;

    bool create(const AppState& app);
    void destroy();
    BOOL poll_events();

    VkSurfaceKHR create_surface(VkInstance instance);
    void destroy_surface(VkInstance instance);

    HWND handler() const;

    void get_framebuff_size(int& width, int& height) const;

    std::vector<const char*> get_required_instance_extensions() const;

private:
    HWND hwnd = nullptr;
    HINSTANCE hinstance = nullptr;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    int m_width;
    int m_height;

    BOOL m_should_close = false;

    static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
};

#endif // Native Window for Win32
