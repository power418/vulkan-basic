#include "args.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            a.help = true;
        } else if (strcmp(argv[i], "--validation") == 0 || strcmp(argv[i], "-v") == 0) {
            a.validation = true;
        } else if (strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-w") == 0) {
            if (i + 1 < argc) a.width = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 || strcmp(argv[i], "-H") == 0) {
            if (i + 1 < argc) a.height = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--speed") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) a.rotationSpeed = static_cast<float>(std::atof(argv[++i]));
        } else if (strcmp(argv[i], "--vert") == 0) {
            if (i + 1 < argc) a.vertShader = argv[++i];
        } else if (strcmp(argv[i], "--frag") == 0) {
            if (i + 1 < argc) a.fragShader = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0 || strcmp(argv[i], "-m") == 0) {
            if (i + 1 < argc) {
                a.mode = argv[++i];
            }
        } else if (strncmp(argv[i], "--mode=", 7) == 0) {
            a.mode = argv[i] + 7;
        } else if (strncmp(argv[i], "-m", 3) == 0) {
            a.mode = argv[i] + 3;
        } else if (strncmp(argv[i], "--speed=", 8) == 0) {
            a.rotationSpeed = static_cast<float>(std::atof(argv[i] + 8));
        } else if (strncmp(argv[i], "-s", 3) == 0) {
            a.rotationSpeed = static_cast<float>(std::atof(argv[i] + 3));
        } else {
            std::printf("Unknown option: %s\n", argv[i]);
            a.help = true;
        }
    }
    if (a.mode != "win-aero-enable" && a.mode != "win-aero-disable") {
        std::printf("Invalid mode: %s\n", a.mode.c_str());
        std::printf("  win-aero-enable   Full Win32 window + Win32 menubar (DWM NC enabled)\n");
        std::printf("  win-aero-disable  Full WinRT window + custom NC menubar (DWM NC disabled)\n");
        a.help = true;
    }
    return a;
}

void printHelp(const char* name) {
    std::printf("Usage: %s [options]\n\n", name);
    std::printf("Options:\n");
    std::printf("  -h, --help              Show this help message\n");
    std::printf("  -v, --validation        Enable Vulkan validation layers\n");
    std::printf("  -w, --width <px>        Window width  (default: 800)\n");
    std::printf("  -H, --height <px>       Window height (default: 600)\n");
    std::printf("  -s, --speed  <float>    Rotation speed multiplier (default: 0.9)\n");
    std::printf("  --vert     <path>       Vertex shader SPIR-V path\n");
    std::printf("  --frag     <path>       Fragment shader SPIR-V path\n");
    std::printf("  -m, --mode <mode>       Window mode:\n");
    std::printf("                            win-aero-enable   Full Win32 + Win32 menubar (DWM NC)\n");
    std::printf("                            win-aero-disable  Full WinRT + custom NC menubar\n");
    std::printf("                          (default: win-aero-enable)\n");
}
