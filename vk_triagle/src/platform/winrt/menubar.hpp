#pragma once
#include <windows.h>

// Menu command IDs (shared with Win32 menu bar)
#define ID_FILE_EXIT    2001
#define ID_HELP_ABOUT   2002

// ---------------------------------------------------------------------------
// Client-area based menu bar — rendered as a child HWND inside the parent
// window's client area.  This avoids NC-area painting which conflicts with
// DWM composition and causes the window frame to fall back to classic/Win7.
// ---------------------------------------------------------------------------
class WinRTMenuBar {
public:
    WinRTMenuBar() = default;
    ~WinRTMenuBar() { destroy(); }

    bool create(HWND parentHwnd);
    void destroy();
    void resize(int parentWidth);

    void setDarkMode(bool dark);
    bool isActive() const { return m_hwnd != nullptr; }
    int  getHeight() const { return MENU_BAR_HEIGHT; }

private:
    HWND  m_hwnd       = nullptr;
    HWND  m_parentHwnd = nullptr;
    HMENU m_hMenuFile  = nullptr;
    HMENU m_hMenuHelp  = nullptr;
    bool  m_isDarkMode = false;

    static constexpr int MENU_BAR_HEIGHT = 28;
    static constexpr int MENU_PAD_LEFT   = 8;
    static constexpr int MENU_ITEM_W     = 40;
    static constexpr int MENU_GAP        = 8;

    static LRESULT CALLBACK menuWndProc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void onClick(int x);
};
