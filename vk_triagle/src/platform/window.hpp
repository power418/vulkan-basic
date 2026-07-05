#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct WindowDesc {
    std::string title = "vk_demo_rendering";
    int width  = 800;
    int height = 600;
};

class Window {
public:
    virtual ~Window() = default;

    virtual bool create(const WindowDesc& desc) = 0;
    virtual void destroy() = 0;
    virtual bool pollEvents() = 0;

    virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;
    virtual VkSurfaceKHR getSurface() const = 0;
    virtual void destroySurface(VkInstance instance) = 0;

    virtual void getFramebufferSize(int& w, int& h) = 0;
    virtual std::vector<const char*> getRequiredInstanceExtensions() const = 0;
};
