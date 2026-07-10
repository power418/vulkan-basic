#pragma once

#include <cmath>
#include <algorithm>

namespace platform {
namespace dpi {

    // Konstanta DPI standar OS (Windows 100% scaling = 96 DPI)
    constexpr float BASE_DPI = 96.0f;

    // Struktur untuk menyimpan skala DPI (X dan Y)
    struct DPIScale {
        float x = 1.0f;
        float y = 1.0f;

        // Mendapatkan skala rata-rata atau seragam
        inline float uniform() const { return (x + y) * 0.5f; }
    };

    // =========================================================================
    // Core Scaling Math
    // =========================================================================

    // Scale dengan presisi (pembulatan ke nilai terdekat, bukan sekedar dipotong/truncate)
    // Sangat penting untuk mencegah glitch 1-pixel pada UI
    template <typename T>
    inline int scale(T value, float dpiScale) {
        return static_cast<int>(std::round(static_cast<float>(value) * dpiScale));
    }

    template <typename T>
    inline int unscale(T value, float dpiScale) {
        return static_cast<int>(std::round(static_cast<float>(value) / dpiScale));
    }

    // Helper konversi DPI raw ke skala float (misal 144 DPI -> 1.5f)
    inline float dpiToScale(uint32_t rawDpi) {
        return static_cast<float>(rawDpi) / BASE_DPI;
    }

    // =========================================================================
    // Geometry Primitives for DPI Scaling
    // =========================================================================

    struct Point {
        int x = 0;
        int y = 0;

        inline Point scale(const DPIScale& dpi) const {
            return { dpi::scale(x, dpi.x), dpi::scale(y, dpi.y) };
        }
        inline Point unscale(const DPIScale& dpi) const {
            return { dpi::unscale(x, dpi.x), dpi::unscale(y, dpi.y) };
        }
    };

    struct Size {
        int width = 0;
        int height = 0;

        inline Size scale(const DPIScale& dpi) const {
            return { dpi::scale(width, dpi.x), dpi::scale(height, dpi.y) };
        }
        inline Size unscale(const DPIScale& dpi) const {
            return { dpi::unscale(width, dpi.x), dpi::unscale(height, dpi.y) };
        }
    };

    struct Rect {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;

        inline int width() const { return right - left; }
        inline int height() const { return bottom - top; }

        inline Rect scale(const DPIScale& dpi) const {
            return {
                dpi::scale(left, dpi.x),
                dpi::scale(top, dpi.y),
                dpi::scale(right, dpi.x),
                dpi::scale(bottom, dpi.y)
            };
        }
        inline Rect unscale(const DPIScale& dpi) const {
            return {
                dpi::unscale(left, dpi.x),
                dpi::unscale(top, dpi.y),
                dpi::unscale(right, dpi.x),
                dpi::unscale(bottom, dpi.y)
            };
        }
    };

    // =========================================================================
    // DPI Awareness Interface
    // =========================================================================
    
    // Interface murni yang bisa di-inherit oleh Window atau Renderer
    // agar bisa merespon secara otomatis ketika OS mengirimkan event perubahan DPI
    // (Misalnya ketika aplikasi digeser dari monitor 1080p ke monitor 4K).
    class IDPIAware {
    public:
        virtual ~IDPIAware() = default;
        
        // Akan dipanggil oleh platform backend (Win32/WinRT) saat DPI berubah
        virtual void onDpiChanged(const DPIScale& newDpi, const Rect& suggestedNewRect) = 0;
    };

} // namespace dpi
} // namespace platform
