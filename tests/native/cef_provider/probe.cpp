#include "probe.h"

#include "include/base/cef_callback.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_cookie.h"
#include "include/cef_frame.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

static_assert(CEF_API_VERSION == 15600, "The provider fixture must compile the pinned stable CEF API");

namespace {
constexpr int ViewWidth = 192;
constexpr int ViewHeight = 96;
constexpr int ResizedWidth = 224;
constexpr int ResizedHeight = 128;
constexpr int PixelBytes = 4;
constexpr int MouseButtonX = 120;
constexpr int MouseButtonY = 20;
constexpr int DeadlineMilliseconds = 30000;
constexpr char FixtureUrlPrefix[] = "http://127.0.0.1:";
constexpr char ExpectedInputTitle[] = "input:A\xd0\xaf\xf0\x9f\x98\x80";

class Probe;
Probe* currentProbe = nullptr;
int result = 1;

class FlushComplete final : public CefCompletionCallback {
public:
    explicit FlushComplete(CefRefPtr<Probe> probe) : probe_(probe) {}
    void OnComplete() override;

private:
    CefRefPtr<Probe> probe_;
    IMPLEMENT_REFCOUNTING(FlushComplete);
};

class Probe final : public CefClient, public CefRenderHandler, public CefDisplayHandler,
                    public CefLoadHandler, public CefLifeSpanHandler {
public:
    explicit Probe(std::string phase) : phase_(std::move(phase)) {
        currentProbe = this;
    }

    CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return this;
    }

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }

    CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rectangle) override {
        rectangle = CefRect(0, 0, width_, height_);
    }

    bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
        info.device_scale_factor = 1.0f;
        info.depth = 32;
        info.depth_per_component = 8;
        info.is_monochrome = false;
        info.rect = CefRect(0, 0, ResizedWidth, ResizedHeight);
        info.available_rect = info.rect;
        return true;
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        browser_ = browser;
        if (reinterpret_cast<std::uintptr_t>(browser.get()) <= UINT32_MAX) {
            Fail("browser pointer did not exercise native high addresses");
        }
    }

    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int status) override {
        CEF_REQUIRE_UI_THREAD();
        if (!frame->IsMain() || closing_) {
            return;
        }
        if (status != 200) {
            Fail("controlled HTTP page did not load");
            return;
        }
        frame->ExecuteJavaScript("bootstrap('" + phase_ + "')", frame->GetURL(), 0);
    }

    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, ErrorCode code, const CefString& text,
        const CefString&) override {
        if (code != ERR_ABORTED && !closing_) {
            std::fprintf(stderr, "cef_load_error=%d %s\n", code, text.ToString().c_str());
            Fail("controlled page load failed");
        }
    }

    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
        CEF_REQUIRE_UI_THREAD();
        if (closing_) {
            return;
        }
        const std::string value = title.ToString();
        lastTitle_ = value;
        if (value.starts_with("failure:")) {
            Fail(value.c_str());
        } else if (value == "fixture-ready" && !mouseSent_) {
            storage_ = true;
            StartMouse();
        } else if (value == "mouse-click" && !inputSent_) {
            mouse_ = true;
            browser->GetMainFrame()->ExecuteJavaScript("focusInput()", browser->GetMainFrame()->GetURL(), 0);
        } else if (value == "input-focused" && !inputSent_) {
            inputSent_ = true;
            // The legacy KeyChar contract carries UTF-16 units on Windows. Exercise the same units on both targets.
            constexpr char16_t Input[] = {u'A', u'\u042f', 0xd83d, 0xde00};
            for (const char16_t character : Input) {
                CefKeyEvent event;
                event.type = KEYEVENT_CHAR;
#if defined(_WIN32)
                // CEF's Windows OSR adapter consumes WM_CHAR's wParam through this field.
                event.windows_key_code = character;
#endif
                event.character = character;
                event.unmodified_character = character;
                browser->GetHost()->SendKeyEvent(event);
            }
        } else if (value == ExpectedInputTitle && !unicode_) {
            unicode_ = true;
            width_ = ResizedWidth;
            height_ = ResizedHeight;
            browser->GetHost()->WasResized();
            browser->GetMainFrame()->ExecuteJavaScript("document.title='script-evaluated'",
                browser->GetMainFrame()->GetURL(), 0);
        } else if (value == "script-evaluated") {
            javascript_ = true;
            CheckComplete();
        }
    }

    void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList&, const void* buffer,
        int width, int height) override {
        CEF_REQUIRE_UI_THREAD();
        if (type != PET_VIEW || width <= 0 || height <= 0 || closing_) {
            return;
        }
        const auto* pixel = static_cast<const unsigned char*>(buffer) +
            ((static_cast<std::size_t>(height - 1) * width) + width - 1) * PixelBytes;
        // CEF's software off-screen buffer is BGRA, independent of the future renderer's RGBA texture contract.
        constexpr unsigned char Blue = 51;
        constexpr unsigned char Green = 34;
        constexpr unsigned char Red = 17;
        constexpr unsigned char Opaque = 255;
        if (pixel[0] != Blue || pixel[1] != Green || pixel[2] != Red || pixel[3] != Opaque) {
            return;
        }
        if (width == ViewWidth && height == ViewHeight) {
            painted_ = true;
        }
        if (width == ResizedWidth && height == ResizedHeight) {
            resized_ = true;
        }
        StartMouse();
        CheckComplete();
    }

    void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        CEF_REQUIRE_UI_THREAD();
        browser_ = nullptr;
        closed_ = true;
        CefQuitMessageLoop();
    }

    void Fail(const char* reason) {
        CEF_REQUIRE_UI_THREAD();
        std::fprintf(stderr, "cef_probe_failure=%s\n", reason);
        std::fprintf(stderr, "cef_last_title_utf8_hex=");
        for (const unsigned char character : lastTitle_) {
            std::fprintf(stderr, "%02x", static_cast<unsigned int>(character));
        }
        std::fprintf(stderr, "\n");
        failed_ = true;
        Close();
    }

    void Deadline() {
        if (!closed_) {
            Fail("fixture deadline exceeded");
        }
    }

    void Flushed() {
        CEF_REQUIRE_UI_THREAD();
        flushed_ = true;
        Close();
    }

    void Close() {
        closing_ = true;
        if (browser_) {
            browser_->GetHost()->CloseBrowser(true);
        }
    }

    void Report() const {
        const bool passed = !failed_ && closed_ && flushed_ && painted_ && resized_ && storage_ && mouse_ &&
            unicode_ && javascript_;
        std::printf("cef_phase=%s painted=%d resized=%d storage=%d mouse=%d unicode=%d javascript=%d "
            "flushed=%d closed=%d\n", phase_.c_str(), painted_, resized_, storage_, mouse_, unicode_, javascript_,
            flushed_, closed_);
        std::printf("native_cef_provider_probe_passed=%d\n", passed);
        result = passed ? 0 : 1;
    }

private:
    void StartMouse() {
        if (!painted_ || !storage_ || mouseSent_ || closing_) {
            return;
        }
        mouseSent_ = true;
        CefMouseEvent event;
        event.x = MouseButtonX;
        event.y = MouseButtonY;
        browser_->GetHost()->SetFocus(true);
        browser_->GetHost()->SendMouseMoveEvent(event, false);
        browser_->GetHost()->SendMouseClickEvent(event, MBT_LEFT, false, 1);
        browser_->GetHost()->SendMouseClickEvent(event, MBT_LEFT, true, 1);
    }

    void CheckComplete() {
        if (painted_ && resized_ && storage_ && mouse_ && unicode_ && javascript_ && !flushing_) {
            flushing_ = true;
            CefCookieManager::GetGlobalManager(nullptr)->FlushStore(new FlushComplete(this));
        }
    }

    std::string phase_;
    std::string lastTitle_;
    CefRefPtr<CefBrowser> browser_;
    int width_ = ViewWidth;
    int height_ = ViewHeight;
    bool failed_ = false;
    bool closing_ = false;
    bool closed_ = false;
    bool painted_ = false;
    bool resized_ = false;
    bool storage_ = false;
    bool mouseSent_ = false;
    bool mouse_ = false;
    bool inputSent_ = false;
    bool unicode_ = false;
    bool javascript_ = false;
    bool flushing_ = false;
    bool flushed_ = false;
    IMPLEMENT_REFCOUNTING(Probe);
};

void FlushComplete::OnComplete() {
    CefPostTask(TID_UI, base::BindOnce(&Probe::Flushed, probe_));
}

class ProbeApp final : public CefApp, public CefBrowserProcessHandler {
public:
    ProbeApp(CefRefPtr<Probe> probe, std::string url) : probe_(probe), url_(std::move(url)) {}

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    void OnContextInitialized() override {
        CEF_REQUIRE_UI_THREAD();
        CefWindowInfo window;
        window.SetAsWindowless(kNullWindowHandle);
        CefBrowserSettings settings;
        settings.windowless_frame_rate = 30;
        if (!CefBrowserHost::CreateBrowser(window, probe_, url_, settings, nullptr, nullptr)) {
            probe_->Fail("browser creation failed");
            CefQuitMessageLoop();
            return;
        }
        CefPostDelayedTask(TID_UI, base::BindOnce(&Probe::Deadline, probe_), DeadlineMilliseconds);
    }

private:
    CefRefPtr<Probe> probe_;
    std::string url_;
    IMPLEMENT_REFCOUNTING(ProbeApp);
};
}

int RunCefProbe(const CefMainArgs& arguments, CefRefPtr<CefCommandLine> commandLine, void* sandbox) {
    const std::string url = commandLine->GetSwitchValue("cse-url").ToString();
    const std::string phase = commandLine->GetSwitchValue("cse-phase").ToString();
    const std::string storage = commandLine->GetSwitchValue("cse-storage").ToString();
    if (!url.starts_with(FixtureUrlPrefix) || (phase != "write" && phase != "read") ||
        !std::filesystem::path(storage).is_absolute()) {
        std::fprintf(stderr, "CEF fixture requires loopback URL, write/read phase and absolute owned storage path\n");
        return 2;
    }
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    CefString(&settings.root_cache_path) = storage;
    CefString(&settings.cache_path) = storage + "/profile";
    CefString(&settings.log_file) = storage + "/cef.log";
    CefString(&settings.user_agent_product) = "CSE-NativeFixture/1";
    CefRefPtr<Probe> probe = new Probe(phase);
    CefRefPtr<ProbeApp> app = new ProbeApp(probe, url);
    if (!CefInitialize(arguments, settings, app, sandbox)) {
        currentProbe = nullptr;
        return CefGetExitCode() == 0 ? 1 : CefGetExitCode();
    }
    CefRunMessageLoop();
    probe->Report();
    currentProbe = nullptr;
    app = nullptr;
    probe = nullptr;
    CefShutdown();
    return result;
}

void CloseCefProbe() {
    if (currentProbe) {
        currentProbe->Fail("application termination requested before fixture completion");
    }
}
