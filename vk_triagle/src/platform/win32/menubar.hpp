#pragma once
#include <windows.h>

// Menu command IDs (shared with WinRT menu bar)
#define ID_FILE_EXIT    2001
#define ID_HELP_ABOUT   2002

class Win32MenuBar {
public:
    Win32MenuBar() = default;
    ~Win32MenuBar() { destroy(); }

    bool create(HWND hwnd);
    void destroy();

    bool isActive() const { return m_hwnd != nullptr; }

private:
    HWND  m_hwnd  = nullptr;
    HMENU m_hMenu = nullptr;
};
