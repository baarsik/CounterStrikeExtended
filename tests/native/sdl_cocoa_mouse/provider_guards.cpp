#define main RetainedModeProviderMain
#include "../sdl_mouse_bridge/provider/provider_probe.cpp"
#undef main
#include "sdl3_native_cocoa_mouse.h"
#include <type_traits>

namespace {
using Owned = decltype(&CseSdl_CheckCocoaMouseOwnedV1);
using Focused = decltype(&CseSdl_CheckCocoaRelativeFocusV1);
using Absolute = decltype(&CseSdl_ReadCocoaMouseAbsoluteV1);
static_assert(std::is_same<Absolute, uint32_t (*)(uint32_t, void *, int32_t, int32_t, float *, float *)>::value,
              "Cocoa absolute prototype is fixed-width C");
struct Api {
    Owned owned;
    Focused focused;
    Absolute absolute;
};
Api LoadApi(Module module) {
    return {Symbol<Owned>(module, "CseSdl_CheckCocoaMouseOwnedV1"),
            Symbol<Focused>(module, "CseSdl_CheckCocoaRelativeFocusV1"),
            Symbol<Absolute>(module, "CseSdl_ReadCocoaMouseAbsoluteV1")};
}
void CheckAll(const Api &api, uint32_t expected) {
    void *poison = reinterpret_cast<void *>(static_cast<uintptr_t>(1));
    float x = 123.5f;
    float y = 456.5f;
    constexpr uint32_t version = CSE_SDL_COCOA_MOUSE_CONTRACT_V1;
    Require(api.owned(version, poison) == expected, "owned guard before opaque object access");
    Require(api.focused(version, poison, 640, 480) == expected, "focused guard before opaque object access");
    Require(api.absolute(version, poison, 640, 480, &x, &y) == expected, "absolute guard before opaque object access");
    Require(x == 123.5f && y == 456.5f, "guard output atomicity");
}
}

int main(int argc, char **argv) {
    Require(argc >= 3, "mode and exact provider required");
    const std::string mode = argv[1];
    Module provider = Open(argv[2]);
    const Api api = LoadApi(provider);
    const auto setMain = Symbol<decltype(&SDL_SetMainReady)>(provider, "SDL_SetMainReady");
    const auto init = Symbol<decltype(&SDL_InitSubSystem)>(provider, "SDL_InitSubSystem");
    const auto quit = Symbol<decltype(&SDL_Quit)>(provider, "SDL_Quit");
    const auto defaultHandler = Symbol<decltype(&SDL_GetDefaultAssertionHandler)>(provider,
        "SDL_GetDefaultAssertionHandler");
    if (mode == "preinit") {
        Require(argc == 4 && !Loaded(argv[3]), "foreign absent before preinit guards");
        Phase("COCOA_PREINIT_BEGIN");
        CheckAll(api, CseSdlCocoaMouseNotReady);
        Require(api.owned(0, reinterpret_cast<void *>(static_cast<uintptr_t>(1))) == CseSdlCocoaMouseContractMismatch,
                "contract mismatch before provider initialization");
        Phase("COCOA_PREINIT_END");
        Require(!Loaded(argv[3]), "guards did not initialize public dispatch");
        Phase("COCOA_EXPLICIT_DISPATCH_BEGIN");
        defaultHandler();
        Phase("COCOA_EXPLICIT_DISPATCH_END");
        Require(Loaded(argv[3]), "explicit public call loads actual foreign provider");
    } else if (mode == "same") {
        setMain();
        Require(init(SDL_INIT_VIDEO), "initialize actual dummy video");
        float x = 1.0f;
        float y = 2.0f;
        Require(api.focused(CSE_SDL_COCOA_MOUSE_CONTRACT_V1,
            reinterpret_cast<void *>(static_cast<uintptr_t>(1)), 640, 480) == CseSdlCocoaMouseWrongMode,
            "absolute mode rejects relative focus before AppKit");
        std::thread worker([&] { CheckAll(api, CseSdlCocoaMouseWrongThread); });
        worker.join();
        Require(api.absolute(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, nullptr, 640, 480, &x, &y) ==
            CseSdlCocoaMouseInvalidArgument, "invalid arguments before opaque access");
        Require(x == 1.0f && y == 2.0f, "invalid arguments preserve outputs");
        quit();
        CheckAll(api, CseSdlCocoaMouseNotReady);
    } else if (mode == "foreign") {
        Require(argc == 4 && !Loaded(argv[3]), "foreign starts unloaded");
        Phase("COCOA_EXPLICIT_DISPATCH_BEGIN");
        defaultHandler();
        Phase("COCOA_EXPLICIT_DISPATCH_END");
        Require(Loaded(argv[3]), "actual foreign dispatch initialized outside query");
        setMain();
        Require(init(SDL_INIT_VIDEO), "B actual video ready");
        const auto localInit = Symbol<LocalInit>(provider, "CseSdlFixture_InitLocalVideo");
        const auto localQuit = Symbol<LocalQuit>(provider, "CseSdlFixture_QuitLocalVideo");
        Require(localInit() == 1, "A local video ready after public dispatch");
        Phase("COCOA_FOREIGN_BEGIN");
        constexpr unsigned repetitions = 1024;
        for (unsigned index = 0; index < repetitions; ++index) {
            CheckAll(api, CseSdlCocoaMouseForeignDispatch);
        }
        Phase("COCOA_FOREIGN_END");
        quit();
        localQuit();
        CheckAll(api, CseSdlCocoaMouseNotReady);
    } else {
        Require(false, "unknown Cocoa guard mode");
    }
    Close(provider);
    return 0;
}
