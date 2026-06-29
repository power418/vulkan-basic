#include <iostream>
#include <Windows.h>

#include <vulkan/vulkan.h>
#include "native_window_win32.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow)
{
    // Konfigurasi aplikasi
    Window::AppState appState;
    appState.title = L"Vulkan Window Test";
    appState.width = 800;
    appState.height = 600;

    // Buat window langsung tanpa class tambahan
    Window window;
    if (!window.create(appState))
    {
        std::cerr << "Failed to create window!" << std::endl;
        return -1;
    }

    // Render loop (Vulkan butuh loop ini untuk me-render frame tiap detiknya)
    // poll_events di sini tidak melakukan 'blocking', sehingga kita bisa menaruh 
    // fungsi render Vulkan di dalam loop ini nanti.
    while (!window.poll_events())
    {
        // ... Kode rendering Vulkan ...
    }

    // Hancurkan window
    window.destroy();
    return 0;
}
