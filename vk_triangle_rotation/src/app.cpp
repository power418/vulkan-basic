#include "app.hpp"
#include "args.hpp"
#include "platform/platform.hpp"
#include "renderer/vulkan_renderer.hpp"

#include <cstdio>
#include <vector>

#define DR_MP3_IMPLEMENTATION
#include "../utils/dr_mp3.h"
#include <cmath>
#include "../utils/math3d.hpp"

// Helper: resolve filename relative to executable directory
static std::string getExeRelativePath(const char* filename) {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return filename;
    char* sep = strrchr(buf, '\\');
    if (sep) *(sep + 1) = '\0'; else buf[0] = '\0';
    return std::string(buf) + filename;
}

// Helper Hit Testing
static Vec4 mulMat4Vec4(const Mat4& m, const Vec4& v) {
    Vec4 r;
    r.x = m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z + m.m[0][3]*v.w;
    r.y = m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z + m.m[1][3]*v.w;
    r.z = m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z + m.m[2][3]*v.w;
    r.w = m.m[3][0]*v.x + m.m[3][1]*v.y + m.m[3][2]*v.z + m.m[3][3]*v.w;
    return r;
}

static bool pointInTriangle(float px, float py, float x1, float y1, float x2, float y2, float x3, float y3) {
    auto sign = [](float p1x, float p1y, float p2x, float p2y, float p3x, float p3y) {
        return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y);
    };
    bool b1 = sign(px, py, x1, y1, x2, y2) < 0.0f;
    bool b2 = sign(px, py, x2, y2, x3, y3) < 0.0f;
    bool b3 = sign(px, py, x3, y3, x1, y1) < 0.0f;
    return ((b1 == b2) && (b2 == b3));
}

// ---------------------------------------------------------------------------
App::App(const Args& args) : m_args(args) {
    m_window = createPlatformWindow(args.mode);
    if (!m_window) {
        std::printf("FATAL: Failed to create window for mode: %s\n", args.mode.c_str());
        std::exit(1);
    }
    m_renderer = std::make_unique<VulkanRenderer>();
    m_audio = audio::createAudioSystem();
}

App::~App() {
    if (m_audio) {
        m_audio->shutdown();
    }
    if (m_renderer) {
        m_renderer->cleanup();
    }
    if (m_window) {
        m_window->destroy();
    }
}

// ---------------------------------------------------------------------------
bool App::init() {
    WindowDesc wdesc;
    wdesc.title = "vk_demo_rendering";
    wdesc.width = m_args.width;
    wdesc.height = m_args.height;

    if (!m_window->create(wdesc)) {
        std::printf("FATAL: window creation failed\n");
        return false;
    }

    // Initialize ImGui window buffer state
    m_window->window_buffer.showOption = true;
    m_window->window_buffer.itemList = {
        "Setup complete",
        "Load 3D Triangle",
        "Start Rendering",
        "Hover to see UI"
    };

    if (!m_renderer->init(m_window.get(), m_args)) {
        std::printf("FATAL: renderer init failed\n");
        return false;
    }

    // Load MP3 BGM
    drmp3_config mp3Config;
    drmp3_uint64 totalFrames = 0;
    auto bgmPath = getExeRelativePath("fish.mp3");
    float* pFrames = drmp3_open_file_and_read_pcm_frames_f32(bgmPath.c_str(), &mp3Config, &totalFrames, nullptr);
    if (pFrames) {
        m_bgmSampleRate = mp3Config.sampleRate;
        m_bgmChannels = mp3Config.channels;
        size_t totalSamples = static_cast<size_t>(totalFrames) * m_bgmChannels;
        m_bgmFrames.assign(pFrames, pFrames + totalSamples);
        drmp3_free(pFrames, nullptr);
        std::printf("BGM Loaded: %zu frames, %d Hz, %d channels\n", static_cast<size_t>(totalFrames), m_bgmSampleRate, m_bgmChannels);
    } else {
        std::printf("WARNING: Failed to load fish.mp3 from: %s\n", bgmPath.c_str());
    }

    // Load SFX
    auto sfxPath = getExeRelativePath("hidup_jokowi.mp3");
    float* pSfxFrames = drmp3_open_file_and_read_pcm_frames_f32(sfxPath.c_str(), &mp3Config, &totalFrames, nullptr);
    if (pSfxFrames) {
        size_t totalSamples = static_cast<size_t>(totalFrames) * mp3Config.channels;
        m_sfxFrames.assign(pSfxFrames, pSfxFrames + totalSamples);
        drmp3_free(pSfxFrames, nullptr);
        std::printf("SFX Loaded: %zu frames, %d channels\n", static_cast<size_t>(totalFrames), mp3Config.channels);
    } else {
        std::printf("WARNING: Failed to load hidup_jokowi.mp3 from: %s\n", sfxPath.c_str());
    }

    if (m_audio && m_audio->init(m_bgmSampleRate)) {
        // Callback untuk Output (Speaker) - Looping BGM MP3 + SFX Polyphony Mixer
        m_audio->setOutputCallback([this](float* outData, size_t frames) {
            static size_t currentFrame = 0;
            size_t framesTotal = m_bgmFrames.size() / m_bgmChannels;
            size_t sfxTotalFrames = m_sfxFrames.size() / m_bgmChannels;

            std::lock_guard<std::mutex> lock(m_sfxMutex);

            for (size_t i = 0; i < frames; ++i) {
                float mixedL = 0.0f;
                float mixedR = 0.0f;

                // 1. Ambil sampel BGM
                if (!m_bgmFrames.empty()) {
                    if (currentFrame >= framesTotal) {
                        currentFrame = 0; // Loop kembali ke awal
                    }
                    if (m_bgmChannels == 1) {
                        mixedL += m_bgmFrames[currentFrame];
                        mixedR += m_bgmFrames[currentFrame];
                    } else if (m_bgmChannels == 2) {
                        mixedL += m_bgmFrames[currentFrame * 2 + 0];
                        mixedR += m_bgmFrames[currentFrame * 2 + 1];
                    }
                    currentFrame++;
                }

                // 2. Ambil sampel SFX (Polyphony / Bertumpuk)
                if (!m_sfxFrames.empty()) {
                    for (size_t& sfxHead : m_sfxPlayheads) {
                        if (sfxHead < sfxTotalFrames) {
                            if (m_bgmChannels == 1) {
                                mixedL += m_sfxFrames[sfxHead];
                                mixedR += m_sfxFrames[sfxHead];
                            } else if (m_bgmChannels == 2) {
                                mixedL += m_sfxFrames[sfxHead * 2 + 0];
                                mixedR += m_sfxFrames[sfxHead * 2 + 1];
                            }
                            sfxHead++;
                        }
                    }
                }

                // 3. Gabungkan dan pastikan tidak ada clipping (hard clipping simple)
                if (mixedL > 1.0f) mixedL = 1.0f;
                if (mixedL < -1.0f) mixedL = -1.0f;
                if (mixedR > 1.0f) mixedR = 1.0f;
                if (mixedR < -1.0f) mixedR = -1.0f;

                outData[i * 2 + 0] = mixedL;
                outData[i * 2 + 1] = mixedR;
            }
            
            // Hapus playhead yang sudah selesai (mencapai akhir lagu)
            auto it = m_sfxPlayheads.begin();
            while (it != m_sfxPlayheads.end()) {
                if (*it >= sfxTotalFrames) {
                    it = m_sfxPlayheads.erase(it);
                } else {
                    ++it;
                }
            }
        });

        // Callback untuk Input (Mic)
        m_audio->setInputCallback([](const float* inData, size_t frames) {});

        m_audio->start();
    }

    // Set callback agar render tetap jalan saat Win32 message loop tertahan (resize/menu)
    m_window->setRenderCallback([this]() {
        int w = 0, h = 0;
        m_window->getFramebufferSize(w, h);
        if (w > 0 && h > 0 && m_renderer) {
            m_renderer->draw();
        }
    });

    // Callback now merged below after isHovering is defined

    // Caching status hit agar tidak selalu update cursor ke OS
    static bool isHovering = false;

    m_window->setMouseMoveCallback([this](int mx, int my) {
        int w = 0, h = 0;
        m_window->getFramebufferSize(w, h);
        if (w == 0 || h == 0 || !m_renderer) return;

        float aspect = (float)w / (float)h;
        float angle = m_renderer->getAngle();
        
        Mat4 proj = Mat4::perspective(1.0472f, aspect, 0.1f, 10.0f);
        Mat4 model = Mat4::translation(0.0f, 0.0f, 3.5f) * Mat4::rotationY(angle);
        Mat4 mvp = proj * model;

        // 5 Titik sudut Piramida
        Vec4 apex(0.0f, -1.0f, 0.0f, 1.0f);
        Vec4 bfl(-1.0f,  1.0f, -1.0f, 1.0f);
        Vec4 bfr( 1.0f,  1.0f, -1.0f, 1.0f);
        Vec4 bbr( 1.0f,  1.0f,  1.0f, 1.0f);
        Vec4 bbl(-1.0f,  1.0f,  1.0f, 1.0f);

        auto projectToScreen = [&](const Vec4& v) -> std::pair<float, float> {
            Vec4 pv = mulMat4Vec4(mvp, v);
            pv.x /= pv.w; pv.y /= pv.w;
            return { (pv.x + 1.0f) * 0.5f * w, (pv.y + 1.0f) * 0.5f * h };
        };

        auto pApex = projectToScreen(apex);
        auto pBfl = projectToScreen(bfl);
        auto pBfr = projectToScreen(bfr);
        auto pBbr = projectToScreen(bbr);
        auto pBbl = projectToScreen(bbl);

        // Cek 6 segitiga (4 sisi + 2 alas)
        bool hit = false;
        if (pointInTriangle(mx, my, pApex.first, pApex.second, pBfr.first, pBfr.second, pBfl.first, pBfl.second)) hit = true;
        if (!hit && pointInTriangle(mx, my, pApex.first, pApex.second, pBbr.first, pBbr.second, pBfr.first, pBfr.second)) hit = true;
        if (!hit && pointInTriangle(mx, my, pApex.first, pApex.second, pBbl.first, pBbl.second, pBbr.first, pBbr.second)) hit = true;
        if (!hit && pointInTriangle(mx, my, pApex.first, pApex.second, pBfl.first, pBfl.second, pBbl.first, pBbl.second)) hit = true;
        if (!hit && pointInTriangle(mx, my, pBfl.first, pBfl.second, pBfr.first, pBfr.second, pBbr.first, pBbr.second)) hit = true;
        if (!hit && pointInTriangle(mx, my, pBfl.first, pBfl.second, pBbr.first, pBbr.second, pBbl.first, pBbl.second)) hit = true;

        if (hit != isHovering) {
            isHovering = hit;
            m_window->setCursorToHand(hit);
        }
    });

    m_window->setMouseClickCallback([this](int x, int y) {
        if (isHovering) {
            // Trigger suara SFX Bertumpuk (Polyphony)
            std::lock_guard<std::mutex> lock(m_sfxMutex);
            m_sfxPlayheads.push_back(0); // Spawn new voice
        }
        if (m_renderer) {
            m_renderer->handleMouseClick(x, y);
        }
    });

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
