#pragma once

class Window;
struct Args;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool init(Window* window, const Args& args) = 0;
    virtual bool draw() = 0;
    virtual void cleanup() = 0;
};
