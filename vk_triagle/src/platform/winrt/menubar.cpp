#include "menubar.hpp"
#include <windowsx.h>
#include <cstdio>

static const char* MENUBAR_CLASS = "VkWinRTMenuBarClass";

// ===========================================================================
// Child HWND WndProc — handles painting and click events for the menubar
// ===========================================================================
LRESULT CALLBACK WinRTMenuBar::menuWndProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    auto* bar = reinterpret_cast<WinRTMenuBar*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!bar) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        bar->paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        bar->onClick(GET_X_LPARAM(lParam));
        return 0;
    case WM_ERASEBKGND:
        return 1;   // we handle painting fully
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ===========================================================================
// create — register class + create child HWND at top of parent client area
// ===========================================================================
bool WinRTMenuBar::create(HWND parentHwnd) {
    if (m_hwnd) destroy();
    m_parentHwnd = parentHwnd;

    HINSTANCE hinst = GetModuleHandle(nullptr);

    WNDCLASS wc{};
    wc.lpfnWndProc   = menuWndProc;
    wc.hInstance      = hinst;
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName  = MENUBAR_CLASS;
    if (!RegisterClass(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    RECT rc;
    GetClientRect(parentHwnd, &rc);

    m_hwnd = CreateWindowEx(
        0, MENUBAR_CLASS, "",
        WS_CHILD | WS_VISIBLE,
        0, 0, rc.right - rc.left, MENU_BAR_HEIGHT,
        parentHwnd, nullptr, hinst, this);

    if (!m_hwnd) {
        std::printf("ERROR: menubar child HWND creation failed\n");
        return false;
    }

    // Popup menus (shown on click)
    m_hMenuFile = CreatePopupMenu();
    AppendMenuW(m_hMenuFile, MF_STRING, ID_FILE_EXIT, L"E&xit");

    m_hMenuHelp = CreatePopupMenu();
    AppendMenuW(m_hMenuHelp, MF_STRING, ID_HELP_ABOUT, L"&About");

    std::printf("WinRTMenuBar: created (client-area child HWND)\n");
    return true;
}

// ===========================================================================
// destroy
// ===========================================================================
void WinRTMenuBar::destroy() {
    if (m_hMenuFile) { DestroyMenu(m_hMenuFile); m_hMenuFile = nullptr; }
    if (m_hMenuHelp) { DestroyMenu(m_hMenuHelp); m_hMenuHelp = nullptr; }
    if (m_hwnd)      { DestroyWindow(m_hwnd);    m_hwnd = nullptr;      }
    m_parentHwnd = nullptr;
}

// ===========================================================================
// resize — called by parent WM_SIZE
// ===========================================================================
void WinRTMenuBar::resize(int parentWidth) {
    if (m_hwnd)
        SetWindowPos(m_hwnd, nullptr, 0, 0, parentWidth, MENU_BAR_HEIGHT,
                     SWP_NOZORDER | SWP_NOMOVE);
}

// ===========================================================================
// setDarkMode — trigger repaint
// ===========================================================================
void WinRTMenuBar::setDarkMode(bool dark) {
    m_isDarkMode = dark;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ===========================================================================
// paint — Ableton-inspired flat menu bar (in client area)
// ===========================================================================
void WinRTMenuBar::paint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    // Background — premium flat
    COLORREF bg = m_isDarkMode ? RGB(0x33,0x33,0x33) : RGB(0xF5,0xF5,0xF5);
    HBRUSH   br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    // Subtle 1px separator at bottom
    COLORREF sep = m_isDarkMode ? RGB(0x22,0x22,0x22) : RGB(0xD0,0xD0,0xD0);
    HPEN pen = CreatePen(PS_SOLID, 1, sep);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
    LineTo(hdc, rc.right, rc.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Segoe UI 9pt
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = CreateFontW(
        -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    SetTextColor(hdc, m_isDarkMode ? RGB(0xCC,0xCC,0xCC) : RGB(0x40,0x40,0x40));

    RECT rcItem = rc;
    rcItem.left  = MENU_PAD_LEFT;
    rcItem.right = rcItem.left + MENU_ITEM_W;
    DrawTextW(hdc, L"File", -1, &rcItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    rcItem.left += MENU_ITEM_W + MENU_GAP;
    rcItem.right = rcItem.left + MENU_ITEM_W;
    DrawTextW(hdc, L"Help", -1, &rcItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

// ===========================================================================
// onClick — show popup menu at the correct screen position
// ===========================================================================
void WinRTMenuBar::onClick(int x) {
    RECT wr;
    GetWindowRect(m_hwnd, &wr);

    int fileL = MENU_PAD_LEFT;
    int fileR = fileL + MENU_ITEM_W;
    int helpL = fileL + MENU_ITEM_W + MENU_GAP;
    int helpR = helpL + MENU_ITEM_W;

    if (x >= fileL && x < fileR) {
        TrackPopupMenu(m_hMenuFile, TPM_LEFTALIGN | TPM_TOPALIGN,
                       wr.left + fileL, wr.bottom,
                       0, m_parentHwnd, nullptr);
    } else if (x >= helpL && x < helpR) {
        TrackPopupMenu(m_hMenuHelp, TPM_LEFTALIGN | TPM_TOPALIGN,
                       wr.left + helpL, wr.bottom,
                       0, m_parentHwnd, nullptr);
    }
}
