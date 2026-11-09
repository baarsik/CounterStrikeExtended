#include "controller_factory.h"
#include "process_channel.h"

#include <SDL3/SDL_filesystem.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace cse::browser;

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "browser_factory_failure=%s\n", message);
        std::exit(1);
    }
}

class Widget final : public IHTMLResponses_HL1 {
public:
    explicit Widget(IHTMLChromeController& owner, bool& moduleAlive) : owner_(owner), moduleAlive_(moduleAlive) {
        serializer = owner_.CreateSerializer(this);
        Require(serializer != nullptr, "factory exposes an initialized provider");
        owner_.CreateBrowser(serializer->GetResponseTarget(), false, "fixture-agent");
    }
    ~Widget() {
        Require(moduleAlive_, "widget destruction precedes module unload");
        owner_.RemoveBrowser(serializer->GetResponseTarget());
    }
    void BrowserReady() override {
        Require(moduleAlive_, "ready callback remains inside widget module lifetime");
        ++ready;
    }
    void BrowserClose() override {
        Require(moduleAlive_, "close callback remains inside widget module lifetime");
        ++closed;
        scheduledForDeletion = true;
    }
    void BrowserSetHTMLTitle(const char*) override {}
    void BrowserNeedsPaint(int, int, int, const unsigned char*, int, int, int, int, int, int,
        const unsigned char*) override {}
    void BrowserStartRequest(const char*, const char*, const char*, bool) override {}
    void BrowserURLChanged(const char*, const char*, bool) override {}
    void BrowserFinishedRequest(const char*, const char*) override {}
    void BrowserShowPopup() override {}
    void BrowserHidePopup() override {}
    void BrowserSizePopup(int, int, int, int) override {}
    void BrowserHorizontalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserVerticalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserGetZoomResponse(float) override {}
    void BrowserCanGoBackandForward(bool, bool) override {}
    void BrowserJSAlert(const char*) override {}
    void BrowserJSConfirm(const char*) override {}
    void BrowserPopupHTMLWindow(const char*, int, int, int, int) override {}
    void BrowserLoadingResource() override {}
    void BrowserStatusText(const char*) override {}
    void BrowserSetCursor(int) override {}
    void BrowserFileLoadDialog() override {}
    void BrowserShowToolTip(const char*) override {}
    void BrowserUpdateToolTip(const char*) override {}
    void BrowserHideToolTip() override {}
    void BrowserLinkAtPositionResponse(const char*, int, int) override {}

    IHTMLSerializer* serializer = nullptr;
    int ready = 0;
    int closed = 0;
    bool scheduledForDeletion = false;
private:
    IHTMLChromeController& owner_;
    bool& moduleAlive_;
};

int main(int count, char* arguments[]) {
    Require(count == 2 && SDL_Init(0), "factory fixture arguments and SDL provider");
    const auto directory = std::filesystem::absolute(std::filesystem::u8path(arguments[1]));
    std::filesystem::create_directories(directory);
    std::string error;
    ControllerConfig config;
    const auto base = directory / std::filesystem::u8path("Game data \xd0\xaf");
    const auto preferences = directory / std::filesystem::u8path("Owned preferences \xd0\xaf");
    Require(ResolveEngineControllerPaths(base.u8string(), preferences.u8string(),
        "../Browser helpers/fixture host \xd0\xaf", config, error), "configured bundle-relative Unicode host path");
    Require(std::filesystem::u8path(config.executable) ==
        (directory / std::filesystem::u8path("Browser helpers/fixture host \xd0\xaf")).lexically_normal() &&
        std::filesystem::u8path(config.profileDirectory) == (preferences / "browser").lexically_normal(),
        "host and writable profile roots remain separate and independent of the working directory");
    Require(config.fixtureKeychainId.empty(), "production configuration has no fixture identity");
    Require(!ResolveEngineControllerPaths("relative", preferences.u8string(), "host", config, error) &&
        !error.empty(), "relative application root rejected");
    Require(!ResolveEngineControllerPaths(base.u8string(), "relative", "host", config, error),
        "relative preference root rejected");
    Require(!ResolveEngineControllerPaths(base.u8string(), preferences.u8string(), directory.u8string(), config,
        error), "absolute configured host rejected");
    Require(!ResolveEngineControllerPaths(base.u8string(), preferences.u8string(), "", config, error),
        "missing configured host rejected");

    const char* borrowedBase = SDL_GetBasePath();
    Require(borrowedBase != nullptr, "actual SDL3 application directory");
    const std::string baseValue = borrowedBase;
    constexpr int ConfigurationQueries = 16;
    for (int query = 0; query < ConfigurationQueries; ++query) {
        Require(GetEngineControllerConfiguration(config, error), "actual SDL3 production path APIs");
        Require(std::filesystem::u8path(config.executable) ==
            (std::filesystem::u8path(baseValue) / CSE_BROWSER_HOST_RELATIVE_PATH).lexically_normal(),
            "packaging host constant is relative to actual SDL3 base semantics");
        Require(config.fixtureKeychainId.empty() && std::filesystem::u8path(config.profileDirectory).is_absolute(),
            "production profile absolute with stable default encryption identity");
#if CSE_BROWSER_DEBUG_PROFILE
        constexpr char ExpectedApplication[] = "CounterStrikeExtended Debug";
#else
        constexpr char ExpectedApplication[] = "CounterStrikeExtended";
#endif
        Require(std::filesystem::u8path(config.profileDirectory).parent_path().filename() == ExpectedApplication,
            "configuration-specific writable application profile");
        Require(SDL_GetBasePath() == borrowedBase && std::string(borrowedBase) == baseValue,
            "borrowed SDL3 base cache is never freed by the factory");
    }

    config = {};
    config.executable = (directory / "missing provider").u8string();
    config.profileDirectory = (directory / "uncreated profile").u8string();
    Require(!CreateInitializedController(config, error) && !error.empty() &&
        !std::filesystem::exists(std::filesystem::u8path(config.profileDirectory)),
        "missing host fails before creating a profile or exposing a controller");
    const auto helpers = directory / std::filesystem::u8path("Browser helpers \xd0\xaf");
    std::filesystem::create_directories(helpers);
    const auto executable = helpers / std::filesystem::u8path(CSE_FACTORY_FIXTURE_HOST).filename();
    std::filesystem::copy_file(std::filesystem::u8path(CSE_FACTORY_FIXTURE_HOST), executable,
        std::filesystem::copy_options::overwrite_existing);
#if defined(_WIN32)
    std::filesystem::copy_file(std::filesystem::u8path(baseValue) / "SDL3.dll", helpers / "SDL3.dll",
        std::filesystem::copy_options::overwrite_existing);
#endif
    config.executable = executable.u8string();
    const auto collision = directory / "profile-file";
    std::ofstream(collision).put('1');
    config.profileDirectory = collision.u8string();
    Require(!CreateInitializedController(config, error) && !error.empty(), "profile file collision rejects startup");
    config.profileDirectory = (directory / std::filesystem::u8path("isolated owned profile \xd0\xaf")).u8string();
    std::filesystem::create_directories(std::filesystem::u8path(config.profileDirectory));
    std::ofstream(std::filesystem::u8path(config.profileDirectory) / "bad-hello-once").put('1');
    Require(!CreateInitializedController(config, error) && !error.empty(),
        "handshake failure never exposes a usable controller");
    auto diagnostics = std::make_shared<ControllerDiagnostics>();
    config.diagnostics = diagnostics;
    auto controller = CreateInitializedController(config, error);
    Require(controller != nullptr && error.empty(), "factory recovery exposes initialized owned controller");
    bool moduleAlive = true;
    auto widget = std::make_unique<Widget>(*controller, moduleAlive);
    constexpr Uint64 ReadyMilliseconds = 5000;
    const Uint64 deadline = SDL_GetTicks() + ReadyMilliseconds;
    while (widget->ready != 1 && SDL_GetTicks() < deadline) {
        controller->RunFrame();
        SDL_Delay(1);
    }
    Require(widget->ready == 1, "factory widget receives actual framed readiness");
    BeginControllerShutdown(*controller);
    Require(widget->closed == 1 && widget->scheduledForDeletion, "engine begins deferred widget deletion");
    constexpr int DeferredFrames = 4;
    for (int frame = 0; frame < DeferredFrames; ++frame) {
        controller->RunFrame();
        Require(widget->serializer->GetBrowserHandle() > UINT32_MAX, "serializer lives through deferred VGUI frames");
    }
    widget.reset();
    moduleAlive = false;
    controller->Shutdown();
    controller.reset();
    Require(diagnostics->cleanShutdown && diagnostics->exitCode == 0,
        "widget drain precedes clean provider destruction and module unload");
    SDL_Quit();
    std::puts("native_browser_factory_fixture_passed=1");
    return 0;
}
