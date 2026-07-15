#pragma once
#include <string>

// File ini digunakan untuk memberikan instruksi ke sistem operasi Windows
// dan driver kartu grafis agar memaksa aplikasi berjalan menggunakan GPU Diskret.
#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>

#include <GLFW/glfw3.h> 

extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// Namespace khusus untuk mengatur urusan Perangkat Keras / GPU
namespace DeviceManager {
    
    // Mendapatkan nama Hardware GPU yang sedang bekerja merender OpenGL saat ini.
    inline std::string GetActiveGPUName() {
        // Kita menggunakan fungsi bawaan GLFW untuk mengambil alamat fungsi glGetString
        // agar tidak perlu menarik library OpenGL raksasa ke dalam header ini.
        typedef const unsigned char* (*PFNGLGETSTRINGPROC)(unsigned int);
        PFNGLGETSTRINGPROC glGetStringProc = (PFNGLGETSTRINGPROC)glfwGetProcAddress("glGetString");
        
        if (glGetStringProc) {
            // 0x1F01 adalah kode konstan untuk GL_RENDERER
            const unsigned char* renderer = glGetStringProc(0x1F01);
            if (renderer) {
                return std::string(reinterpret_cast<const char*>(renderer));
            }
        }
        return "Unknown GPU";
    }

    // Fungsi untuk mensimulasikan perpindahan GPU dengan cara merestart aplikasi.
    // Di OpenGL (Windows), kita tidak bisa menukar GPU tanpa merestart program.
    inline void RestartApplicationAndSwitchGPU() {
#if defined(_WIN32)
        // Ambil path lengkap dari file .exe aplikasi ini
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);

        // Eksekusi/Jalankan ulang file .exe tersebut sebagai proses baru (Self-Restart)
        HINSTANCE result = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
        
        // Jika berhasil dijalankan (kode > 32), matikan proses aplikasi yang lama seketika.
        if ((intptr_t)result > 32) {
            ExitProcess(0);
        }
#endif
    }
}
