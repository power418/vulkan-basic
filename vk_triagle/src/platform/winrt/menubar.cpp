#include "menubar.hpp"
#include <windowsx.h>
#include <dwmapi.h>
#include <cstdio>

#pragma comment(lib, "dwmapi.lib")

bool WinRTMenuBar::create(HWND hwnd) {
    if (m_hwnd) destroy();
    m_hwnd = hwnd;

    m_hMenuFile = CreatePopupMenu();
    AppendMenuW(m_hMenuFile, MF_STRING, ID_FILE_EXIT, L"E&xit");
    MENUINFO mi{};
    mi.cbSize  = sizeof(mi);
    mi.fMask   = MIM_STYLE;
    mi.dwStyle = MNS_AUTODISMISS;
    SetMenuInfo(m_hMenuFile, &mi);

    m_hMenuHelp = CreatePopupMenu();
    AppendMenuW(m_hMenuHelp, MF_STRING, ID_HELP_ABOUT, L"&About");
    SetMenuInfo(m_hMenuHelp, &mi);

    std::printf("WinRTMenuBar: created\n");
    return true;
}

void WinRTMenuBar::destroy() {
    if (m_hMenuFile) { DestroyMenu(m_hMenuFile); m_hMenuFile = nullptr; }
    if (m_hMenuHelp) { DestroyMenu(m_hMenuHelp); m_hMenuHelp = nullptr; }
    m_hwnd = nullptr;
}

// ===========================================================================
// WM_NCCALCSIZE — shrink client area by menu bar height
// ===========================================================================
bool WinRTMenuBar::onNcCalcSize(WPARAM wParam, LPARAM lParam) {
    if (wParam != TRUE) return false;
    auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
    DefWindowProc(m_hwnd, WM_NCCALCSIZE, TRUE, lParam);
    p->rgrc[0].top += MENU_BAR_HEIGHT;
    return true;
}

// ===========================================================================
// WM_NCPAINT — draw Ableton-inspired menu bar in NC area
// ===========================================================================
bool WinRTMenuBar::onNcPaint(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    LRESULT dwmRet = 0;
    DwmDefWindowProc(hwnd, WM_NCPAINT, wParam, lParam, &dwmRet);

    HDC hdc = GetWindowDC(hwnd);
    if (!hdc) return true;

    RECT wndRect;
    GetWindowRect(hwnd, &wndRect);
    OffsetRect(&wndRect, -wndRect.left, -wndRect.top);

    int titleBot = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);
    RECT rcMenu{wndRect.left, titleBot, wndRect.right, titleBot + MENU_BAR_HEIGHT};

    // Background — premium flat
    COLORREF bg  = m_isDarkMode ? RGB(0x1E,0x1E,0x1E) : RGB(0xF5,0xF5,0xF5);
    HBRUSH   br  = CreateSolidBrush(bg);
    FillRect(hdc, &rcMenu, br);
    DeleteObject(br);

    // Subtle 1px separator at bottom
    COLORREF sep = m_isDarkMode ? RGB(0x38,0x38,0x38) : RGB(0xD0,0xD0,0xD0);
    HPEN pen = CreatePen(PS_SOLID, 1, sep);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, rcMenu.left, rcMenu.bottom - 1, nullptr);
    LineTo(hdc, rcMenu.right, rcMenu.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Segoe UI 11pt
    SetBkMode(hdc, TRANSPARENT);
    HFONT hMenuFont = CreateFontW(
        -MulDiv(11, GetDeviceCaps(hdc, LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hMenuFont);

    SetTextColor(hdc, m_isDarkMode ? RGB(0xE8,0xE8,0xE8) : RGB(0x28,0x28,0x28));

    RECT rcItem = rcMenu;
    rcItem.left += MENU_PAD_LEFT;
    rcItem.right = rcItem.left + MENU_ITEM_W;
    DrawTextW(hdc, L"File", -1, &rcItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    rcItem.left += MENU_ITEM_W + MENU_GAP;
    rcItem.right = rcItem.left + MENU_ITEM_W;
    DrawTextW(hdc, L"Help", -1, &rcItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(hMenuFont);
    ReleaseDC(hwnd, hdc);
    return true;
}

// ===========================================================================
// WM_NCHITTEST — return HTMENU for menu bar area
// ===========================================================================
bool WinRTMenuBar::onNcHitTest(LPARAM lParam, LRESULT& out) {
    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    RECT wr;
    GetWindowRect(m_hwnd, &wr);
    int y  = pt.y - wr.top;
    int tb = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);
    if (y >= tb && y < tb + MENU_BAR_HEIGHT) {
        out = HTMENU;
        return true;
    }
    return false;
}

// ===========================================================================
// WM_NCLBUTTONDOWN — show popup via TrackPopupMenu
// ===========================================================================
bool WinRTMenuBar::onNcLButtonDown(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    if (wParam != HTMENU || !m_hMenuFile || !m_hMenuHelp)
        return false;

    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    RECT wr;
    GetWindowRect(hwnd, &wr);
    int x = pt.x - wr.left;
    int tb = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION);

    int fileL = MENU_PAD_LEFT;
    int fileR = fileL + MENU_ITEM_W;
    int helpL = fileL + MENU_ITEM_W + MENU_GAP;
    int helpR = helpL + MENU_ITEM_W;

    if (x >= fileL && x < fileR) {
        TrackPopupMenu(m_hMenuFile, TPM_LEFTALIGN | TPM_TOPALIGN,
                       wr.left + fileL, wr.top + tb + MENU_BAR_HEIGHT,
                       0, hwnd, nullptr);
        return true;
    }
    if (x >= helpL && x < helpR) {
        TrackPopupMenu(m_hMenuHelp, TPM_LEFTALIGN | TPM_TOPALIGN,
                       wr.left + helpL, wr.top + tb + MENU_BAR_HEIGHT,
                       0, hwnd, nullptr);
        return true;
    }
    return false;
}
