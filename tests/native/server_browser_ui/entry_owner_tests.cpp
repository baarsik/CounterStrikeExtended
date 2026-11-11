#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <cstdint>
using CreateInterfaceFn = void *(*)(const char *, int *);
std::string testCase;
int pendingTurns = 0;
int runDepth = 0;
namespace cse {
struct AudioModuleShutdown {
    AudioModuleShutdown() {
        std::puts("audio-guard-created");
    }
    ~AudioModuleShutdown() {
        std::puts("audio-guard-destroyed");
    }
};
bool InitializeNativeGameUIHost() {
    return testCase != "invalid-entry";
}
bool FinishNativeListenShutdown() {
    if (pendingTurns) {
        --pendingTurns;
        return false;
    }
    return true;
}
}
void SDL_Delay(unsigned int) {
    std::puts("retirement-yield");
}
char *va(const char *format, int) {
    return const_cast<char *>(format);
}
int build_number() {
    return 1;
}
void SteamAPI_UseBreakpadCrashHandler(char *, const char *, const char *, int, void *, void *) {}
#if defined(_WIN32)
void ExceptionHandler() {}
void _set_se_translator(void (*)()) {}
#endif
class CEngineAPI {
public:
    int Run(void *, char *, char *, char *, CreateInterfaceFn, CreateInterfaceFn);
};
int RunListenServer(void *, char *base, char *command, char *restart, CreateInterfaceFn launcher,
                    CreateInterfaceFn filesystem) {
    if (testCase == "exception") {
        throw std::runtime_error("controlled unknown partial initialization exception");
    }
    if (testCase == "reentry" && runDepth++ == 0) {
        CEngineAPI nested;
        nested.Run(nullptr, base, command, restart, launcher, filesystem);
    }
    std::printf("listen-entered-pending-%d\n", pendingTurns);
    return 0;
}
#if CSE_TEST_HARDWARE
#include "entry_hw.inc"
#else
#include "entry_sw.inc"
#endif
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    testCase = argv[1];
    std::set_terminate([] {
        std::puts("fatal-invariant-observed");
        std::fflush(stdout);
        // Fixture-only death hook prevents a deliberate OS crash report. Production uses std::terminate.
        std::_Exit(86);
    });
    pendingTurns = testCase == "pending" ? 2 : 0;
    CEngineAPI owner;
    char command[] = "-nobreakpad";
    const int result = owner.Run(nullptr, nullptr, command, nullptr, nullptr, nullptr);
    // The real launcher's post-Run route closes/unloads the engine image on every ordinary return.
    std::puts("caller-image-unload-authorized");
    return result;
}
