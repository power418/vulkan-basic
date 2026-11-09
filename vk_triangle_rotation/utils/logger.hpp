#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERR,
    RENDER,
    GPU_STALL
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    bool init(const std::string& filepath = "output.txt", bool append = false) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string resolvedPath = filepath;
#if defined(_WIN32)
        if (filepath == "output.txt" || filepath.find('\\') == std::string::npos) {
            char buf[MAX_PATH];
            DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
            if (len > 0 && len < MAX_PATH) {
                char* sep = strrchr(buf, '\\');
                if (sep) {
                    *(sep + 1) = '\0';
                    resolvedPath = std::string(buf) + filepath;
                }
            }
        }
#endif
        m_filepath = resolvedPath;
        if (m_file.is_open()) {
            m_file.close();
        }

        auto mode = std::ios::out;
        if (append) {
            mode |= std::ios::app;
        } else {
            mode |= std::ios::trunc;
        }

        m_file.open(m_filepath, mode);
        if (!m_file.is_open()) {
            std::fprintf(stderr, "[Logger] Failed to open log file: %s\n", m_filepath.c_str());
            return false;
        }

        std::printf("[Logger] Debug log initialized. Output file: %s\n", m_filepath.c_str());
        m_initialized = true;
        return true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.flush();
            m_file.close();
        }
        m_initialized = false;
    }

    void log(LogLevel level, const char* format, ...) {
        char buffer[2048];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        logMessage(level, buffer);
    }

    void logMessage(LogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Auto-open file if not yet opened
        if (!m_file.is_open()) {
            m_file.open(m_filepath, std::ios::out | std::ios::trunc);
            m_initialized = m_file.is_open();
        }

        // Get local timestamp with milliseconds
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow{};
#if defined(_WIN32)
        localtime_s(&tmNow, &t);
#else
        localtime_r(&t, &tmNow);
#endif

        char timeBuf[64];
        std::snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d.%03lld",
                      tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                      tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec,
                      static_cast<long long>(nowMs.count()));

        const char* levelStr = "INFO     ";
        switch (level) {
            case LogLevel::DEBUG:     levelStr = "DEBUG    "; break;
            case LogLevel::INFO:      levelStr = "INFO     "; break;
            case LogLevel::WARN:      levelStr = "WARN     "; break;
            case LogLevel::ERR:       levelStr = "ERROR    "; break;
            case LogLevel::RENDER:    levelStr = "RENDER   "; break;
            case LogLevel::GPU_STALL: levelStr = "GPU-STALL"; break;
        }

        // Output to stdout / console
        std::printf("[%s] [%s] %s\n", timeBuf, levelStr, msg.c_str());

        // Output to file and immediately flush
        if (m_file.is_open()) {
            m_file << "[" << timeBuf << "] [" << levelStr << "] " << msg << "\n";
            m_file.flush();
        }
    }

    const std::string& getFilePath() const { return m_filepath; }
    bool isInitialized() const { return m_initialized; }

private:
    Logger() = default;
    ~Logger() {
        shutdown();
    }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex m_mutex;
    std::ofstream m_file;
    std::string m_filepath = "output.txt";
    bool m_initialized = false;
};

// Convenience helper macros
#define LOG_DEBUG(fmt, ...)     ::Logger::getInstance().log(LogLevel::DEBUG,     fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)      ::Logger::getInstance().log(LogLevel::INFO,      fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)      ::Logger::getInstance().log(LogLevel::WARN,      fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)     ::Logger::getInstance().log(LogLevel::ERR,       fmt, ##__VA_ARGS__)
#define LOG_RENDER(fmt, ...)    ::Logger::getInstance().log(LogLevel::RENDER,    fmt, ##__VA_ARGS__)
#define LOG_GPU_STALL(fmt, ...) ::Logger::getInstance().log(LogLevel::GPU_STALL, fmt, ##__VA_ARGS__)

// ===========================================================================
// RTX GPU Performance & Stall Diagnostic Monitor
// ===========================================================================
struct FrameStageTimes {
    float fenceWaitMs  = 0.0f; // vkWaitForFences (waiting for GPU to complete previous frame)
    float acquireMs    = 0.0f; // vkAcquireNextImageKHR (waiting for swapchain image)
    float uboUpdateMs  = 0.0f; // vkMapMemory + memcpy + vkUnmapMemory
    float recordCmdMs  = 0.0f; // Command buffer recording & ImGui render
    float submitMs     = 0.0f; // vkQueueSubmit (driver submission latency)
    float presentMs    = 0.0f; // vkQueuePresentKHR (presentation latency & VSync wait)
    float totalFrameMs = 0.0f; // Total frame duration
};

class GpuStallMonitor {
public:
    static GpuStallMonitor& getInstance() {
        static GpuStallMonitor instance;
        return instance;
    }

    // Thresholds (ms)
    float warningThresholdMs  = 33.3f; // Drop below 30 FPS (> 2 frame times at 60Hz)
    float severeThresholdMs   = 66.6f; // Severe hitch (> 4 frame times)
    float criticalThresholdMs = 150.0f; // Noticeable graphic hang/freeze

    // Running statistics
    uint64_t totalFrames = 0;
    uint64_t stallCount = 0;
    uint64_t severeStallCount = 0;
    uint64_t criticalStallCount = 0;
    float worstFrameMs = 0.0f;
    uint64_t worstFrameIndex = 0;
    std::string worstFrameCause;

    // Periodic heartbeat timer
    std::chrono::high_resolution_clock::time_point lastHeartbeat = std::chrono::high_resolution_clock::now();
    uint32_t framesSinceHeartbeat = 0;

    void recordFrame(uint64_t frameIdx, const std::string& gpuName, const FrameStageTimes& t, uint32_t width, uint32_t height) {
        totalFrames++;
        framesSinceHeartbeat++;

        if (t.totalFrameMs > worstFrameMs) {
            worstFrameMs = t.totalFrameMs;
            worstFrameIndex = frameIdx;
        }

        // Detect graphic stall / hitch
        if (t.totalFrameMs >= warningThresholdMs) {
            stallCount++;
            if (t.totalFrameMs >= severeThresholdMs) severeStallCount++;
            if (t.totalFrameMs >= criticalThresholdMs) criticalStallCount++;

            // Analyze stage bottleneck
            std::string cause;
            if (t.fenceWaitMs > t.acquireMs && t.fenceWaitMs > t.presentMs && t.fenceWaitMs > t.recordCmdMs) {
                cause = "GPU Pipeline Backlog (vkWaitForFences) - GPU RTX membutuhkan waktu terlalu lama menyelesaikan frame sebelumnya (kemungkinan GPU clock throttling / heavy draw / TDR).";
            } else if (t.acquireMs > t.fenceWaitMs && t.acquireMs > t.presentMs && t.acquireMs > 15.0f) {
                cause = "Swapchain Starvation (vkAcquireNextImageKHR) - DWM/Compositor laptop menahan buffer (cross-adapter delay pada laptop Optimus).";
            } else if (t.presentMs > t.fenceWaitMs && t.presentMs > t.acquireMs && t.presentMs > 15.0f) {
                cause = "Presentation Queue Blocked (vkQueuePresentKHR) - Tertahan di antrean VSync FIFO atau presentasi DWM.";
            } else if (t.recordCmdMs > t.fenceWaitMs && t.recordCmdMs > 15.0f) {
                cause = "CPU Command Recording Spike - Waktu generasi command buffer atau ImGui terlalu tinggi.";
            } else if (t.uboUpdateMs > 10.0f) {
                cause = "UBO Memory Stall - vkMapMemory/vkUnmapMemory tertahan karena GPU sedang membaca buffer yang sama.";
            } else {
                cause = "Multi-Stage Latency Spike.";
            }

            if (t.totalFrameMs >= worstFrameMs) {
                worstFrameCause = cause;
            }

            const char* tag = (t.totalFrameMs >= criticalThresholdMs) ? "CRITICAL GPU HANG DETECTED" :
                              ((t.totalFrameMs >= severeThresholdMs) ? "SEVERE GPU STALL DETECTED" : "GPU STALL DETECTED");

            LOG_GPU_STALL("======================================================================");
            LOG_GPU_STALL("[%s] Frame #%llu | Durasi: %.2f ms (Target: 16.67 ms, ~%.0f frame drop)",
                          tag, frameIdx, t.totalFrameMs, t.totalFrameMs / 16.67f);
            LOG_GPU_STALL("  Active GPU     : %s", gpuName.c_str());
            LOG_GPU_STALL("  Resolusi       : %ux%u", width, height);
            LOG_GPU_STALL("  Rincian Tahap  : FenceWait: %6.2f ms | Acquire: %6.2f ms | UBO: %6.2f ms",
                          t.fenceWaitMs, t.acquireMs, t.uboUpdateMs);
            LOG_GPU_STALL("                   Record   : %6.2f ms | Submit : %6.2f ms | Present: %6.2f ms",
                          t.recordCmdMs, t.submitMs, t.presentMs);
            LOG_GPU_STALL("  Diagnosa Utama : %s", cause.c_str());
            LOG_GPU_STALL("  Statistik Total: %llu stall terdeteksi dari %llu frame (Worst: %.2f ms pada Frame #%llu)",
                          stallCount, totalFrames, worstFrameMs, worstFrameIndex);
            LOG_GPU_STALL("======================================================================");
        }

        // Periodic Health Heartbeat every 5.0 seconds
        auto now = std::chrono::high_resolution_clock::now();
        float elapsedSec = std::chrono::duration<float>(now - lastHeartbeat).count();
        if (elapsedSec >= 5.0f) {
            float avgFps = static_cast<float>(framesSinceHeartbeat) / (elapsedSec > 0.0f ? elapsedSec : 1.0f);
            if (stallCount == 0) {
                LOG_INFO("[RTX-HEALTH] Status GPU Normal | FPS: %.1f | Total Frame: %llu | Tidak ada macet/stall grafis",
                         avgFps, totalFrames);
            } else {
                LOG_WARN("[RTX-HEALTH] Status GPU: Ada %llu kejadian macet (%llu severe, %llu critical) dari %llu frame | FPS: %.1f | Worst: %.2f ms (Frame #%llu)",
                         stallCount, severeStallCount, criticalStallCount, totalFrames, avgFps, worstFrameMs, worstFrameIndex);
            }
            lastHeartbeat = now;
            framesSinceHeartbeat = 0;
        }
    }
};
