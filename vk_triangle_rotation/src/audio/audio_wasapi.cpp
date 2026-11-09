#include "audio_wasapi.hpp"

#ifdef _WIN32
#include "../utils/logger.hpp"
#include <cstdio>
#include <avrt.h>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")

namespace audio {

// Helper untuk melepas COM object
template <class T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
std::unique_ptr<AudioSystem> createAudioSystem() {
    return std::make_unique<AudioWasapi>();
}

// ---------------------------------------------------------------------------
// AudioWasapi
// ---------------------------------------------------------------------------
AudioWasapi::AudioWasapi() {}

AudioWasapi::~AudioWasapi() {
    shutdown();
}

bool AudioWasapi::init(int sampleRate) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::printf("ERROR: CoInitializeEx failed for Audio\n");
        return false;
    }

    if (!initRender(sampleRate)) {
        std::printf("WARNING: Failed to init Audio Render (Speaker)\n");
    }
    if (!initCapture(sampleRate)) {
        std::printf("WARNING: Failed to init Audio Capture (Microphone)\n");
    }

    return true; // We return true even if one fails, as long as app can run
}

void AudioWasapi::shutdown() {
    stop();

    SafeRelease(&m_renderService);
    SafeRelease(&m_renderClient);
    SafeRelease(&m_renderDevice);

    SafeRelease(&m_captureService);
    SafeRelease(&m_captureClient);
    SafeRelease(&m_captureDevice);

    if (m_renderEvent) { CloseHandle(m_renderEvent); m_renderEvent = nullptr; }
    if (m_captureEvent) { CloseHandle(m_captureEvent); m_captureEvent = nullptr; }

    CoUninitialize();
}

void AudioWasapi::setInputCallback(AudioInputCallback cb) {
    m_inputCb = cb;
}

void AudioWasapi::setOutputCallback(AudioOutputCallback cb) {
    m_outputCb = cb;
}

bool AudioWasapi::start() {
    if (m_running) return true;
    m_running = true;

    if (m_renderClient) {
        if (FAILED(m_renderClient->Start())) {
            std::printf("ERROR: m_renderClient->Start() failed\n");
        } else {
            m_renderThread = std::thread(&AudioWasapi::renderThreadLoop, this);
        }
    }

    if (m_captureClient) {
        if (FAILED(m_captureClient->Start())) {
            std::printf("ERROR: m_captureClient->Start() failed\n");
        } else {
            m_captureThread = std::thread(&AudioWasapi::captureThreadLoop, this);
        }
    }

    return true;
}

void AudioWasapi::stop() {
    if (!m_running) return;
    m_running = false;

    // Wake up the threads if they are blocked
    if (m_renderEvent) SetEvent(m_renderEvent);
    if (m_captureEvent) SetEvent(m_captureEvent);

    if (m_renderThread.joinable()) m_renderThread.join();
    if (m_captureThread.joinable()) m_captureThread.join();

    if (m_renderClient) m_renderClient->Stop();
    if (m_captureClient) m_captureClient->Stop();
}

// ---------------------------------------------------------------------------
// Render (Speaker) Initialization
// ---------------------------------------------------------------------------
bool AudioWasapi::initRender(int sampleRate) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_renderDevice);
    SafeRelease(&pEnumerator);
    if (FAILED(hr)) return false;

    hr = m_renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_renderClient);
    if (FAILED(hr)) return false;

    // Format target: sampleRate, Stereo, 32-bit Float
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = 32;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    m_sampleRate = wfx.nSamplesPerSec;
    m_channels = wfx.nChannels;
    m_frameSize = wfx.nBlockAlign;

    // Gunakan durasi buffer 25ms (250,000 unit 100ns) untuk memberikan ruang aman
    // mencegah buffer underrun / suara glitch 'ttrrrrttt' saat GPU/CPU mengalami spike.
    REFERENCE_TIME hnsRequestedDuration = 250000;

    // Gunakan AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM agar OS otomatis menyesuaikan (Windows 10+)
    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | 0x80000000 /*AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM*/ | 0x08000000 /*AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY*/;

    hr = m_renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, hnsRequestedDuration, 0, &wfx, nullptr);
    if (FAILED(hr)) {
        // Fallback jika flag AUTOCONVERT gagal
        hr = m_renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsRequestedDuration, 0, &wfx, nullptr);
        if (FAILED(hr)) {
            LOG_ERROR("Failed to initialize Render AudioClient (Format mismatch)");
            return false;
        }
    }

    m_renderEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_renderClient->SetEventHandle(m_renderEvent);

    hr = m_renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_renderService);
    if (FAILED(hr)) return false;

    hr = m_renderClient->GetBufferSize(&m_renderBufferFrameCount);
    
    LOG_INFO("Audio Render (Speaker) initialized: %dHz, %d Channels (Buffer: %u frames, ~%.1f ms)",
             m_sampleRate, m_channels, m_renderBufferFrameCount,
             (m_sampleRate > 0) ? (m_renderBufferFrameCount * 1000.0f / m_sampleRate) : 0.0f);
    return true;
}

// ---------------------------------------------------------------------------
// Capture (Microphone) Initialization
// ---------------------------------------------------------------------------
bool AudioWasapi::initCapture(int sampleRate) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &m_captureDevice);
    SafeRelease(&pEnumerator);
    if (FAILED(hr)) return false;

    hr = m_captureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_captureClient);
    if (FAILED(hr)) return false;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = 32;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    REFERENCE_TIME hnsRequestedDuration = 10000000;

    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | 0x80000000 /*AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM*/ | 0x08000000 /*AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY*/;

    hr = m_captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, hnsRequestedDuration, 0, &wfx, nullptr);
    if (FAILED(hr)) {
        hr = m_captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsRequestedDuration, 0, &wfx, nullptr);
        if (FAILED(hr)) {
            std::printf("ERROR: Failed to initialize Capture AudioClient (Format mismatch)\n");
            return false;
        }
    }

    m_captureEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_captureClient->SetEventHandle(m_captureEvent);

    hr = m_captureClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureService);
    
    std::printf("Audio Capture (Microphone) initialized: %luHz, %d Channels\n", wfx.nSamplesPerSec, wfx.nChannels);
    return true;
}

// ---------------------------------------------------------------------------
// Threads
// ---------------------------------------------------------------------------
void AudioWasapi::renderThreadLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    
    auto lastWakeup = std::chrono::high_resolution_clock::now();
    uint64_t underrunCount = 0;

    while (m_running) {
        DWORD waitResult = WaitForSingleObject(m_renderEvent, 2000);
        if (!m_running) break;
        if (waitResult != WAIT_OBJECT_0) continue;

        auto now = std::chrono::high_resolution_clock::now();
        float intervalMs = std::chrono::duration<float, std::milli>(now - lastWakeup).count();
        lastWakeup = now;

        UINT32 numFramesPadding = 0;
        if (FAILED(m_renderClient->GetCurrentPadding(&numFramesPadding))) continue;

        // If buffer ran dry (padding 0 and interval was delayed), audio underrun occurred
        if (numFramesPadding == 0 && intervalMs > 20.0f) {
            underrunCount++;
            LOG_WARN("[HARDWARE-AUDIO] GLITCH / BUFFER UNDERRUN DETECTED (#%llu)! Interval: %.2f ms | Audio buffer dry -> Menghasilkan suara buzz 'ttrrrrttt'!",
                     underrunCount, intervalMs);
        }

        UINT32 numFramesAvailable = m_renderBufferFrameCount - numFramesPadding;
        if (numFramesAvailable == 0) continue;

        BYTE* pData = nullptr;
        if (SUCCEEDED(m_renderService->GetBuffer(numFramesAvailable, &pData))) {
            auto cbStart = std::chrono::high_resolution_clock::now();
            
            // Panggil callback milik engine jika di set
            if (m_outputCb) {
                m_outputCb(reinterpret_cast<float*>(pData), numFramesAvailable);
            } else {
                // Tulis silence (diam)
                std::memset(pData, 0, numFramesAvailable * m_frameSize);
            }

            float cbMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - cbStart).count();
            if (cbMs > 10.0f) {
                LOG_WARN("[HARDWARE-AUDIO] Audio mixer callback lambat (%.2f ms), memicu risiko buffer underrun!", cbMs);
            }
            
            m_renderService->ReleaseBuffer(numFramesAvailable, 0);
        }
    }

    if (hTask) AvRevertMmThreadCharacteristics(hTask);
    CoUninitialize();
}

void AudioWasapi::captureThreadLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    
    while (m_running) {
        DWORD waitResult = WaitForSingleObject(m_captureEvent, 2000);
        if (!m_running) break;
        if (waitResult != WAIT_OBJECT_0) continue;

        UINT32 packetLength = 0;
        while (SUCCEEDED(m_captureService->GetNextPacketSize(&packetLength)) && packetLength > 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags = 0;
            
            if (SUCCEEDED(m_captureService->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr))) {
                
                if (m_inputCb) {
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        // Jika silent, pData mungkin invalid/kosong
                        std::vector<float> silence(numFramesAvailable * m_channels, 0.0f);
                        m_inputCb(silence.data(), numFramesAvailable);
                    } else {
                        m_inputCb(reinterpret_cast<const float*>(pData), numFramesAvailable);
                    }
                }
                
                m_captureService->ReleaseBuffer(numFramesAvailable);
            }
        }
    }

    if (hTask) AvRevertMmThreadCharacteristics(hTask);
    CoUninitialize();
}

} // namespace audio
#endif
