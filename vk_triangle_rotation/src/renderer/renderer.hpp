#pragma once
#include <functional>
#include <string>

class Window;
struct Args;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool init(Window* window, const Args& args) = 0;
    virtual bool draw() = 0;
    virtual void cleanup() = 0;
    
    virtual float getAngle() const { return 0.0f; }
    virtual void handleMouseClick(int x, int y) {}
    virtual void setCharacterAudioCallback(std::function<void(const std::string&)> callback) {}
    virtual void setBgmMuteCallback(std::function<void(bool)> callback) {}
};
