#include "platform.hpp"
#include "win32/window_win32.hpp"
#include "winrt/window_winrt.hpp"
#include <VersionHelpers.h>
#include <cstdio>

// ---------------------------------------------------------------------------
// Runtime Windows version check + create the right window implementation.
//
//   Windows 10  (build 10240+)  →  WindowWinRT   (C++/WinRT + comctl32)
//   Windows 8.1 / 7 / older    →  WindowWin32    (classic Win32)
// ---------------------------------------------------------------------------
std::unique_ptr<Window> createPlatformWindow() {
    if (IsWindows10OrGreater()) {
        std::printf("Platform: Windows 10+  →  WinRT window\n");
        return std::make_unique<WindowWinRT>();
    }

    std::printf("Platform: < Windows 10  →  Win32 window\n");
    return std::make_unique<WindowWin32>();
}
