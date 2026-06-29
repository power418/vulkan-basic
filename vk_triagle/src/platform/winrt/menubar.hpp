#pragma once
#include <windows.h>

// Menu command IDs (shared with Win32 menu bar)
#define ID_FILE_EXIT    2001
#define ID_HELP_ABOUT   2002

class WinRTMenuBar {
public:
    WinRTMenuBar() = default;
    ~WinRTMenuBar() { destroy(); }

    bool create(HWND hwnd);
    void destroy();

    void setDarkMode(bool dark) { m_isDarkMode = dark; }
    bool isActive() const { return m_hwnd != nullptr; }

    // NC-area message handlers — return true if handled
    bool onNcCalcSize(WPARAM wParam, LPARAM lParam);
    bool onNcPaint(HWND hwnd, WPARAM wParam, LPARAM lParam);
    bool onNcHitTest(LPARAM lParam, LRESULT& out);
    bool onNcLButtonDown(HWND hwnd, WPARAM wParam, LPARAM lParam);

private:
    HWND  m_hwnd       = nullptr;
    HMENU m_hMenuFile  = nullptr;
    HMENU m_hMenuHelp  = nullptr;
    bool  m_isDarkMode = false;

    static constexpr int MENU_BAR_HEIGHT = 32;
    static constexpr int MENU_PAD_LEFT   = 14;
    static constexpr int MENU_ITEM_W     = 64;
    static constexpr int MENU_GAP        = 20;
};
