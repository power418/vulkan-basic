#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <functional>

struct WindowDesc {
    std::string title = "vk_demo_rendering";
    int width  = 800;
    int height = 600;
};

struct WindowBuffer {
    // Digunakan untuk menampung data/state spesifik window (misalnya untuk ImGui)
    bool showOption = false;
    std::vector<std::string> itemList;
};

class Window {
public:
    WindowBuffer window_buffer;

    virtual ~Window() = default;

    virtual bool create(const WindowDesc& desc) = 0;
    virtual void destroy() = 0;
    virtual bool pollEvents() = 0;

    virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;
    virtual VkSurfaceKHR getSurface() const = 0;
    virtual void destroySurface(VkInstance instance) = 0;

    virtual void* getHandle() const = 0;

    virtual void getFramebufferSize(int& w, int& h) = 0;
    virtual std::vector<const char*> getRequiredInstanceExtensions() const = 0;
    
    // Dipanggil untuk mendaftarkan fungsi render yang akan dieksekusi secara realtime
    virtual void setRenderCallback(std::function<void()> cb) {}

    // Mouse input
    virtual void setMouseMoveCallback(std::function<void(int x, int y)> cb) {}
    virtual void setMouseClickCallback(std::function<void(int x, int y)> cb) {}

    // Kursor
    virtual void setCursorToHand(bool isHand) {}
};
