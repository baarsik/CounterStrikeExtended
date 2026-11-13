#include "browser_controller.h"
#include "browser_commands.h"
#include "process_channel.h"
#include "../../../src/engine/utlstring.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace cse::browser;

static void Require(bool value, const char* text) {
    if (!value) {
        std::fprintf(stderr, "browser_controller_failure=%s\n", text);
        std::exit(1);
    }
}

static bool Output(Header header, const Writer& payload) {
    header.payloadBytes = static_cast<std::uint32_t>(payload.Data().size());
    const auto encoded = EncodeHeader(header);
    return std::fwrite(encoded.data(), 1, encoded.size(), stdout) == encoded.size() &&
        std::fwrite(payload.Data().data(), 1, payload.Data().size(), stdout) == payload.Data().size() &&
        std::fflush(stdout) == 0;
}

static int Host(const std::vector<std::string>& arguments) {
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    constexpr char StorageSwitch[] = "--cse-storage=";
    Require(arguments.size() >= 3 && arguments[2].find(StorageSwitch) == 0, "controlled host arguments");
    const auto folder = std::filesystem::u8path(arguments[2].substr(sizeof(StorageSwitch) - 1));
    if (std::filesystem::exists(folder / "fixture-keychain-option")) {
        constexpr std::size_t FixtureIdentityLength = 64;
        Require(arguments.size() == 4 && arguments[3] == "--cse-fixture-keychain=" +
            std::string(FixtureIdentityLength, 'a'), "isolated encrypted fixture identity forwarded");
    } else {
        Require(arguments.size() == 3, "production launch contains only owned production arguments");
    }
    const auto failOnce = folder / "bad-hello-once";
    const bool badHello = std::filesystem::exists(failOnce);
    if (badHello) {
        std::filesystem::remove(failOnce);
    }
    Writer hello;
    hello.U32(badHello ? ProviderContractVersion + 1 : ProviderContractVersion);
    hello.U32(ProviderApiVersion);
    Require(Output({Kind::Control, static_cast<std::uint32_t>(Control::Hello), 0, 0, 0}, hello), "host hello");
    Decoder decoder(1024 * 1024);
    std::array<std::uint8_t, 4096> bytes;
    for (;;) {
#if defined(_WIN32)
        const int count = _read(_fileno(stdin), bytes.data(), static_cast<unsigned int>(bytes.size()));
#else
        const int count = static_cast<int>(read(STDIN_FILENO, bytes.data(), bytes.size()));
#endif
        if (count == 0) {
            return decoder.Finish() ? 0 : 1;
        }
        Require(count > 0, "host input pipe");
        std::size_t offset = 0;
        while (offset < static_cast<std::size_t>(count)) {
            Message command;
            std::size_t used = 0;
            const auto status = decoder.Feed(bytes.data() + offset, static_cast<std::size_t>(count) - offset,
                used, command);
            Require(status != DecodeStatus::Invalid && used != 0, "host command framing");
            offset += used;
            if (status != DecodeStatus::Complete) {
                continue;
            }
            if (command.header.kind == Kind::Command &&
                (command.header.operation == eHTMLCommands_KeyDown || command.header.operation == eHTMLCommands_KeyUp)) {
                Reader reader(command.payload);
                std::int32_t key = 0;
                std::int32_t modifiers = 0;
                constexpr int MaximumVirtualKey = 0xff;
                Require(reader.I32(key) && reader.I32(modifiers) && reader.Finish() &&
                    key > 0 && key <= MaximumVirtualKey,
                    "unmapped UI keys must not cross the native provider boundary");
            } else if (command.header.kind == Kind::Control &&
                command.header.operation == static_cast<std::uint32_t>(Control::GetCookies)) {
                Reader reader(command.payload);
                std::string url;
                std::string domain;
                Require(reader.String(url) && reader.String(domain) && reader.Finish() &&
                    url == "https://fixture.invalid/" && domain == "fixture.invalid", "cookie request fields");
                Writer cookies;
                cookies.String("fixture=persisted");
                Output({Kind::Control, static_cast<std::uint32_t>(Control::Cookies), 0, command.header.request, 0}, cookies);
            } else if (command.header.kind == Kind::Command && command.header.operation == eHTMLCommands_BrowserCreate) {
                Reader reader(command.payload);
                bool popup = false;
                std::string agent;
                Require(reader.Boolean(popup) && reader.String(agent) && reader.Finish() && !popup &&
                    agent == "fixture-agent", "create browser fields");
                Writer ready;
                Output({Kind::Event, eHTMLCommands_BrowserReady, command.header.browser, 0, 0}, ready);
            } else if (command.header.kind == Kind::Command && command.header.operation == eHTMLCommands_PostURL) {
                const auto closeBrowser = folder / "close-after-post";
                if (std::filesystem::exists(closeBrowser)) {
                    std::filesystem::remove(closeBrowser);
                    Writer closed;
                    Output({Kind::Event, eHTMLCommands_Close, command.header.browser, 0, 0}, closed);
                    Output({Kind::Event, eHTMLCommands_Close, command.header.browser, 0, 0}, closed);
                    continue;
                }
                const auto partialPaint = folder / "partial-paint";
                if (std::filesystem::exists(partialPaint)) {
                    std::filesystem::remove(partialPaint);
                    Writer paint;
                    // Full 8x6 backing pixels, but only the middle two rows changed.
                    for (int value : {1, 8, 6, 2, 3, 3, 2, 0, 0}) {
                        paint.I32(value);
                    }
                    std::vector<std::uint8_t> pixels(8 * 6 * 4);
                    pixels[0] = 11;
                    pixels[8 * 3 * 4] = 77;
                    pixels.back() = 99;
                    paint.Bytes(pixels.data(), pixels.size());
                    paint.Bytes(nullptr, 0);
                    Output({Kind::Pixels, eHTMLCommands_NeedsPaint, command.header.browser, 0, 0}, paint);
                    continue;
                }
                const auto popupPaint = folder / "popup-paint";
                if (std::filesystem::exists(popupPaint)) {
                    std::filesystem::remove(popupPaint);
                    Writer paint;
                    for (int value : {1, 8, 6, 0, 0, 0, 0, 2, 2}) {
                        paint.I32(value);
                    }
                    const std::vector<std::uint8_t> pixels(8 * 6 * 4, 13);
                    const std::vector<std::uint8_t> popupPixels(2 * 2 * 4, 27);
                    paint.Bytes(pixels.data(), pixels.size());
                    paint.Bytes(popupPixels.data(), popupPixels.size());
                    Output({Kind::Pixels, eHTMLCommands_NeedsPaint, command.header.browser, 0, 0}, paint);
                    continue;
                }
                const auto invalidUpdate = folder / "invalid-update";
                if (std::filesystem::exists(invalidUpdate)) {
                    std::ifstream input(invalidUpdate);
                    Writer paint;
                    for (int value : {1, 8, 6}) {
                        paint.I32(value);
                    }
                    int value = 0;
                    for (int field = 0; field < 4; ++field) {
                        Require(static_cast<bool>(input >> value), "invalid-update fixture fields");
                        paint.I32(value);
                    }
                    input.close();
                    std::filesystem::remove(invalidUpdate);
                    paint.I32(0);
                    paint.I32(0);
                    const std::vector<std::uint8_t> pixels(8 * 6 * 4);
                    paint.Bytes(pixels.data(), pixels.size());
                    paint.Bytes(nullptr, 0);
                    Output({Kind::Pixels, eHTMLCommands_NeedsPaint, command.header.browser, 0, 0}, paint);
                    continue;
                }
                const auto badPaint = folder / "out-of-range-paint";
                if (std::filesystem::exists(badPaint)) {
                    std::filesystem::remove(badPaint);
                    constexpr int InvalidWidth = 8193; // One beyond the provider's supported viewport bound.
                    constexpr int PixelBytes = 4;
                    Writer paint;
                    paint.I32(1);
                    paint.I32(InvalidWidth);
                    paint.I32(1);
                    paint.I32(0);
                    paint.I32(0);
                    paint.I32(InvalidWidth);
                    paint.I32(1);
                    paint.I32(0);
                    paint.I32(0);
                    const std::vector<std::uint8_t> pixels(InvalidWidth * PixelBytes);
                    paint.Bytes(pixels.data(), pixels.size());
                    paint.Bytes(nullptr, 0);
                    Output({Kind::Pixels, eHTMLCommands_NeedsPaint, command.header.browser, 0, 0}, paint);
                    continue;
                }
                const auto crash = folder / "crash-after-create";
                if (std::filesystem::exists(crash)) {
                    std::filesystem::remove(crash);
                    return 17;
                }
                Reader reader(command.payload);
                std::string url;
                std::string post;
                Require(reader.String(url) && reader.String(post) && reader.Finish() &&
                    url == "https://fixture.invalid/" && post == "field=\xd0\xaf", "post body preserved");
                Writer title;
                title.String("native \xd0\xaf");
                Output({Kind::Event, eHTMLCommands_SetHTMLTitle, command.header.browser, 0, 0}, title);
                // Queued events for an immediately removed browser must not reach its freed target.
                Output({Kind::Event, eHTMLCommands_SetHTMLTitle, command.header.browser, 0, 0}, title);
            }
        }
    }
}

class Responses final : public IHTMLResponses_HL1 {
public:
    IHTMLChromeController* controller = nullptr;
    IHTMLSerializer* serializer = nullptr;
    int ready = 0;
    int titles = 0;
    int closed = 0;
    int paints = 0;
    bool expectPopupPaint = false;
    bool removeOnTitle = false;
    bool removeOnClose = false;
    bool alive = true;

    void BrowserReady() override {
        Require(alive, "ready callback after widget module lifetime");
        ++ready;
    }
    void BrowserSetHTMLTitle(const char* title) override {
        Require(alive, "title callback after widget module lifetime");
        Require(std::string(title) == "native \xd0\xaf", "UTF-8 title");
        ++titles;
        if (removeOnTitle) {
            controller->RemoveBrowser(serializer->GetResponseTarget());
            serializer = nullptr;
        }
    }
    void BrowserNeedsPaint(int texture, int width, int height, const unsigned char* pixels,
        int x, int y, int updateWidth, int updateHeight, int popupWidth, int popupHeight,
        const unsigned char* popup) override {
        if (expectPopupPaint) {
            Require(texture == 1 && width == 8 && height == 6 && x == 0 && y == 0 &&
                updateWidth == 0 && updateHeight == 0 && popupWidth == 2 && popupHeight == 2 && popup,
                "popup-only paint survives dispatch with empty main bounds");
            Require(pixels[0] == 13 && pixels[8 * 6 * 4 - 1] == 13 && popup[0] == 27 && popup[15] == 27,
                "popup-only paint preserves both backing buffers");
        } else {
            Require(texture == 1 && width == 8 && height == 6 && x == 2 && y == 3 &&
                updateWidth == 3 && updateHeight == 2 && popupWidth == 0 && popupHeight == 0 && !popup,
                "partial paint bounds survive wire dispatch");
            Require(pixels[0] == 11 && pixels[8 * 3 * 4] == 77 && pixels[8 * 6 * 4 - 1] == 99,
                "partial paint retains full backing pointer and row stride");
        }
        ++paints;
        serializer->NeedsPaintResponse(texture);
    }
    void BrowserStartRequest(const char* , const char* , const char* , bool ) override {}
    void BrowserURLChanged(const char* , const char* , bool ) override {}
    void BrowserFinishedRequest(const char* , const char* ) override {}
    void BrowserShowPopup() override {}
    void BrowserHidePopup() override {}
    void BrowserSizePopup(int , int , int , int ) override {}
    void BrowserHorizontalScrollBarSizeResponse(int , int , int , int , int , int , float ) override {}
    void BrowserVerticalScrollBarSizeResponse(int , int , int , int , int , int , float ) override {}
    void BrowserGetZoomResponse(float ) override {}
    void BrowserCanGoBackandForward(bool , bool ) override {}
    void BrowserJSAlert(const char* ) override {}
    void BrowserJSConfirm(const char* ) override {}
    void BrowserPopupHTMLWindow(const char* , int , int , int , int ) override {}
    void BrowserLoadingResource() override {}
    void BrowserStatusText(const char* ) override {}
    void BrowserSetCursor(int ) override {}
    void BrowserFileLoadDialog() override {}
    void BrowserShowToolTip(const char* ) override {}
    void BrowserUpdateToolTip(const char* ) override {}
    void BrowserHideToolTip() override {}
    void BrowserClose() override {
        Require(alive, "close callback after widget module lifetime");
        ++closed;
        if (removeOnClose) {
            controller->RemoveBrowser(serializer->GetResponseTarget());
            serializer = nullptr;
        }
    }
    void BrowserLinkAtPositionResponse(const char* , int , int ) override {}
};

static void Pump(IHTMLChromeController& controller, const int& value, int expected) {
    constexpr Uint64 DeadlineMilliseconds = 5000;
    const Uint64 deadline = SDL_GetTicks() + DeadlineMilliseconds;
    while (value != expected && SDL_GetTicks() < deadline) {
        controller.RunFrame();
        SDL_Delay(1);
    }
    Require(value == expected, "controller callback deadline");
}

static int Run(const std::vector<std::string>& arguments) {
    if (arguments.size() > 1 && arguments[1] == "--cse-browser-host") {
        return Host(arguments);
    }
    Require(arguments.size() == 2 && SDL_Init(0), "controller test arguments");
    const auto directory = std::filesystem::u8path(arguments[1]);
    std::filesystem::create_directories(directory);
    std::filesystem::remove(directory / "fixture-keychain-option");
    std::ofstream(directory / "bad-hello-once").put('1');
    auto controller = CreateController({arguments[0], directory.u8string()});
    Require(!controller->Init("cache", "cookies"), "ABI mismatch rejected before callbacks");
    Require(controller->Init("cache", "cookies"), "retry after rejected provider");
    Require(controller->Init("cache", "cookies"), "idempotent initialization");
    Responses responses;
    responses.controller = controller.get();
    responses.serializer = controller->CreateSerializer(&responses);
    Require(responses.serializer != nullptr && responses.serializer->GetBrowserHandle() > UINT32_MAX,
        "native serializer identity above 4 GiB");
    Require(responses.serializer->BrowserGetIndex() == -1, "navigation remains pending before BrowserReady");
    controller->CreateBrowser(responses.serializer->GetResponseTarget(), false, "fixture-agent");
    Pump(*controller, responses.ready, 1);
    Require(responses.serializer->BrowserGetIndex() > 0, "browser index published before ready callback returns");
    // VGUI forwards KEY_NONE for SDL media keys it cannot translate. The browser must remain usable.
    responses.serializer->KeyDown(0, 0);
    responses.serializer->KeyUp(0, 0);
    constexpr int FirstOutOfRangeVirtualKey = 256;
    responses.serializer->KeyDown(FirstOutOfRangeVirtualKey, 0);
    responses.serializer->KeyUp(-1, 0);
    CUtlString cookies;
    controller->GetWebCookiesForURL(&cookies, "https://fixture.invalid/", "fixture.invalid");
    Require(cookies.m_pchString && std::string(cookies.m_pchString) == "fixture=persisted", "cookie reply ownership");
    std::ofstream(directory / "partial-paint").put('1');
    responses.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    Pump(*controller, responses.paints, 1);
    responses.expectPopupPaint = true;
    std::ofstream(directory / "popup-paint").put('1');
    responses.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    Pump(*controller, responses.paints, 2);
    responses.removeOnTitle = true;
    responses.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    Pump(*controller, responses.titles, 1);
    for (int index = 0; index < 10; ++index) {
        controller->RunFrame();
    }
    Require(responses.titles == 1 && responses.serializer == nullptr, "synchronous callback detachment");
    controller->Shutdown();
    controller->Shutdown();
    Require(controller->Init("cache", "cookies"), "restart after normal shutdown");
    Responses fault;
    fault.controller = controller.get();
    fault.serializer = controller->CreateSerializer(&fault);
    Require(fault.serializer != nullptr, "fault fixture serializer");
    controller->CreateBrowser(fault.serializer->GetResponseTarget(), false, "fixture-agent");
    Pump(*controller, fault.ready, 1);
    std::ofstream(directory / "crash-after-create").put('1');
    fault.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    Pump(*controller, fault.closed, 1);
    Require(fault.serializer->GetBrowserHandle() > UINT32_MAX, "serializer survives deferred GUI deletion");
    fault.serializer->KeyChar('A');
    controller->RemoveBrowser(fault.serializer->GetResponseTarget());
    fault.serializer = nullptr;
    controller->Shutdown();
    Require(controller->Init("cache", "cookies"), "restart after provider failure");
    Responses invalidPaint;
    invalidPaint.serializer = controller->CreateSerializer(&invalidPaint);
    Require(invalidPaint.serializer != nullptr, "invalid paint fixture serializer");
    controller->CreateBrowser(invalidPaint.serializer->GetResponseTarget(), false, "fixture-agent");
    Pump(*controller, invalidPaint.ready, 1);
    std::ofstream(directory / "out-of-range-paint").put('1');
    invalidPaint.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    Pump(*controller, invalidPaint.closed, 1);
    controller->RemoveBrowser(invalidPaint.serializer->GetResponseTarget());
    invalidPaint.serializer = nullptr;
    controller->Shutdown();
    for (const std::array<int, 4> update : std::vector<std::array<int, 4>>{
        {-1, 0, 1, 1}, {0, -1, 1, 1}, {0, 0, -1, 1}, {0, 0, 1, -1}, {7, 0, 2, 1}, {0, 5, 1, 2},
        {8, 0, 1, 1}, {0, 6, 1, 1}, {1, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 0},
        {1, 0, std::numeric_limits<int>::max(), 1}}) {
        Require(controller->Init("cache", "cookies"), "restart for malformed update bounds");
        Responses rejected;
        rejected.serializer = controller->CreateSerializer(&rejected);
        controller->CreateBrowser(rejected.serializer->GetResponseTarget(), false, "fixture-agent");
        Pump(*controller, rejected.ready, 1);
        {
            std::ofstream marker(directory / "invalid-update");
            for (int field : update) {
                marker << field << ' ';
            }
        }
        rejected.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
        Pump(*controller, rejected.closed, 1);
        Require(rejected.paints == 0, "malformed bounds rejected before pixel callback");
        controller->RemoveBrowser(rejected.serializer->GetResponseTarget());
        rejected.serializer = nullptr;
        controller->Shutdown();
    }
    controller.reset();
    std::ofstream(directory / "fixture-keychain-option").put('1');
    ControllerConfig fixtureConfig{arguments[0], directory.u8string()};
    constexpr std::size_t FixtureIdentityLength = 64;
    fixtureConfig.fixtureKeychainId.assign(FixtureIdentityLength, 'a');
    controller = CreateController(fixtureConfig);
    Require(controller->Init("cache", "cookies"), "isolated encrypted fixture launch");
    controller->Shutdown();
    controller.reset();
    std::filesystem::remove(directory / "fixture-keychain-option");

    auto diagnostics = std::make_shared<ControllerDiagnostics>();
    controller = CreateController({arguments[0], directory.u8string(), diagnostics});
    Require(controller->Init(nullptr, nullptr), "owned engine session initialization");
    Responses immediate;
    Responses deferred;
    immediate.controller = controller.get();
    deferred.controller = controller.get();
    immediate.removeOnClose = true;
    immediate.serializer = controller->CreateSerializer(&immediate);
    deferred.serializer = controller->CreateSerializer(&deferred);
    controller->CreateBrowser(immediate.serializer->GetResponseTarget(), false, "fixture-agent");
    controller->CreateBrowser(deferred.serializer->GetResponseTarget(), false, "fixture-agent");
    Pump(*controller, immediate.ready, 1);
    Pump(*controller, deferred.ready, 1);
    deferred.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    BeginControllerShutdown(*controller);
    Require(immediate.closed == 1 && immediate.serializer == nullptr && deferred.closed == 1,
        "owned shutdown closes callbacks with reentrant removal");
    immediate.alive = false;
    Require(controller->CreateSerializer(&immediate) == nullptr && !controller->Init(nullptr, nullptr),
        "new widgets cannot reopen a draining engine session");
    constexpr int DeferredFrames = 8;
    for (int frame = 0; frame < DeferredFrames; ++frame) {
        controller->RunFrame();
        Require(deferred.serializer->GetBrowserHandle() > UINT32_MAX,
            "serializer remains alive over deferred widget deletion frames");
        deferred.serializer->KeyChar('A');
        SDL_Delay(1);
    }
    Require(deferred.titles == 0 && deferred.closed == 1, "detached widgets receive no queued callbacks");
    BeginControllerShutdown(*controller);
    Require(deferred.closed == 1, "repeated begin shutdown is idempotent");
    controller->RemoveBrowser(deferred.serializer->GetResponseTarget());
    deferred.serializer = nullptr;
    deferred.alive = false;
    controller->Shutdown();
    controller->Shutdown();
    Require(diagnostics->cleanShutdown && diagnostics->exitCode == 0,
        "graceful engine drain is not a provider failure");

    Require(controller->Init(nullptr, nullptr), "engine session restart after completed drain");
    Responses drainFailure;
    drainFailure.controller = controller.get();
    drainFailure.serializer = controller->CreateSerializer(&drainFailure);
    controller->CreateBrowser(drainFailure.serializer->GetResponseTarget(), false, "fixture-agent");
    Pump(*controller, drainFailure.ready, 1);
    const auto failureMarker = directory / "crash-after-create";
    std::ofstream(failureMarker).put('1');
    drainFailure.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    BeginControllerShutdown(*controller);
    constexpr Uint64 FailureDeadlineMilliseconds = 5000;
    const Uint64 failureDeadline = SDL_GetTicks() + FailureDeadlineMilliseconds;
    while (std::filesystem::exists(failureMarker) && SDL_GetTicks() < failureDeadline) {
        controller->RunFrame();
        SDL_Delay(1);
    }
    Require(!std::filesystem::exists(failureMarker), "provider failed during the deferred engine drain");
    for (int frame = 0; frame < DeferredFrames; ++frame) {
        controller->RunFrame();
        SDL_Delay(1);
    }
    Require(drainFailure.closed == 1 && drainFailure.serializer->GetBrowserHandle() > UINT32_MAX,
        "provider failure during drain neither renotifies nor frees the widget serializer");
    controller->RemoveBrowser(drainFailure.serializer->GetResponseTarget());
    drainFailure.serializer = nullptr;
    drainFailure.alive = false;
    controller->Shutdown();
    controller->Shutdown();
    Require(!diagnostics->cleanShutdown, "actual provider failure remains a failed shutdown");

    Require(controller->Init(nullptr, nullptr), "restart for real close-event drain");
    Responses closedByProvider;
    closedByProvider.controller = controller.get();
    closedByProvider.serializer = controller->CreateSerializer(&closedByProvider);
    controller->CreateBrowser(closedByProvider.serializer->GetResponseTarget(), false, "fixture-agent");
    Pump(*controller, closedByProvider.ready, 1);
    std::ofstream(directory / "close-after-post").put('1');
    closedByProvider.serializer->PostURL("https://fixture.invalid/", "field=\xd0\xaf");
    Pump(*controller, closedByProvider.closed, 1);
    BeginControllerShutdown(*controller);
    for (int frame = 0; frame < DeferredFrames; ++frame) {
        controller->RunFrame();
    }
    Require(closedByProvider.closed == 1, "provider and engine close notifications share one detached lifetime");
    controller->RemoveBrowser(closedByProvider.serializer->GetResponseTarget());
    closedByProvider.serializer = nullptr;
    closedByProvider.alive = false;
    controller->Shutdown();
    Require(diagnostics->cleanShutdown, "normal provider close does not become a startup or shutdown failure");
    controller.reset();
    SDL_Quit();
    std::puts("native_browser_controller_fixture_passed=1");
    return 0;
}

#if defined(_WIN32)
int wmain(int count, wchar_t* values[]) {
    std::vector<std::string> arguments;
    for (int index = 0; index < count; ++index) {
        const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[index], -1,
            nullptr, 0, nullptr, nullptr);
        Require(required > 0, "UTF-16 arguments");
        std::string value(static_cast<std::size_t>(required), '\0');
        Require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[index], -1,
            value.data(), required, nullptr, nullptr) == required, "UTF-8 argument conversion");
        value.pop_back();
        arguments.push_back(std::move(value));
    }
    return Run(arguments);
}
#else
int main(int count, char* values[]) {
    return Run(std::vector<std::string>(values, values + count));
}
#endif
