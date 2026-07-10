#include "menubar.hpp"
#include <cstdio>

bool Win32MenuBar::create(HWND hwnd) {
    if (m_hwnd) destroy();
    m_hwnd = hwnd;

    m_hMenu = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, ID_FILE_EXIT, L"E&xit");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, ID_HELP_ABOUT, L"&About");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    SetMenu(m_hwnd, m_hMenu);
    std::printf("Win32MenuBar: created\n");
    return true;
}

void Win32MenuBar::destroy() {
    if (m_hMenu) {
        DestroyMenu(m_hMenu);
        m_hMenu = nullptr;
    }
    m_hwnd = nullptr;
}
