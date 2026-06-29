#pragma once
#include <memory>

class Window;
class Renderer;
struct Args;

class App {
public:
    App(const Args& args);
    ~App();

    bool init();
    void run();

private:
    const Args&                m_args;
    std::unique_ptr<Window>   m_window;
    std::unique_ptr<Renderer> m_renderer;
};
