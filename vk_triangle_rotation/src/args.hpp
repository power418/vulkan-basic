#pragma once
#include <cstddef>
#include <string>

struct Args {
    bool help       = false;
    bool validation = false;
    int  width      = 800;
    int  height     = 600;
    float rotationSpeed = 0.9f;
    std::string vertShader  = "shaders_spv/triangle.vert.spv";
    std::string fragShader  = "shaders_spv/triangle.frag.spv";
    std::string mode        = "win-aero-enable";   // win-aero-enable | win-aero-disable
    std::string gpu         = "";                  // e.g. "rtx", "nvidia", "intel"
    size_t cuman_angka;
};

Args parseArgs(int argc, char* argv[]);
void printHelp(const char* programName);
