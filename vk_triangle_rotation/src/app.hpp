#pragma once
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
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
    struct SoundBuffer {
        std::string filename;
        std::vector<float> frames;
        int sampleRate = 48000;
        int channels = 2;
    };

    struct ActiveSound {
        size_t soundIndex = 0;
        size_t playhead = 0;
    };

    struct BgmMuteNode {
        bool muted = false;
        BgmMuteNode* prev = nullptr;
        BgmMuteNode* next = nullptr;
    };

    const Args&                m_args;
    std::unique_ptr<Window>   m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<audio::AudioSystem> m_audio;

    // BGM data
    std::vector<float> m_bgmFrames;
    int m_bgmSampleRate = 48000;
    int m_bgmChannels = 2;
    BgmMuteNode m_bgmEnabledNode;
    BgmMuteNode m_bgmMutedNode;
    BgmMuteNode* m_bgmMuteState = nullptr;

    // SFX data
    std::vector<SoundBuffer> m_sfxSounds;
    size_t m_focusedSoundIndex = 0;
    
    // Polyphony Tracker
    std::mutex m_sfxMutex;
    std::vector<ActiveSound> m_activeSounds;
};
