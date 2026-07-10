#pragma once
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>

#include "audio/audio.hpp"

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
    std::unique_ptr<audio::AudioSystem> m_audio;

    // BGM data
    std::vector<float> m_bgmFrames;
    int m_bgmSampleRate = 48000;
    int m_bgmChannels = 2;

    // SFX data
    std::vector<float> m_sfxFrames;
    
    // Polyphony Tracker
    std::mutex m_sfxMutex;
    std::vector<size_t> m_sfxPlayheads;
};
