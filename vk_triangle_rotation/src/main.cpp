#include "args.hpp"
#include "app.hpp"
#include "logger.hpp"

#include <cstdio>

#if defined(_WIN32)
// Request high-performance dedicated GPU on hybrid graphics laptops (NVIDIA Optimus / AMD PowerXpress)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char* argv[]) 
{
    Logger::getInstance().init("output.txt");
    LOG_INFO("==================================================");
    LOG_INFO("Vulkan Triangle Rotation - Debug Session Started");
    LOG_INFO("Debug logger active. Writing to console & output.txt");
    LOG_INFO("==================================================");

#if defined(_WIN32)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        const char* acStatus = (sps.ACLineStatus == 1) ? "AC Adapter Connected (Charging/Online)" :
                               ((sps.ACLineStatus == 0) ? "Running on BATTERY" : "Unknown Power State");
        LOG_INFO("[HARDWARE-POWER] Laptop Power Source: %s | Battery: %d%%",
                 acStatus, (int)sps.BatteryLifePercent);
        if (sps.ACLineStatus == 0) {
            LOG_WARN("[HARDWARE-WARNING] Laptop berjalan menggunakan BATERAI! GPU NVIDIA RTX otomatis menurunkan clock & PCIe link di mode baterai, yang sering memicu macet grafis dan glitch audio 'ttrrrrttt'. Sambungkan charger untuk performa maksimal!");
        }
    }
#endif

    Args args = parseArgs(argc, argv);

    if (args.help) {
        printHelp(argv[0]);
        Logger::getInstance().shutdown();
        return 0;
    }

    LOG_INFO("Application arguments: mode=%s, width=%d, height=%d, speed=%.2f, gpu=%s",
             args.mode.c_str(), args.width, args.height, args.rotationSpeed,
             args.gpu.empty() ? "(auto-prefer RTX)" : args.gpu.c_str());

    App app(args);
    if (!app.init()) {
        LOG_ERROR("FATAL: App init failed");
        Logger::getInstance().shutdown();
        return 1;
    }

    LOG_INFO("Entering main render loop...");
    app.run();

    LOG_INFO("Application exited main loop successfully");
    Logger::getInstance().shutdown();
    return 0;
}
