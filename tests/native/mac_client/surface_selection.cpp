#include "controls.h"
#include "FileSystem.h"
#include "native_filesystem_module.h"
#include "native_standalone_abi.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <iterator>
#include <string>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

bool SameImage(const void* first, const void* second) {
    Dl_info firstImage{};
    Dl_info secondImage{};
    return dladdr(first, &firstImage) && dladdr(second, &secondImage) &&
        firstImage.dli_fbase == secondImage.dli_fbase;
}

void RequireNoWindows() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_free(windows);
    Require(count == 0, "provider selection creates no standalone or client window");
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "expected actual client runtime directory");
    const std::string runtime(argv[1]);
    void* guiImage = dlopen((runtime + "/vgui2.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* filesystemImage = dlopen((runtime + "/filesystem_stdio.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    Require(guiImage && filesystemImage, "load actual standalone providers");
    const auto guiFactory = cse::standalone_abi::CheckedFactory(guiImage);
    const auto filesystemFactory = cse::filesystem_abi::CheckedFactory(filesystemImage);
    Require(guiFactory && filesystemFactory, "resolve actual checked standalone/filesystem factories");
    Require(cse::standalone_abi::ValidateViews(guiFactory, filesystemFactory),
        "admit complete standalone interfaces and physical method owners");
    Require(SDL_WasInit(SDL_INIT_VIDEO) == 0 && SDL_WasInit(SDL_INIT_AUDIO) == 0,
        "standalone factory discovery starts no video/audio subsystem");
    void* engineImage = dlopen((runtime + "/hw.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    Require(engineImage != nullptr, "load actual client engine");
    const auto engineFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(engineImage, "CreateInterface"));
    Require(engineFactory != nullptr, "resolve actual client engine factory");
    // The existing hardware BaseUISurface constructor initializes SDL video for its default cursors.
    const SDL_InitFlags engineSubsystems = SDL_WasInit(0);
    RequireNoWindows();
    auto* engineSurface = static_cast<vgui2::ISurface*>(engineFactory("VGUI_Surface026", nullptr));
    auto* standaloneSurface = static_cast<vgui2::ISurface*>(guiFactory("VGUI_Surface026", nullptr));
    constexpr std::size_t SurfaceMethodCount = 105;
    Require(cse::dedicated_abi::ViewCompatible<vgui2::ISurface>(engineSurface, engineFactory, SurfaceMethodCount),
        "engine Surface is an actual engine-owned interface view");
    Require(engineSurface != standaloneSurface, "engine and standalone surfaces are distinct real providers");
    Require(engineFactory("EngineSurface007", nullptr) != nullptr, "engine retains its companion EngineSurface");

    // Resolve existing exports from each actual image: no copied selector or test-only production hook.
    const auto initialize = reinterpret_cast<decltype(&vgui2::VGuiControls_Init)>(
        dlsym(engineImage, "_ZN5vgui217VGuiControls_InitEPKcPPFPvS1_PiEi"));
    const auto selectedSurface = reinterpret_cast<decltype(&vgui2::surface)>(
        dlsym(engineImage, "_ZN5vgui27surfaceEv"));
    const auto shutdown = reinterpret_cast<decltype(&vgui2::VGuiControls_Shutdown)>(
        dlsym(engineImage, "_ZN5vgui221VGuiControls_ShutdownEv"));
    auto* internalSurface = static_cast<vgui2::ISurface**>(dlsym(guiImage, "_ZN5vgui210g_pSurfaceE"));
    Require(initialize && selectedSurface && shutdown && internalSurface, "resolve actual consumer exports");
    Require(SameImage(reinterpret_cast<void*>(initialize), reinterpret_cast<void*>(engineFactory)) &&
        SameImage(reinterpret_cast<void*>(selectedSurface), reinterpret_cast<void*>(engineFactory)) &&
        SameImage(reinterpret_cast<void*>(shutdown), reinterpret_cast<void*>(engineFactory)) &&
        SameImage(internalSurface, reinterpret_cast<void*>(guiFactory)), "consumer exports belong to requested images");
    auto* gui = static_cast<vgui2::IVGui*>(guiFactory("VGUI_ivgui006", nullptr));

    // CBaseUI supplies engine, VGUI, filesystem in this order. Repeat after the reversed-order control
    // to catch stale IVGui selection across shutdown/reinitialization as well as first-provider regressions.
    for (const bool engineFirst : {true, false, true}) {
        CreateInterfaceFn factories[] = {
            engineFirst ? engineFactory : guiFactory,
            engineFirst ? guiFactory : engineFactory,
            filesystemFactory
        };
        Require(initialize("CLIENT_SURFACE_SELECTION", factories, static_cast<int>(std::size(factories))),
            "actual engine controls initialize with complete real providers");
        auto* expected = engineFirst ? engineSurface : standaloneSurface;
        Require(selectedSurface() == expected, "actual engine controls select the first real Surface provider");
        Require(*internalSurface == expected, "actual VGUI internal consumer selects the same Surface provider");
        Require(SDL_WasInit(0) == engineSubsystems, "actual consumer selection starts no additional SDL subsystem");
        RequireNoWindows();
        std::printf("PASS actual consumer selection: %s\n", engineFirst ? "engine first" : "standalone first");
        gui->Shutdown();
        shutdown();
        Require(selectedSurface() == nullptr, "actual engine controls release their borrowed Surface");
    }
    standaloneSurface->Shutdown();
    engineSurface->Shutdown();
    dlclose(engineImage);
    dlclose(guiImage);
    dlclose(filesystemImage);
    SDL_Quit();
    std::puts("PASS actual provider coexistence, consumer order, reopen and lazy standalone video/audio");
}
