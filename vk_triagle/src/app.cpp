#include "app.hpp"
#include "args.hpp"
#include "platform/platform.hpp"
#include "renderer/vulkan_renderer.hpp"
#include <cstdio>

// ---------------------------------------------------------------------------
App::App(const Args& args)
    : m_args(args)
{}

App::~App() = default;

// ---------------------------------------------------------------------------
bool App::init() {
    WindowDesc wdesc;
    wdesc.title = "vk_demo_rendering";
    wdesc.width = m_args.width;
    wdesc.height = m_args.height;

    m_window = createPlatformWindow(m_args.mode);
    if (!m_window || !m_window->create(wdesc)) {
        std::printf("FATAL: window creation failed\n");
        return false;
    }

    m_renderer = std::make_unique<VulkanRenderer>();
    if (!m_renderer->init(m_window.get(), m_args)) {
        std::printf("FATAL: renderer init failed\n");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
void App::run() {
    while (m_window->pollEvents()) {
        int w = 0, h = 0;
        m_window->getFramebufferSize(w, h);
        if (w == 0 || h == 0) continue;
        m_renderer->draw();
    }

    // Explicit cleanup – Renderer needs Window alive for surface teardown,
    // so we destroy the renderer before the window.
    m_renderer.reset();
    m_window.reset();
}
