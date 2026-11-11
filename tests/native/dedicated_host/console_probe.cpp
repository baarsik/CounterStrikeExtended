#include "dedicated/native/TextConsolePosix.h"
#include "dedicated/native/Signals.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>

void Sys_Printf(char*, ...) {}

int main(int argc, char** argv) {
    cse::dedicated::SignalScope signals;
    if (!signals.Install()) {
        return 2;
    }
    CTextConsolePosix console;
    if (!console.Init(nullptr)) {
        return 3;
    }
    std::fprintf(stderr, "READY\n");
    const bool repeat = argc > 1 && std::strcmp(argv[1], "repeat") == 0;
    int eofFrames = 0;
    while (!cse::dedicated::ShutdownRequested) {
        if (char* line = console.GetLine()) {
            std::fprintf(stderr, "LINE:%s\n", line);
        }
        if (console.InputEnded() && ++eofFrames == 5) {
            std::fprintf(stderr, "EOF_FRAMES:5\n");
            if (repeat) {
                console.ShutDown();
                if (!console.Init(nullptr)) {
                    return 4;
                }
            }
            break;
        }
        usleep(10000);
    }
    console.PrintRaw((char*)"SHUTDOWN_OUTPUT\n");
    console.ShutDown();
    signals.Restore();
    std::fprintf(stderr, "CLOSED\n");
    return 0;
}
