#include "platform.hpp"
#include "win32/window_win32.hpp"
#include "winrt/window_winrt.hpp"
#include <cstdio>

// ---------------------------------------------------------------------------
// Mode-based window creation.
//
//   win-aero-enable   -> WindowWin32  (pure Win32 + SetMenu menubar)
//   win-aero-disable  -> WindowWinRT  (C++/WinRT + client-area menubar)
// ---------------------------------------------------------------------------
std::unique_ptr<Window> createPlatformWindow(const std::string& mode) {
    if (mode == "win-aero-disable") {
        std::printf("Platform: mode=win-aero-disable -> WinRT window\n");
        return std::make_unique<WindowWinRT>();
    }

    std::printf("Platform: mode=win-aero-enable -> Win32 window\n");
    return std::make_unique<WindowWin32>();
}
