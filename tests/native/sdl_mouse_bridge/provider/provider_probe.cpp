#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "sdl3_native_mouse_bridge.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#if defined(_WIN32)
#include <Windows.h>
using Module = HMODULE;
#else
#include <dlfcn.h>
using Module = void *;
#endif

static std::atomic<unsigned> handlerCalls{0};

static void Phase(const char *name) {
    std::printf("CSE_BRIDGE_PHASE:%s\n", name);
    std::fflush(stdout);
}

static void Require(bool value, const char *message) {
    if (!value) {
        constexpr int assertionFailure = 17;
        std::fprintf(stderr, "provider-probe:%s\n", message);
        std::exit(assertionFailure);
    }
}

static SDL_AssertState SDLCALL CustomHandler(const SDL_AssertData *, void *) {
    ++handlerCalls;
    return SDL_ASSERTION_ABORT;
}

static Module Open(const char *path) {
#if defined(_WIN32)
    Module module = LoadLibraryExA(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    Module module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        std::fprintf(stderr, "dlopen:%s\n", dlerror());
    }
#endif
    Require(module != nullptr, "load exact owned provider");
    return module;
}

static bool Loaded(const char *path) {
#if defined(_WIN32)
    return GetModuleHandleA(path) != nullptr;
#else
    Module module = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
    if (module) {
        dlclose(module);
    }
    return module != nullptr;
#endif
}

static void Close(Module module) {
#if defined(_WIN32)
    Require(FreeLibrary(module) != 0, "release owned loader reference");
#else
    Require(dlclose(module) == 0, "release owned loader reference");
#endif
}

template <typename Function> static Function Symbol(Module module, const char *name) {
#if defined(_WIN32)
    FARPROC symbol = GetProcAddress(module, name);
#else
    void *symbol = dlsym(module, name);
#endif
    Require(symbol != nullptr, name);
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(symbol), "Native loader C function-pointer representation");
    std::memcpy(&function, &symbol, sizeof(function));
    return function;
}

using Query = decltype(&CseSdl_QueryMouseModeV1);
using LocalInit = uint32_t (CSE_SDL_NATIVE_MOUSE_CALL *)(void);
using LocalQuit = void (CSE_SDL_NATIVE_MOUSE_CALL *)(void);

int main(int argc, char **argv) {
    Require(argc >= 3, "mode and exact provider path required");
    const std::string mode = argv[1];
    Module provider = Open(argv[2]);
    const Query query = Symbol<Query>(provider, "CseSdl_QueryMouseModeV1");
    const auto setMainReady = Symbol<decltype(&SDL_SetMainReady)>(provider, "SDL_SetMainReady");
    const auto init = Symbol<decltype(&SDL_InitSubSystem)>(provider, "SDL_InitSubSystem");
    const auto quit = Symbol<decltype(&SDL_Quit)>(provider, "SDL_Quit");
    const auto defaultHandler = Symbol<decltype(&SDL_GetDefaultAssertionHandler)>(provider,
        "SDL_GetDefaultAssertionHandler");

    if (mode == "preinit") {
        Require(argc == 4, "preinit requires valid foreign-image sentinel");
        Require(!Loaded(argv[3]), "foreign absent before rejected query");
        Phase("PREINIT_QUERY_BEGIN");
        Require(query(0) == CseSdlMouseModeContractMismatch, "preinit contract check");
        Require(query(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeNotReady, "preinit unavailable");
        Phase("PREINIT_QUERY_END");
        Require(!Loaded(argv[3]), "rejected query did not initialize dynapi/load foreign image");
        Phase("FOREIGN_DISPATCH_INIT_BEGIN");
        defaultHandler();
        Phase("FOREIGN_DISPATCH_INIT_END");
        Require(Loaded(argv[3]), "explicit public call initializes real foreign dispatch outside query");
        quit();
        std::puts("preinit: rejected query did not initialize dispatch/video; explicit public call loaded sentinel");
    } else if (mode == "same") {
        setMainReady();
        Require(init(SDL_INIT_VIDEO), "explicit vendor dummy video initialization");
        const auto currentHandler = Symbol<decltype(&SDL_GetAssertionHandler)>(provider, "SDL_GetAssertionHandler");
        const auto setHandler = Symbol<decltype(&SDL_SetAssertionHandler)>(provider, "SDL_SetAssertionHandler");
        const SDL_AssertionHandler originalDefault = defaultHandler();
        setHandler(CustomHandler, nullptr);
        Require(currentHandler(nullptr) == CustomHandler && defaultHandler() == originalDefault,
            "custom handler is independent of default identity");
        constexpr unsigned repeatedQueries = 1024;
        Phase("SAME_QUERY_BEGIN");
        for (unsigned index = 0; index < repeatedQueries; ++index) {
            Require(query(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeAbsolute, "same provider actual mode");
        }
        Phase("SAME_QUERY_END");
        Require(currentHandler(nullptr) == CustomHandler, "query preserves custom handler");
        uint32_t workerResult = UINT32_MAX;
        std::thread worker([&] { workerResult = query(CSE_SDL_MOUSE_MODE_CONTRACT_V1); });
        worker.join();
        Require(workerResult == CseSdlMouseModeWrongThread, "stable-lifetime actual worker rejection");
        Require(handlerCalls == 0, "no assertion handler invoked");
        setHandler(nullptr, nullptr);
        quit();
        Require(query(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeNotReady, "post-quit unavailable");
        std::puts("same: 1024 absolute queries; custom handler preserved; worker rejected; post-quit unavailable");
    } else if (mode == "foreign") {
        Require(argc == 4, "foreign requires exact image B");
        Require(!Loaded(argv[3]), "B absent before explicit A public dispatch");
        Phase("FOREIGN_DISPATCH_INIT_BEGIN");
        defaultHandler();
        Phase("FOREIGN_DISPATCH_INIT_END");
        Require(Loaded(argv[3]), "A public dynapi dispatch initialized outside observed query");
        setMainReady();
        Require(init(SDL_INIT_VIDEO), "B public video ready before query");
        const LocalInit localInit = Symbol<LocalInit>(provider, "CseSdlFixture_InitLocalVideo");
        const LocalQuit localQuit = Symbol<LocalQuit>(provider, "CseSdlFixture_QuitLocalVideo");
        Require(localInit() == 1, "fixture-only A local video ready after public dispatch initialization");
        Module other = Open(argv[3]);
        const Query otherQuery = Symbol<Query>(other, "CseSdl_QueryMouseModeV1");
        Phase("B_QUERY_BEGIN");
        Require(otherQuery(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeAbsolute, "B local readiness/mode proven");
        Phase("B_QUERY_END");
        constexpr unsigned repeatedQueries = 1024;
        Phase("FOREIGN_QUERY_BEGIN");
        for (unsigned index = 0; index < repeatedQueries; ++index) {
            Require(query(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeForeignDispatch,
                "A rejects genuine foreign dispatch with both local videos ready");
        }
        Phase("FOREIGN_QUERY_END");
        quit();
        localQuit();
        Require(query(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeNotReady, "A post-quit unavailable");
        Close(other);
        std::puts("foreign: public dispatch initialized first; both local videos ready; 1024 ForeignDispatch results");
    } else {
        Require(false, "unknown mode");
    }
    Close(provider);
    // Upstream dynapi intentionally retains its B loader reference until process exit.
    return 0;
}
