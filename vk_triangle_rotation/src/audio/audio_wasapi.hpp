#pragma once

#include "audio.hpp"

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <thread>
#include <atomic>

namespace audio {

class AudioWasapi : public AudioSystem {
public:
    AudioWasapi();
    ~AudioWasapi() override;

    bool init(int sampleRate = 48000) override;
    void shutdown() override;

    void setInputCallback(AudioInputCallback cb) override;
    void setOutputCallback(AudioOutputCallback cb) override;

    bool start() override;
    void stop() override;

    int getSampleRate() const override { return m_sampleRate; }
    int getChannels() const override { return m_channels; }

private:
    bool initRender(int sampleRate);
    bool initCapture(int sampleRate);
    
    void renderThreadLoop();
    void captureThreadLoop();

private:
    AudioInputCallback  m_inputCb;
    AudioOutputCallback m_outputCb;

    int m_sampleRate = 48000;
    int m_channels = 2;
    int m_frameSize = 0; // bytes per frame

    std::atomic<bool> m_running{false};

    // -- Render --
    IMMDevice*          m_renderDevice  = nullptr;
    IAudioClient*       m_renderClient  = nullptr;
    IAudioRenderClient* m_renderService = nullptr;
    HANDLE              m_renderEvent   = nullptr;
    std::thread         m_renderThread;
    UINT32              m_renderBufferFrameCount = 0;

    // -- Capture --
    IMMDevice*           m_captureDevice  = nullptr;
    IAudioClient*        m_captureClient  = nullptr;
    IAudioCaptureClient* m_captureService = nullptr;
    HANDLE               m_captureEvent   = nullptr;
    std::thread          m_captureThread;
};

} // namespace audio

#endif // _WIN32
