#include "browser_controller.h"
#include "../../../GoldSrc-SDK/engine/utlstring.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

using namespace cse::browser;

namespace {
constexpr int InitialWidth = 192;
constexpr int InitialHeight = 96;
constexpr int ResizedWidth = 224;
constexpr int ResizedHeight = 128;
constexpr Uint64 DeadlineMilliseconds = 30000;

class Responses final : public IHTMLResponses_HL1 {
public:
    IHTMLSerializer* serializer = nullptr;
    IHTMLChromeController* controller = nullptr;
    std::string url;
    std::string phase;
    bool failed = false;
    bool painted = false;
    bool resized = false;
    bool storage = false;
    bool mouse = false;
    bool unicode = false;
    bool script = false;
    bool dialogs = false;
    bool link = false;
    int horizontal = 0;
    int vertical = 0;
    int zoom = 0;
    int alerts = 0;
    int confirms = 0;

    void BrowserReady() override {
        serializer->BrowserSize(InitialWidth, InitialHeight);
        serializer->PostURL(url.c_str(), nullptr);
    }

    void BrowserNeedsPaint(int texture, int width, int height, const unsigned char* pixels, int, int, int, int,
        int, int, const unsigned char*) override {
        const auto* corner = pixels + (static_cast<std::size_t>(width) * height - 1) * 4;
        if (corner[0] == 51 && corner[1] == 34 && corner[2] == 17 && corner[3] == 255) {
            painted = painted || (width == InitialWidth && height == InitialHeight);
            resized = resized || (width == ResizedWidth && height == ResizedHeight);
        }

        serializer->NeedsPaintResponse(texture);
        StartMouse();
    }

    void BrowserStartRequest(const char* requested, const char*, const char*, bool) override {
        const std::string target(requested);
        const std::size_t slash = url.find('/', sizeof("http://") - 1);
        const std::string origin = url.substr(0, slash) + "/";
        const bool allowed = target == "about:blank" || target.find(origin) == 0;
        if (target.find(origin) == 0) {
            CUtlString cookies;
            controller->GetWebCookiesForURL(&cookies, target.c_str(), "127.0.0.1");
            if (phase == "read" && (!cookies.m_pchString ||
                std::string(cookies.m_pchString).find("cse=persisted") == std::string::npos)) {
                Fail("cookie RPC during navigation policy");
            }
        }
        serializer->StartRequestResponse(allowed);
    }

    void BrowserFinishedRequest(const char* loaded, const char*) override {
        if (url == loaded && !bootstrapped_) {
            bootstrapped_ = true;
            serializer->RunJavaScript(("bootstrap('" + phase + "')").c_str());
            serializer->RequestBrowserSizes();
        }

    }

    void BrowserSetHTMLTitle(const char* title) override {
        const std::string value(title);
        if (value.find("failure:") == 0) {
            Fail(value.c_str());
        } else if (value == "fixture-ready") {
            storage = true;
            StartMouse();
        } else if (value == "mouse-click" && !focused_) {
            mouse = true;
            focused_ = true;
            serializer->RunJavaScript("focusInput()");
        } else if (value == "input-focused" && inputStage_ == 0) {
            inputStage_ = 1;
            serializer->KeyChar('A');
            serializer->KeyChar(0x042f);
            if (phase == "write") {
                serializer->KeyChar(0x1f600);
            } else {
                serializer->KeyChar(0xd83d);
                serializer->KeyChar(0xde00);
            }

        } else if (value == "input:A\xd0\xaf\xf0\x9f\x98\x80" && inputStage_ == 1) {
            inputStage_ = 2;
            constexpr int VirtualLeft = 0x25;
            constexpr int VirtualBackspace = 0x08;
            serializer->KeyDown(VirtualLeft, 0);
            serializer->KeyUp(VirtualLeft, 0);
            serializer->KeyDown(VirtualBackspace, 0);
            serializer->KeyUp(VirtualBackspace, 0);
        } else if (value == "input:A\xf0\x9f\x98\x80" && inputStage_ == 2) {
            inputStage_ = 3;
            serializer->KeyChar(0x042f);
        } else if (value == "input:A\xd0\xaf\xf0\x9f\x98\x80" && inputStage_ == 3) {
            inputStage_ = 4;
            unicode = true;
            serializer->BrowserSize(ResizedWidth, ResizedHeight);
            serializer->RunJavaScript("document.title='script-evaluated'");
        } else if (value == "script-evaluated" && !script) {
            script = true;
            serializer->RunJavaScript("alert('fixture-alert');"
                "if(confirm('fixture-deny'))document.title='failure:confirm-deny';"
                "else if(!confirm('fixture-accept'))document.title='failure:confirm-accept';"
                "else document.title='dialogs-complete'");
        } else if (value == "dialogs-complete") {
            dialogs = alerts == 1 && confirms == 2;
            serializer->GetLinkAtPosition(20, 64);
            serializer->RequestBrowserSizes();
        }

    }

    void BrowserJSAlert(const char* text) override {
        if (std::string(text) != "fixture-alert") {
            Fail("unexpected alert");
        }

        ++alerts;
        serializer->JSDialogResponse(1);
    }

    void BrowserJSConfirm(const char* text) override {
        const std::string value(text);
        ++confirms;
        if (value != "fixture-deny" && value != "fixture-accept") {
            Fail("unexpected confirm");
        }

        serializer->JSDialogResponse(value == "fixture-accept" ? 1 : 0);
    }

    void BrowserLinkAtPositionResponse(const char* target, int x, int y) override {
        const std::string expected = url.substr(0, url.rfind('/')) + "/target";
        link = std::string(target) == expected && x == 20 && y == 64;
        if (!link) {
            Fail("link-at-position result");
        }

    }

    void BrowserHorizontalScrollBarSizeResponse(int, int, int, int, int, int, float) override {
        ++horizontal;
    }

    void BrowserVerticalScrollBarSizeResponse(int, int, int, int, int, int, float) override {
        ++vertical;
    }

    void BrowserGetZoomResponse(float value) override {
        if (!(value > 0)) {
            Fail("invalid zoom");
        }

        ++zoom;
    }

    void BrowserClose() override {
        Fail("provider closed before completion");
    }

    void BrowserURLChanged(const char*, const char*, bool) override {}
    void BrowserShowPopup() override {}
    void BrowserHidePopup() override {}
    void BrowserSizePopup(int, int, int, int) override {}
    void BrowserCanGoBackandForward(bool, bool) override {}
    void BrowserPopupHTMLWindow(const char*, int, int, int, int) override {}
    void BrowserLoadingResource() override {}
    void BrowserStatusText(const char*) override {}
    void BrowserSetCursor(int) override {}
    void BrowserFileLoadDialog() override {}
    void BrowserShowToolTip(const char*) override {}
    void BrowserUpdateToolTip(const char*) override {}
    void BrowserHideToolTip() override {}

    bool Complete() const {
        return !failed && painted && resized && storage && mouse && unicode && script && dialogs && link &&
            horizontal > 0 && vertical > 0 && zoom > 0;
    }

private:
    void Fail(const char* reason) {
        std::fprintf(stderr, "cef_controller_failure=%s\n", reason);
        failed = true;
    }

    void StartMouse() {
        if (painted && storage && !mouseSent_) {
            mouseSent_ = true;
            serializer->SetFocus(true);
            serializer->MouseMove(120, 20);
            serializer->MouseDown(0);
            serializer->MouseUp(0);
        }

    }
    bool bootstrapped_ = false;
    bool mouseSent_ = false;
    bool focused_ = false;
    int inputStage_ = 0;
};

std::string Option(const std::vector<std::string>& arguments, const char* prefix) {
    for (const auto& argument : arguments) {
        if (argument.find(prefix) == 0) {
            return argument.substr(std::char_traits<char>::length(prefix));
        }

    }
    return {};
}

int Run(const std::vector<std::string>& arguments) {
    const std::string host = Option(arguments, "--cse-host=");
    const std::string storage = Option(arguments, "--cse-storage=");
    Responses responses;
    responses.url = Option(arguments, "--cse-url=");
    responses.phase = Option(arguments, "--cse-phase=");
    if (host.empty() || storage.empty() || responses.url.find("http://127.0.0.1:") != 0 ||
        (responses.phase != "write" && responses.phase != "read") || !SDL_Init(0)) {
        return 2;
    }

    auto diagnostics = std::make_shared<ControllerDiagnostics>();
    auto controller = CreateController({host, storage, diagnostics, Option(arguments, "--cse-fixture-keychain=")});
    if (!controller->Init("cache", "cookies")) {
        std::fprintf(stderr, "CEF controller initialization failed\n");
        return 1;
    }

    responses.controller = controller.get();
    responses.serializer = controller->CreateSerializer(&responses);
    if (!responses.serializer || responses.serializer->GetBrowserHandle() <= UINT32_MAX) {
        return 1;
    }

    controller->CreateBrowser(responses.serializer->GetResponseTarget(), false, "CSE Native Fixture");
    const Uint64 deadline = SDL_GetTicks() + DeadlineMilliseconds;
    while (!responses.Complete() && !responses.failed && SDL_GetTicks() < deadline) {
        controller->RunFrame();
        SDL_Delay(1);
    }

    bool passed = responses.Complete();
    std::printf("cef_controller_phase=%s painted=%d resized=%d storage=%d mouse=%d unicode=%d javascript=%d "
        "dialogs=%d link=%d horizontal=%d vertical=%d zoom=%d\n", responses.phase.c_str(), responses.painted,
        responses.resized, responses.storage, responses.mouse, responses.unicode, responses.script, responses.dialogs,
        responses.link, responses.horizontal, responses.vertical, responses.zoom);
    controller->RemoveBrowser(responses.serializer->GetResponseTarget());
    responses.serializer = nullptr;
    controller->Shutdown();
    passed = passed && diagnostics->cleanShutdown && diagnostics->exitCode == 0;
    controller.reset();
    SDL_Quit();
    std::printf("native_cef_provider_probe_passed=%d\n", passed);
    return passed ? 0 : 1;
}
}

#if defined(_WIN32)
int wmain(int count, wchar_t* values[]) {
    std::vector<std::string> arguments;
    for (int index = 0; index < count; ++index) {
        const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[index], -1,
            nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            return 2;
        }

        std::string value(static_cast<std::size_t>(required), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[index], -1,
            value.data(), required, nullptr, nullptr) != required) {
            return 2;
        }

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
