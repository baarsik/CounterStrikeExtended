#include <SDL3/SDL.h>
#include "../../../src/public/native_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    if (argc != 8 || !SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy") || SDL_GetVersion() != SDL_VERSION) {
        return 2;
    }
    const bool externalReference = std::strcmp(argv[7], "external") == 0;
    if (externalReference && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return 3;
    }
    void* module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
    if (!module) {
        return 4;
    }
    using Run = int (*)(const char*, const char*, const char*, int);
    using Shutdown = int (*)();
    using State = int (*)(int);
    const auto run = reinterpret_cast<Run>(cse_platform::GetSymbol(module, "CseAudioFixtureRun"));
    const auto shutdown = reinterpret_cast<Shutdown>(cse_platform::GetSymbol(module, "CseAudioFixtureShutdown"));
    const auto state = reinterpret_cast<State>(cse_platform::GetSymbol(module, "CseAudioFixtureState"));
    if (!run || !shutdown || !state) {
        return 5;
    }
    const bool explicitCleanup = std::strcmp(argv[5], "explicit") == 0;
    const int result = run(argv[2], argv[3], argv[4], explicitCleanup ? 1 : 0);
    std::printf("fixture_run_returned=%d\n", result);
    std::fflush(stdout);
    const int expected = std::strcmp(argv[3], "none") == 0 ? 1 : 0;
    if (result != expected) {
        return 6;
    }
    if (state(5) != 1 || state(6) != 1) {
        return 13;
    }
    if (explicitCleanup) {
        const int before = state(0);
        const bool earlyFailure = std::strcmp(argv[3], "partial") == 0
            || std::strcmp(argv[3], "mutex-fail") == 0 || std::strcmp(argv[3], "event-fail") == 0
            || std::strcmp(argv[3], "attribute-fail") == 0 || std::strcmp(argv[3], "attribute-type-fail") == 0
            || std::strcmp(argv[3], "condition-fail") == 0;
        const int expectedControllerShutdowns = earlyFailure ? 0 : 1;
        if (before != expectedControllerShutdowns || state(1) != 0 || state(2) != 0
            || !shutdown() || !shutdown() || state(0) != before || state(1) != 0 || state(2) != 0
            || state(5) != 3 || state(6) != 3) {
            return 7;
        }
        if (std::strcmp(argv[3], "inflight") == 0 && (state(3) != 1 || state(4) != 1)) {
            return 12;
        }
        if (SDL_WasInit(SDL_INIT_AUDIO) != (externalReference ? SDL_INIT_AUDIO : 0)) {
            return 8;
        }
    }
    std::puts("fixture_before_unload");
    std::fflush(stdout);
    cse_platform::UnloadModule(module);
    std::puts("fixture_after_unload");
    std::fflush(stdout);
    if (cse_platform::FindLoadedModule(argv[1])) {
        constexpr int ModuleStillLoaded = 11;
        std::puts("fixture_module_still_loaded");
        std::fflush(stdout);
        return ModuleStillLoaded;
    }
    constexpr Uint32 PostUnloadMilliseconds = 30;
    SDL_Delay(PostUnloadMilliseconds);
    if (externalReference) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    if (SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        return 10;
    }
    return 0;
}
