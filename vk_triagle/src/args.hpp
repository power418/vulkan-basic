#pragma once
#include <string>

struct Args {
    bool help       = false;
    bool validation = false;
    int  width      = 800;
    int  height     = 600;
    std::string vertShader  = "shaders_spv/triangle.vert.spv";
    std::string fragShader  = "shaders_spv/triangle.frag.spv";
    std::string mode        = "win-aero-enable";   // win-aero-enable | win-aero-disable
};

Args parseArgs(int argc, char* argv[]);
void printHelp(const char* programName);
