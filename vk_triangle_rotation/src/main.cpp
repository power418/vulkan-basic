#include "args.hpp"
#include "app.hpp"

#include <cstdio>

int main(int argc, char* argv[]) 
{
    Args args = parseArgs(argc, argv);

    if (args.help) {
        printHelp(argv[0]);
        return 0;
    }

    App app(args);
    if (!app.init()) {
        std::printf("FATAL: App init failed\n");
        return 1;
    }

    app.run();
    return 0;
}
