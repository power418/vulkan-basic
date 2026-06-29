#include "shader_compiler.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

static std::string exeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    auto pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
#else
    return ".";
#endif
}

std::vector<uint32_t> loadSPIRV(const std::string& path) {
    // Try the given path first
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            size_t size = static_cast<size_t>(file.tellg());
            file.seekg(0);
            std::vector<uint32_t> code(size / sizeof(uint32_t));
            file.read(reinterpret_cast<char*>(code.data()), size);
            return code;
        }
    }

    // Fallback: resolve relative to executable directory
    std::string exeRel = exeDir() + "/" + path;
    std::ifstream file(exeRel, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::printf("ERROR: Cannot open SPIR-V file: %s\n"
                     "       (also tried: %s)\n"
                     "HINT:  Run from build dir, or use --vert / --frag\n",
                     path.c_str(), exeRel.c_str());
        return {};
    }

    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);
    return code;
}
