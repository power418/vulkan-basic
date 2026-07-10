#pragma once

#include <vector>
#include <functional>
#include <memory>

namespace audio {

// Callback untuk menerima data dari Microphone (Capture)
// Param: data audio float32 (stereo atau mono), jumlah frame
using AudioInputCallback = std::function<void(const float* data, size_t frames)>;

// Callback untuk menyuplai data ke Speaker (Render)
// Param: buffer output (float32), jumlah frame yang diminta
using AudioOutputCallback = std::function<void(float* outData, size_t frames)>;

class AudioSystem {
public:
    virtual ~AudioSystem() = default;

    virtual bool init(int sampleRate = 48000) = 0;
    virtual void shutdown() = 0;

    // Daftarkan callback input dan output
    virtual void setInputCallback(AudioInputCallback cb) = 0;
    virtual void setOutputCallback(AudioOutputCallback cb) = 0;

    // Mulai atau hentikan aliran audio
    virtual bool start() = 0;
    virtual void stop() = 0;

    // Ambil sample rate yang disepakati oleh sistem OS (misal: 48000)
    virtual int getSampleRate() const = 0;
    
    // Ambil jumlah channel (biasanya 2 untuk Stereo)
    virtual int getChannels() const = 0;
};

// Factory function
std::unique_ptr<AudioSystem> createAudioSystem();

} // namespace audio
