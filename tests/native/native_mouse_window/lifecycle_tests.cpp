#define main RetainedWindowRegressionMain
#include "window_tests.cpp"
#undef main
#include <cstring>
#if defined(__APPLE__)
#include "VGui1NativeRuntime.h"
#endif

[[noreturn]] void Sys_Error(const char *, ...);
namespace {
int errors = 0;
int shows = 0;
int destroys = 0;
#if defined(__APPLE__)
bool fontReady = true;
#endif
class GameLifecycle {
    public:
    SDL_Window *m_hSDLWindow = fixtureWindow;
    bool Startup() {
#include "hidden_bind.inc"
        ++shows;
        return true;
    }
    bool Shutdown() {
#include "shutdown_prefix.inc"
        float x = 11.0f;
        float y = 12.0f;
        CHECK(!cse::TrySampleNativeMousePosition(fixtureWindow, 640, 480, &x, &y));
        CHECK(x == 11.0f && y == 12.0f);
        ++destroys;
        return true;
    }
};
void Setup() {
    Reset();
    active = &cse::NativeMouseWindowState();
    active->Invalidate(nullptr);
    shows = 0;
    destroys = 0;
    errors = 0;
}
} // namespace
[[noreturn]] void Sys_Error(const char *, ...) {
    ++errors;
    CHECK(errors == 1 && shows == 0);
    NoSample(*active);
    std::fputs("expected-startup-fatal-before-show\n", stderr);
    std::exit(21);
}
#if defined(__APPLE__)
namespace cse_vgui1_native {
Result RendererWillDestroy(bool lost) noexcept {
    CHECK(!lost);
    return fontReady ? Result::Ready : Result::Busy;
}
} // namespace cse_vgui1_native
#endif

int main(int argc, char **argv) {
    CHECK(RetainedWindowRegressionMain() == 0);
#if UINTPTR_MAX > UINT32_MAX
    Setup();
    GameLifecycle game;
    if (argc == 2) {
        if (std::strcmp(argv[1], "--startup-fail-setter") == 0) {
            scene.setterSuccess = false;
        } else if (std::strcmp(argv[1], "--startup-fail-postcheck") == 0) {
            scene.applyMode = false;
            scene.mode = CseSdlMouseModeRelative;
        } else {
            CHECK(false);
        }
        game.Startup();
        CHECK(false);
    }
    CHECK(game.Startup());
    CHECK(shows == 1 && errors == 0);
#if defined(__APPLE__)
    fontReady = false;
    CHECK(!game.Shutdown());
    CHECK(destroys == 0);
    float x = 0.0f;
    float y = 0.0f;
    CHECK(cse::TrySampleNativeMousePosition(fixtureWindow, 640, 480, &x, &y));
    fontReady = true;
#endif
    CHECK(game.Shutdown());
    CHECK(destroys == 1);
    Setup();
    CHECK(game.Startup());
    CHECK(game.Shutdown());
#else
    // Exact source guards exclude native admission/invalidation from retained Win32.
    CHECK(argc == 1 && argv != nullptr);
    Setup();
    GameLifecycle game;
    CHECK(game.Startup() && shows == 1);
#endif
    std::printf("actual-lifecycle-hooks checks=%d\n", checks);
    return 0;
}
