#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "cef_host.h"
#include "browser_commands.h"
#include "browser_wire.h"
#include "../../../public/html_key_modifiers.h"

#include "include/base/cef_callback.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_cookie.h"
#include "include/cef_dialog_handler.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_frame.h"
#include "include/cef_jsdialog_handler.h"
#include "include/cef_parser.h"
#include "include/cef_process_message.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#if !defined(CEF_USE_BOOTSTRAP)
#error The owned Windows host requires the sandboxed CEF bootstrap
#endif
#elif !defined(CEF_USE_SANDBOX)
#error The owned macOS host requires the CEF process sandbox
#endif

static_assert(CEF_API_VERSION == cse::browser::ProviderApiVersion, "Compile the pinned stable CEF API");

namespace {
using namespace cse::browser;
constexpr int DefaultWidth = 1;
constexpr int DefaultHeight = 1;
constexpr int DefaultFrameRate = 60;
constexpr int MaximumFrameRate = 60;
constexpr int MaximumDimension = MaximumBrowserDimension;
constexpr std::size_t PixelBytes = PaintBytesPerPixel;
// Leave room for the legacy scrollbar consumer to add a viewport extent without signed overflow.
constexpr int MaximumGeometryExtent = std::numeric_limits<int>::max() - MaximumDimension;
constexpr std::size_t CommandPayloadLimit = 64 * 1024 * 1024;
constexpr std::size_t MaximumPendingBrowserCommands = 1024;
constexpr int MouseWheelScale = 40;
constexpr int SingleClickCount = 1;
constexpr int DoubleClickCount = 2;
constexpr int LegacyAlt = 1;
constexpr int LegacyControl = 2;
constexpr int LegacyShift = 4;
constexpr float ZoomPercent = 100.0f;
constexpr int ScrollBarThickness = 16;
constexpr int UserAgentOverrideMessage = 1;
constexpr std::size_t InputChunkBytes = 16384;
constexpr int MaximumUnicodeScalar = 0x10ffff;
constexpr int MaximumVirtualKey = 0xff;
constexpr int LeftWindowsVirtualKey = 0x5b;
constexpr int RightWindowsVirtualKey = 0x5c;
constexpr int DefaultPopupWidth = 800;
constexpr int DefaultPopupHeight = 600;
constexpr std::size_t FixtureKeychainIdentifierCharacters = 64;

class Host;
class Client;
Host* currentHost = nullptr;

std::string PostData(CefRefPtr<CefRequest> request) {
    std::string result;
    const auto post = request->GetPostData();
    if (post) {
        CefPostData::ElementVector elements;
        post->GetElements(elements);
        for (const auto& element : elements) {
            if (element->GetType() == PDE_TYPE_BYTES) {
                const std::size_t offset = result.size();
                result.resize(offset + element->GetBytesCount());
                element->GetBytes(result.size() - offset, result.data() + offset);
            }
        }
    }

    return result;
}

std::uint32_t Modifiers(int legacy) {
    std::uint32_t result = 0;
    if ((legacy & LegacyAlt) != 0) {
        result |= EVENTFLAG_ALT_DOWN;
    }

    if ((legacy & LegacyControl) != 0) {
        result |= EVENTFLAG_CONTROL_DOWN;
    }

    if ((legacy & LegacyShift) != 0) {
        result |= EVENTFLAG_SHIFT_DOWN;
    }

    if ((legacy & cse::html::KeypadModifier) != 0) {
        result |= EVENTFLAG_IS_KEY_PAD;
    }

    return result;
}

std::string EscapeHTML(const std::string& input) {
    std::string result;
    for (char character : input) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        default:
            result += character;
            break;
        }
    }

    return result;
}

bool GeometryInteger(const CefRefPtr<CefListValue>& list, std::size_t index, int minimum, int maximum,
    int& result) {
    const auto type = list->GetType(index);
    if (type != VTYPE_INT && type != VTYPE_DOUBLE) {
        return false;
    }
    const double number = type == VTYPE_INT ? static_cast<double>(list->GetInt(index)) : list->GetDouble(index);
    if (!std::isfinite(number) || std::trunc(number) != number || number < minimum || number > maximum) {
        return false;
    }
    result = static_cast<int>(number);
    return true;
}

bool ValidFixtureKeychainIdentifier(std::string_view value) {
    return value.size() == FixtureKeychainIdentifierCharacters &&
        std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        });
}

class CookieVisitor final : public CefCookieVisitor {
public:
    CookieVisitor(Host& host, std::uint32_t request, std::string filter) :
        host_(host), request_(request), filter_(std::move(filter)) {}
    ~CookieVisitor() override;
    bool Visit(const CefCookie& cookie, int, int, bool&) override {
        std::string domain = CefString(&cookie.domain).ToString();
        if (!domain.empty() && domain.front() == '.') {
            domain.erase(domain.begin());
        }
        std::transform(domain.begin(), domain.end(), domain.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        std::transform(filter_.begin(), filter_.end(), filter_.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        if (!filter_.empty() && filter_.front() == '.') {
            filter_.erase(filter_.begin());
        }
        const bool subdomain = filter_.size() > domain.size() &&
            filter_.compare(filter_.size() - domain.size(), domain.size(), domain) == 0 &&
            filter_[filter_.size() - domain.size() - 1] == '.';
        if (filter_.empty() || filter_ == domain || subdomain) {
            if (!value_.empty()) {
                value_ += "; ";
            }
            value_ += CefString(&cookie.name).ToString() + "=" + CefString(&cookie.value).ToString();
        }
        return true;
    }
private:
    Host& host_;
    std::uint32_t request_;
    std::string filter_;
    std::string value_;
    IMPLEMENT_REFCOUNTING(CookieVisitor);
};

class Host final : public CefApp, public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    void OnContextInitialized() override;
    bool Send(Kind kind, std::uint32_t operation, std::uint32_t browser, const Writer& payload,
        std::uint32_t request = 0) {
        if (!payload.Good() || outputBroken_.load()) {
            return false;
        }
        const auto header = EncodeHeader({kind, operation, browser, request,
            static_cast<std::uint32_t>(payload.Data().size())});
        std::lock_guard<std::mutex> lock(outputMutex_);
        if (std::fwrite(header.data(), 1, header.size(), stdout) != header.size() ||
            (!payload.Data().empty() && std::fwrite(payload.Data().data(), 1, payload.Data().size(), stdout) !=
                payload.Data().size()) || std::fflush(stdout) != 0) {
            outputBroken_ = true;
            SignalStop(true);
            return false;
        }
        return true;
    }

    void Event(std::uint32_t browser, EHTMLCommands operation, const Writer& payload = Writer()) {
        Send(operation == eHTMLCommands_NeedsPaint ? Kind::Pixels : Kind::Event, operation, browser, payload);
    }

    void Cookies(std::uint32_t request, const std::string& value) {
        Writer payload;
        payload.String(value);
        Send(Kind::Control, static_cast<std::uint32_t>(Control::Cookies), 0, payload, request);
    }

    void Fail(const char* reason) {
        std::fprintf(stderr, "cef_host_failure=%s\n", reason);
        failed_ = true;
        SignalStop(true);
    }

    void SignalStop(bool failure);
    void CloseAll();
    void Closed(std::uint32_t identifier);
    CefRefPtr<Client> OwnPopup(std::uint32_t parent, int opener, int popup, const std::string& agent,
        const std::string& url, const CefPopupFeatures& features);
    void PopupCreated(std::uint64_t identifier);
    void PopupClosed(std::uint64_t identifier);
    void PopupAborted(int opener, int popup);
    void ClosePopups(std::uint32_t parent);
    void MaybeQuit();
    void ReaderLoop();
    void Dispatch(Message message);
    void ControlMessage(const Message& message);
    void Ask(std::uint32_t identifier, Writer payload, std::function<void(bool)> continuation,
        std::uint64_t nativePopup = 0);
    void CancelPopupDecision(std::uint32_t identifier, std::uint64_t nativePopup);
    void Answer(std::uint32_t identifier, bool allow);
    void Cancel(std::uint32_t identifier, bool preserveResponse = false);
    void CancelAll();
    void Join() {
        stopReader_ = true;
        if (reader_.joinable()) {
            reader_.join();
        }
    }

    bool Failed() const {
        return failed_.load();
    }

    void Flushed() {
        flushed_ = true;
        MaybeQuit();
    }
private:
    struct Decision {
        Writer payload;
        std::function<void(bool)> continuation;
        std::uint64_t nativePopup = 0;
    };
    struct PopupEntry {
        CefRefPtr<Client> client;
        std::uint32_t parent;
        int opener;
        int popup;
        bool created = false;
    };
    std::map<std::uint32_t, CefRefPtr<Client>> clients_;
    std::set<std::uint32_t> retired_;
    std::map<std::uint64_t, PopupEntry> popups_;
    std::uint64_t nextPopup_ = 1;
    std::mutex outputMutex_;
    std::mutex decisionMutex_;
    std::map<std::uint32_t, std::deque<Decision>> decisions_;
    std::thread reader_;
    std::atomic<bool> stopReader_{false};
    std::atomic<bool> closing_{false};
    std::atomic<bool> failed_{false};
    std::atomic<bool> outputBroken_{false};
    int frameRate_ = DefaultFrameRate;
    std::uint64_t build_ = 0;
    bool flushing_ = false;
    bool flushed_ = false;
    IMPLEMENT_REFCOUNTING(Host);
};

class Client final : public CefClient, public CefRenderHandler, public CefDisplayHandler,
                     public CefLoadHandler, public CefLifeSpanHandler, public CefRequestHandler,
                     public CefResourceRequestHandler, public CefJSDialogHandler, public CefDialogHandler,
                     public CefDevToolsMessageObserver {
public:
    Client(Host& host, std::uint32_t identifier, bool popup, std::string agent, std::uint64_t nativePopup = 0,
        std::string popupUrl = {}, CefPopupFeatures popupFeatures = {}) :
        host_(host), identifier_(identifier), popupBrowser_(popup), agent_(std::move(agent)),
        nativePopup_(nativePopup), popupUrl_(std::move(popupUrl)), popupFeatures_(popupFeatures) {}

    bool Closing() const {
        return closing_;
    }

    CefWindowHandle CreateNativeParent() {
        nativeParent_ = CreateCefHostNativeParent();
        return nativeParent_ ? nativeParent_->Handle() : kNullWindowHandle;
    }
    CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return nativePopup_ == 0 ? this : nullptr;
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

    CefRefPtr<CefRequestHandler> GetRequestHandler() override {
        return this;
    }

    CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override {
        return nativePopup_ == 0 ? this : nullptr;
    }

    CefRefPtr<CefDialogHandler> GetDialogHandler() override {
        return nativePopup_ == 0 ? this : nullptr;
    }

    void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rectangle) override {
        rectangle = CefRect(0, 0, width_, height_);
    }

    bool GetScreenPoint(CefRefPtr<CefBrowser>, int x, int y, int& screenX, int& screenY) override {
        screenX = positionX_ + x;
        screenY = positionY_ + y;
        return true;
    }

    bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
        info.device_scale_factor = 1.0f;
        info.depth = 32;
        info.depth_per_component = 8;
        info.is_monochrome = false;
        info.rect = CefRect(positionX_, positionY_, width_, height_);
        info.available_rect = info.rect;
        return true;
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        browser_ = browser;
        if (nativePopup_ == 0 && (!nativeParent_ ||
            !IsCefHostNativeWindow(browser_->GetHost()->GetWindowHandle()))) {
            host_.Fail("CEF OSR browser has no owned native parent");
            Close();
            return;
        }
        if (nativePopup_ != 0) {
            if (!IsCefHostNativeWindow(browser_->GetHost()->GetWindowHandle())) {
                host_.Fail("CEF popup has no owned native window");
                Close();
                return;
            }
            host_.PopupCreated(nativePopup_);
            Writer payload;
            payload.String(popupUrl_);
            payload.I32(popupFeatures_.widthSet ? popupFeatures_.width : DefaultPopupWidth);
            payload.I32(popupFeatures_.heightSet ? popupFeatures_.height : DefaultPopupHeight);
            payload.I32(popupFeatures_.x);
            payload.I32(popupFeatures_.y);
            host_.Event(identifier_, eHTMLCommands_PopupHTMLWindow, payload);
        }
        browser_->GetHost()->SetFocus(!popupBrowser_);
        if (closing_) {
            // A removal can arrive before asynchronous creation finishes. Closing here
            // reenters OnBeforeClose before CEF returns from its creation callback.
            CefPostTask(TID_UI, base::BindOnce([](CefRefPtr<Client> client) {
                client->Close();
            }, CefRefPtr<Client>(this)));
        } else if (agent_.empty()) {
            agentReady_ = true;
            MaybeReady();
        } else {
            devtools_ = browser_->GetHost()->AddDevToolsMessageObserver(this);
            auto parameters = CefDictionaryValue::Create();
            parameters->SetString("userAgent", agent_);
            if (browser_->GetHost()->ExecuteDevToolsMethod(UserAgentOverrideMessage,
                "Emulation.setUserAgentOverride", parameters) <= 0) {
                host_.Fail("CEF rejected the native user-agent override");
            }
        }
    }

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser>, int identifier, bool success, const void*,
        std::size_t) override {
        if (identifier == UserAgentOverrideMessage && !closing_) {
            if (!success) {
                host_.Fail("CEF failed to apply the native user-agent override");
            } else {
                agentReady_ = true;
                MaybeReady();
            }
        }
    }

    void OnRenderProcessTerminated(CefRefPtr<CefBrowser>, TerminationStatus, int,
        const CefString&) override {
        if (!closing_) {
            host_.Fail("CEF renderer process terminated");
        }
    }

    void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        if (nativePopup_ == 0) {
            host_.Cancel(identifier_);
            host_.ClosePopups(identifier_);
        } else {
            host_.CancelPopupDecision(identifier_, nativePopup_);
        }
        CancelDialog(false);
        devtools_ = nullptr;
        browser_ = nullptr;
        if (nativeParent_) {
            // CEF still finishes browser destruction after this callback returns. Release the native
            // owner on the next UI task, rather than on whichever thread drops the final CefClient ref.
            CefPostTask(TID_UI, base::BindOnce([](std::unique_ptr<CefHostNativeParent> parent) {
                parent.reset();
            }, std::move(nativeParent_)));
        }
        Emit(eHTMLCommands_Close);
        if (nativePopup_ != 0) {
            host_.PopupClosed(nativePopup_);
        } else {
            host_.Closed(identifier_);
        }
    }

    bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>, int popup,
        const CefString& url, const CefString&, CefLifeSpanHandler::WindowOpenDisposition, bool,
        const CefPopupFeatures& features,
        CefWindowInfo& window, CefRefPtr<CefClient>& client, CefBrowserSettings&,
        CefRefPtr<CefDictionaryValue>&, bool*) override {
        if (closing_) {
            return true;
        }
        client = host_.OwnPopup(identifier_, browser->GetIdentifier(), popup, agent_, url.ToString(), features);
#if defined(_WIN32)
        window.SetAsPopup(nullptr, "CounterStrikeExtended");
#endif
        window.windowless_rendering_enabled = false;
        window.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
        window.bounds = CefRect(features.x, features.y,
            features.widthSet ? std::max(1, features.width) : DefaultPopupWidth,
            features.heightSet ? std::max(1, features.height) : DefaultPopupHeight);
        // CEF owns the real native window/opener relationship. Its navigation is gated asynchronously
        // through the parent's policy queue; child painting, titles and closure never reuse parent events.
        return false;
    }

    void OnBeforePopupAborted(CefRefPtr<CefBrowser> browser, int popup) override {
        host_.PopupAborted(browser->GetIdentifier(), popup);
    }

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(CefRefPtr<CefBrowser>,
        CefRefPtr<CefFrame>, CefRefPtr<CefRequest>, bool, bool, const CefString&, bool&) override {
        return this;
    }

    ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override {
        {
            std::lock_guard<std::mutex> lock(headersMutex_);
            if (request->GetResourceType() == RT_MAIN_FRAME &&
                request->GetURL().ToString() == pendingPostUrl_ && !pendingPost_.empty()) {
                auto element = CefPostDataElement::Create();
                element->SetToBytes(pendingPost_.size(), pendingPost_.data());
                auto post = CefPostData::Create();
                post->AddElement(element);
                request->SetMethod("POST");
                request->SetPostData(post);
                request->SetHeaderByName("Content-Type", "application/x-www-form-urlencoded", true);
                pendingPostUrl_.clear();
                pendingPost_.clear();
            }
            for (const auto& header : headers_) {
                request->SetHeaderByName(header.first, header.second, true);
            }
        }
        if (!agent_.empty()) {
            request->SetHeaderByName("User-Agent", agent_, true);
        }
        Emit(eHTMLCommands_LoadingResource);
        if (request->GetResourceType() != RT_MAIN_FRAME && request->GetResourceType() != RT_SUB_FRAME) {
            return RV_CONTINUE;
        }
        Writer payload;
        payload.String(request->GetURL().ToString());
        payload.String(nativePopup_ != 0 ? "_blank" :
            frame && !frame->IsMain() ? frame->GetName().ToString() : "");
        const std::string post = PostData(request);
        payload.String(post);
        {
            std::lock_guard<std::mutex> lock(navigationMutex_);
            const auto redirected = redirectedRequests_.find(request->GetIdentifier());
            const bool isRedirect = redirected != redirectedRequests_.end();
            payload.Boolean(isRedirect);
            if (isRedirect) {
                redirectedRequests_.erase(redirected);
            }
            if (frame && frame->IsMain()) {
                redirected_ = isRedirect;
                navigationPost_ = post;
            }
        }
        host_.Ask(identifier_, std::move(payload), [callback](bool allow) {
            if (allow) {
                callback->Continue();
            } else {
                callback->Cancel();
            }
        }, nativePopup_);
        return RV_CONTINUE_ASYNC;
    }

    void OnResourceRedirect(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
        CefRefPtr<CefRequest> request, CefRefPtr<CefResponse>, CefString&) override {
        if (request->GetResourceType() == RT_MAIN_FRAME || request->GetResourceType() == RT_SUB_FRAME) {
            // CEF restarts the same request after redirects, then calls OnBeforeResourceLoad again.
            // Gate that new request asynchronously; never block CEF's IO thread or rewrite redirect URLs.
            std::lock_guard<std::mutex> lock(navigationMutex_);
            redirectedRequests_[request->GetIdentifier()] = true;
        }
    }

    void OnAddressChange(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, const CefString& url) override {
        if (frame->IsMain()) {
            Writer payload;
            payload.String(url.ToString());
            {
                std::lock_guard<std::mutex> lock(navigationMutex_);
                payload.String(navigationPost_);
                payload.Boolean(redirected_);
                redirected_ = false;
            }
            Emit(eHTMLCommands_URLChanged, payload);
        }
    }

    void OnLoadingStateChange(CefRefPtr<CefBrowser>, bool loading, bool back, bool forward) override {
        Writer payload;
        payload.Boolean(back);
        payload.Boolean(forward);
        Emit(eHTMLCommands_CanGoBackandForward, payload);
        if (!loading && browser_) {
            Query(0, 0, 0);
        }
    }

    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int) override {
        if (frame->IsMain()) {
            if (frame->GetURL() == "about:blank") {
                initialLoaded_ = true;
                MaybeReady();
            }
            Writer payload;
            payload.String(frame->GetURL().ToString());
            payload.String(title_);
            Emit(eHTMLCommands_FinishedRequest, payload);
            Query(0, 0, 0);
        }
    }

    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code,
        const CefString& text, const CefString& url) override {
        if (code == ERR_ABORTED || !frame->IsMain() || closing_) {
            return;
        }
        std::fprintf(stderr, "cef_host_load_error=%d %s\n", code, text.ToString().c_str());
        const std::string heading = errors_[0].empty() ? "Unable to load page" : errors_[0];
        enum ErrorText { Title, Connection, Retry, Unknown, Cache, BadUrl, Detail };
        std::string reason = errors_[Unknown];
        switch (code) {
        case ERR_INVALID_URL:
        case ERR_UNKNOWN_URL_SCHEME:
            reason = errors_[BadUrl];
            break;
        case ERR_CACHE_MISS:
            reason = errors_[Cache];
            break;
        case ERR_CONNECTION_CLOSED:
        case ERR_CONNECTION_RESET:
        case ERR_CONNECTION_REFUSED:
        case ERR_CONNECTION_ABORTED:
        case ERR_CONNECTION_FAILED:
        case ERR_TIMED_OUT:
        case ERR_NAME_NOT_RESOLVED:
        case ERR_INTERNET_DISCONNECTED:
            reason = errors_[Connection];
            break;
        default:
            break;
        }
        const std::string detail = errors_[Detail].empty() ? text.ToString() :
            errors_[Detail] + ": " + text.ToString();
        const std::string retry = errors_[Retry].empty() ? "Retry" : errors_[Retry];
        const std::string html = "<!doctype html><meta charset=utf-8><title>" + EscapeHTML(heading) +
            "</title><h1>" + EscapeHTML(heading) + "</h1><p>" + EscapeHTML(reason) + "</p><p>" +
            EscapeHTML(detail) + "</p><p>" + EscapeHTML(url.ToString()) + "</p><a href=\"" +
            EscapeHTML(url.ToString()) + "\">" + EscapeHTML(retry) + "</a>";
        frame->LoadURL("data:text/html;charset=utf-8;base64," + CefBase64Encode(html.data(), html.size()).ToString());
    }

    void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) override {
        title_ = title.ToString();
        if (nativePopup_ != 0 && browser_) {
            SetCefHostNativeTitle(browser_->GetHost()->GetWindowHandle(), title);
        }
        TextEvent(eHTMLCommands_SetHTMLTitle, title_);
    }

    void OnStatusMessage(CefRefPtr<CefBrowser>, const CefString& value) override {
        TextEvent(eHTMLCommands_StatusText, value.ToString());
    }

    bool OnTooltip(CefRefPtr<CefBrowser>, CefString& text) override {
        const auto value = text.ToString();
        if (value.empty()) {
            Emit(eHTMLCommands_HideToolTip);
        } else {
            TextEvent(tooltipVisible_ ? eHTMLCommands_UpdateToolTip : eHTMLCommands_ShowToolTip, value);
        }
        tooltipVisible_ = !value.empty();
        return true;
    }

    bool OnCursorChange(CefRefPtr<CefBrowser>, CefCursorHandle, cef_cursor_type_t type,
        const CefCursorInfo&) override {
        enum VguiCursor { None = 1, Arrow, Ibeam, Wait, WaitArrow, Cross, Up, NWSE, NESW, WE, NS, All, No, Hand };
        int cursor = Arrow;
        switch (type) {
        case CT_NONE:
            cursor = None;
            break;
        case CT_IBEAM:
            cursor = Ibeam;
            break;
        case CT_WAIT:
            cursor = Wait;
            break;
        case CT_PROGRESS:
            cursor = WaitArrow;
            break;
        case CT_CROSS:
            cursor = Cross;
            break;
        case CT_NORTHWESTSOUTHEASTRESIZE:
            cursor = NWSE;
            break;
        case CT_NORTHEASTSOUTHWESTRESIZE:
            cursor = NESW;
            break;
        case CT_EASTWESTRESIZE:
            cursor = WE;
            break;
        case CT_NORTHSOUTHRESIZE:
            cursor = NS;
            break;
        case CT_MOVE:
            cursor = All;
            break;
        case CT_NOTALLOWED: case CT_NODROP:
            cursor = No;
            break;
        case CT_HAND:
            cursor = Hand;
            break;
        default:
            break;
        }
        Writer payload;
        payload.I32(cursor);
        Emit(eHTMLCommands_SetCursor, payload);
        return true;
    }

    void OnPopupShow(CefRefPtr<CefBrowser>, bool show) override {
        popupVisible_ = show;
        if (!show) {
            popupPixels_.clear();
            popupWidth_ = 0;
            popupHeight_ = 0;
        }
        Emit(show ? eHTMLCommands_ShowPopup : eHTMLCommands_HidePopup);
    }

    void OnPopupSize(CefRefPtr<CefBrowser>, const CefRect& rectangle) override {
        Writer payload;
        payload.I32(rectangle.x);
        payload.I32(rectangle.y);
        payload.I32(rectangle.width);
        payload.I32(rectangle.height);
        Emit(eHTMLCommands_SizePopup, payload);
    }

    void OnScrollOffsetChanged(CefRefPtr<CefBrowser>, double, double) override {
        Query(0, 0, 0);
    }

    void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList&, const void* pixels,
        int width, int height) override {
        if (width <= 0 || height <= 0 || width > MaximumDimension || height > MaximumDimension || closing_) {
            return;
        }
        const auto* begin = static_cast<const std::uint8_t*>(pixels);
        const auto count = static_cast<std::size_t>(width) * height * PixelBytes;
        if (type == PET_VIEW) {
            paintWidth_ = width;
            paintHeight_ = height;
            mainPixels_.assign(begin, begin + count);
        } else {
            popupWidth_ = width;
            popupHeight_ = height;
            popupPixels_.assign(begin, begin + count);
        }
        Paint();
    }

    bool OnJSDialog(CefRefPtr<CefBrowser>, const CefString&, JSDialogType type, const CefString& text,
        const CefString&, CefRefPtr<CefJSDialogCallback> callback, bool&) override {
        if (type == JSDIALOGTYPE_PROMPT) {
            // Text input is owned by the native CEF dialog because the wire response carries only a boolean.
            return false;
        }
        if (dialog_) {
            host_.Fail("Overlapping JavaScript dialogs");
            callback->Continue(false, "");
            return true;
        }
        dialog_ = callback;
        TextEvent(type == JSDIALOGTYPE_ALERT ? eHTMLCommands_JSAlert : eHTMLCommands_JSConfirm, text.ToString());
        return true;
    }

    bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser>, const CefString& text, bool,
        CefRefPtr<CefJSDialogCallback> callback) override {
        if (dialog_) {
            callback->Continue(false, "");
            return true;
        }
        dialog_ = callback;
        TextEvent(eHTMLCommands_JSConfirm, text.ToString());
        return true;
    }

    void OnResetDialogState(CefRefPtr<CefBrowser>) override {
        CancelDialog(true);
    }

    bool OnFileDialog(CefRefPtr<CefBrowser>, FileDialogMode, const CefString&, const CefString&,
        const std::vector<CefString>&, const std::vector<CefString>&, const std::vector<CefString>&,
        CefRefPtr<CefFileDialogCallback>) override {
        Emit(eHTMLCommands_FileLoadDialog);
        return false;
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId source,
        CefRefPtr<CefProcessMessage> message) override {
        if (source != PID_RENDERER || message->GetName() != "cse.dom.reply") {
            return false;
        }
        const auto arguments = message->GetArgumentList();
        const auto value = CefParseJSON(arguments->GetString(3), JSON_PARSER_RFC);
        if (!value) {
            std::fprintf(stderr, "cef_host_ignored_dom_reply=invalid_json\n");
            return true;
        }
        Writer payload;
        if (arguments->GetInt(0) == 1 && value->GetType() == VTYPE_STRING) {
            payload.String(value->GetString().ToString());
            payload.I32(arguments->GetInt(1));
            payload.I32(arguments->GetInt(2));
            Emit(eHTMLCommands_LinkAtPositionResponse, payload);
        } else if (arguments->GetInt(0) == 0 && value->GetType() == VTYPE_LIST) {
            const auto list = value->GetList();
            constexpr std::size_t GeometryValues = 6;
            if (list->GetSize() != GeometryValues) {
                std::fprintf(stderr, "cef_host_ignored_dom_reply=geometry_size\n");
                return true;
            }
            std::array<int, GeometryValues> geometry{};
            if (!GeometryInteger(list, 0, 1, MaximumDimension, geometry[0]) ||
                !GeometryInteger(list, 1, 1, MaximumDimension, geometry[1]) ||
                !GeometryInteger(list, 2, 0, MaximumGeometryExtent, geometry[2]) ||
                !GeometryInteger(list, 3, 0, MaximumGeometryExtent, geometry[3]) ||
                !GeometryInteger(list, 4, -MaximumGeometryExtent, MaximumGeometryExtent, geometry[4]) ||
                !GeometryInteger(list, 5, -MaximumGeometryExtent, MaximumGeometryExtent, geometry[5])) {
                // This is untrusted page/renderer metadata, not a command from the owning UI. Ignore it
                // before conversion or scrollbar arithmetic, keeping the browser usable for later queries.
                std::fprintf(stderr, "cef_host_ignored_dom_reply=geometry_bounds\n");
                return true;
            }
            Geometry(false, geometry[0], geometry[1], geometry[2], geometry[4]);
            Geometry(true, geometry[0], geometry[1], geometry[3], geometry[5]);
            std::uint32_t zoom = 0;
            std::memcpy(&zoom, &ZoomPercent, sizeof(zoom));
            payload.U32(zoom);
            Emit(eHTMLCommands_GetZoomResponse, payload);
        }
        return true;
    }

    void Command(const Message& message);
    void Close() {
        closing_ = true;
        pendingCommands_.clear();
        pendingCommandBytes_ = 0;
        if (nativePopup_ == 0) {
            host_.Cancel(identifier_);
            host_.ClosePopups(identifier_);
        }
        CancelDialog(false);
        if (browser_) {
            browser_->GetHost()->CloseBrowser(true);
        }
    }

    void SetRate(int rate) {
        if (browser_) {
            browser_->GetHost()->SetWindowlessFrameRate(rate);
        }
    }
private:
    void CancelDialog(bool retainResponse) {
        if (!dialog_) {
            return;
        }
        // Clear before Continue: CEF synchronously calls OnResetDialogState when the callback completes.
        CefRefPtr<CefJSDialogCallback> callback = dialog_;
        dialog_ = nullptr;
        if (retainResponse) {
            ++cancelledDialogReplies_;
        }
        callback->Continue(false, "");
    }

    void MaybeReady() {
        if (nativePopup_ == 0 && initialLoaded_ && agentReady_ && !readySent_ && !closing_) {
            readySent_ = true;
            Emit(eHTMLCommands_BrowserReady);
            // VGUI configures controls immediately after CreateBrowser. CEF creation,
            // the initial document and the user-agent override finish asynchronously.
            // Replay accepted commands only once all three are ready, in their original order.
            std::deque<Message> pending;
            pending.swap(pendingCommands_);
            pendingCommandBytes_ = 0;
            for (const auto& command : pending) {
                if (closing_) {
                    break;
                }
                Command(command);
            }
        }
    }

    void Emit(EHTMLCommands operation, const Writer& payload = Writer()) {
        if (nativePopup_ == 0) {
            host_.Event(identifier_, operation, payload);
        }
    }

    void TextEvent(EHTMLCommands operation, const std::string& value) {
        Writer payload;
        payload.String(value);
        Emit(operation, payload);
    }

    void Query(int operation, int x, int y) {
        if (!browser_ || closing_ || nativePopup_ != 0) {
            return;
        }
        auto message = CefProcessMessage::Create("cse.dom.query");
        auto values = message->GetArgumentList();
        values->SetInt(0, operation);
        values->SetInt(1, x);
        values->SetInt(2, y);
        browser_->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
    }

    void Geometry(bool vertical, int width, int height, int content, int scroll) {
        Writer payload;
        const int viewport = vertical ? height : width;
        const int thickness = content > viewport ? ScrollBarThickness : 0;
        payload.I32(vertical ? width - thickness : 0);
        payload.I32(vertical ? 0 : height - thickness);
        payload.I32(vertical ? thickness : width);
        payload.I32(vertical ? height : thickness);
        payload.I32(scroll);
        payload.I32(std::max(0, content - viewport));
        std::uint32_t zoom = 0;
        std::memcpy(&zoom, &ZoomPercent, sizeof(zoom));
        payload.U32(zoom);
        Emit(vertical ? eHTMLCommands_VerticalScrollBarSizeResponse :
            eHTMLCommands_HorizontalScrollBarSizeResponse, payload);
    }

    void Paint() {
        if (mainPixels_.empty() || painting_ || (popupVisible_ && popupPixels_.empty())) {
            paintPending_ = true;
            return;
        }
        painting_ = true;
        paintPending_ = false;
        Writer payload;
        payload.I32(texture_);
        payload.I32(paintWidth_);
        payload.I32(paintHeight_);
        payload.I32(popupWidth_);
        payload.I32(popupHeight_);
        payload.Bytes(mainPixels_.data(), mainPixels_.size());
        payload.Bytes(popupPixels_.data(), popupPixels_.size());
        Emit(eHTMLCommands_NeedsPaint, payload);
    }

    Host& host_;
    std::uint32_t identifier_;
    bool popupBrowser_;
    const std::string agent_;
    const std::uint64_t nativePopup_;
    const std::string popupUrl_;
    const CefPopupFeatures popupFeatures_;
    std::unique_ptr<CefHostNativeParent> nativeParent_;
    CefRefPtr<CefBrowser> browser_;
    CefRefPtr<CefJSDialogCallback> dialog_;
    std::size_t cancelledDialogReplies_ = 0;
    CefRefPtr<CefRegistration> devtools_;
    std::mutex headersMutex_;
    std::map<std::string, std::string> headers_;
    std::string pendingPostUrl_;
    std::string pendingPost_;
    std::mutex navigationMutex_;
    std::array<std::string, 7> errors_;
    std::string title_;
    std::map<std::uint64_t, bool> redirectedRequests_;
    std::string navigationPost_;
    std::vector<std::uint8_t> mainPixels_;
    std::vector<std::uint8_t> popupPixels_;
    int width_ = DefaultWidth;
    int height_ = DefaultHeight;
    int paintWidth_ = 0;
    int paintHeight_ = 0;
    int popupWidth_ = 0;
    int popupHeight_ = 0;
    int positionX_ = 0;
    int positionY_ = 0;
    int texture_ = 1;
    CefMouseEvent mouse_;
    std::uint32_t keyModifiers_ = 0;
    std::uint32_t mouseButtons_ = 0;
    std::uint32_t commandKeys_ = 0;
    bool redirected_ = false;
    bool closing_ = false;
    bool initialLoaded_ = false;
    bool agentReady_ = false;
    bool readySent_ = false;
    bool tooltipVisible_ = false;
    bool popupVisible_ = false;
    bool painting_ = false;
    bool paintPending_ = false;
    std::deque<Message> pendingCommands_;
    std::size_t pendingCommandBytes_ = 0;
    IMPLEMENT_REFCOUNTING(Client);
};

void Client::Command(const Message& message) {
    Reader reader(message.payload);
    std::int32_t a = 0;
    std::int32_t b = 0;
    std::string text;
    std::string second;
    bool flag = false;
    if (message.header.operation == eHTMLCommands_BrowserRemove && reader.Finish()) {
        Close();
        return;
    }

    if (closing_) {
        host_.Fail("Command for a browser that is not ready");
        return;
    }

    if (nativePopup_ == 0 && !readySent_) {
        if (pendingCommands_.size() >= MaximumPendingBrowserCommands ||
            message.payload.size() > CommandPayloadLimit - pendingCommandBytes_) {
            host_.Fail("Pending browser commands exceed the bounded startup queue");
            return;
        }
        pendingCommands_.push_back(message);
        pendingCommandBytes_ += message.payload.size();
        return;
    }

    if (!browser_) {
        host_.Fail("Command for a browser that is not ready");
        return;
    }

    auto browserHost = browser_->GetHost();
    auto frame = browser_->GetMainFrame();
    switch (message.header.operation) {
    case eHTMLCommands_BrowserPosition:
        if (reader.I32(a) && reader.I32(b) && reader.Finish()) {
            positionX_ = a;
            positionY_ = b;
            browserHost->NotifyScreenInfoChanged();
            browserHost->NotifyMoveOrResizeStarted();
            return;
        }
        break;
    case eHTMLCommands_BrowserSize:
        if (reader.I32(a) && reader.I32(b) && reader.Finish() && a >= 0 && b >= 0 &&
            a <= MaximumDimension && b <= MaximumDimension) {
            width_ = std::max(DefaultWidth, a);
            height_ = std::max(DefaultHeight, b);
            browserHost->WasHidden(a == 0 || b == 0);
            browserHost->WasResized();
            Query(0, 0, 0);
            return;
        }
        break;
    case eHTMLCommands_BrowserErrorStrings:
        flag = true;
        for (auto& error : errors_) {
            flag = reader.String(error) && flag;
        }
        if (flag && reader.Finish()) {
            return;
        }
        break;
    case eHTMLCommands_PostURL:
        if (reader.String(text) && reader.String(second) && reader.Finish()) {
            CancelDialog(true);
            {
                std::lock_guard<std::mutex> lock(headersMutex_);
                pendingPostUrl_ = text;
                pendingPost_ = second;
            }
            // LoadRequest cannot cross an unvisited origin (CEF INVALID_INITIATOR_ORIGIN 213).
            // Apply POST bytes to the browser-initiated request in OnBeforeResourceLoad instead.
            frame->LoadURL(text);
            return;
        }
        break;
    case eHTMLCommands_ExecuteJavaScript:
        if (reader.String(text) && reader.Finish()) {
            frame->ExecuteJavaScript(text, frame->GetURL(), 0);
            return;
        }
        break;
    case eHTMLCommands_StopLoad:
    case eHTMLCommands_Reload:
    case eHTMLCommands_GoBack:
    case eHTMLCommands_GoForward:
    case eHTMLCommands_FullRepaint:
        if (reader.Finish()) {
            switch (message.header.operation) {
            case eHTMLCommands_StopLoad:
                host_.Cancel(identifier_, true);
                browser_->StopLoad();
                break;
            case eHTMLCommands_Reload:
                CancelDialog(true);
                browser_->Reload();
                break;
            case eHTMLCommands_GoBack:
                CancelDialog(true);
                browser_->GoBack();
                break;
            case eHTMLCommands_GoForward:
                CancelDialog(true);
                browser_->GoForward();
                break;
            default:
                browserHost->Invalidate(PET_VIEW);
                Query(0, 0, 0);
                break;
            }
            return;
        }
        break;
    case eHTMLCommands_MouseMove:
    case eHTMLCommands_LinkAtPosition:
        if (reader.I32(a) && reader.I32(b) && reader.Finish()) {
            if (message.header.operation == eHTMLCommands_LinkAtPosition) {
                Query(1, a, b);
            } else {
                mouse_.x = a;
                mouse_.y = b;
                mouse_.modifiers = keyModifiers_ | mouseButtons_;
                browserHost->SendMouseMoveEvent(mouse_, false);
            }
            return;
        }
        break;
    case eHTMLCommands_MouseDown:
    case eHTMLCommands_MouseUp:
    case eHTMLCommands_MouseDblClick:
        if (reader.I32(a) && reader.Finish() && a >= MBT_LEFT && a <= MBT_RIGHT) {
            const auto button = static_cast<CefBrowserHost::MouseButtonType>(a);
            const bool up = message.header.operation == eHTMLCommands_MouseUp;
            const int count = message.header.operation == eHTMLCommands_MouseDblClick ?
                DoubleClickCount : SingleClickCount;
            const std::uint32_t buttonFlag = button == MBT_LEFT ? EVENTFLAG_LEFT_MOUSE_BUTTON :
                button == MBT_MIDDLE ? EVENTFLAG_MIDDLE_MOUSE_BUTTON : EVENTFLAG_RIGHT_MOUSE_BUTTON;
            if (up) {
                mouseButtons_ &= ~buttonFlag;
            } else {
                mouseButtons_ |= buttonFlag;
            }
            mouse_.modifiers = keyModifiers_ | mouseButtons_;
            browserHost->SendMouseClickEvent(mouse_, button, up, count);
            return;
        }
        break;
    case eHTMLCommands_MouseWheel:
        if (reader.I32(a) && reader.Finish() && a >= INT32_MIN / MouseWheelScale &&
            a <= INT32_MAX / MouseWheelScale) {
            mouse_.modifiers = keyModifiers_ | mouseButtons_;
            browserHost->SendMouseWheelEvent(mouse_, 0, a * MouseWheelScale);
            return;
        }
        break;
    case eHTMLCommands_KeyDown:
    case eHTMLCommands_KeyUp:
        if (reader.I32(a) && reader.I32(b) && reader.Finish() && a > 0 && a <= MaximumVirtualKey &&
            (b & ~(LegacyAlt | LegacyControl | LegacyShift | cse::html::KeypadModifier)) == 0) {
            if (a == LeftWindowsVirtualKey || a == RightWindowsVirtualKey) {
                const std::uint32_t mask = a == LeftWindowsVirtualKey ? 1 : 2;
                if (message.header.operation == eHTMLCommands_KeyDown) {
                    commandKeys_ |= mask;
                } else {
                    commandKeys_ &= ~mask;
                }
            }
            keyModifiers_ = Modifiers(b);
#if defined(__APPLE__)
            if (commandKeys_ != 0) {
                keyModifiers_ |= EVENTFLAG_COMMAND_DOWN;
            }
#endif
            CefKeyEvent event;
            event.type = message.header.operation == eHTMLCommands_KeyDown ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
            event.windows_key_code = a;
            event.modifiers = keyModifiers_;
#if defined(__APPLE__)
            if (!PrepareCefHostMacKey(event)) {
                host_.Fail("Virtual key has no native macOS translation");
                return;
            }
#else
            event.is_system_key = (b & LegacyAlt) != 0;
            constexpr unsigned int ScanCodeMask = 0xff;
            constexpr unsigned int ExtendedScanCodePrefix = 0xe000;
            constexpr int ReturnVirtualKey = 0x0d;
            // Pinned CEF 156 passes this value directly to NativeKeycodeToDomCode, which expects
            // a compact scan code (for example 0xE01C), not WM_KEY lParam/repeat/transition bits.
            // The event type and modifiers already carry the release and Alt state.
            std::uint32_t native = MapVirtualKeyW(a, MAPVK_VK_TO_VSC_EX);
            if (a == ReturnVirtualKey && (b & cse::html::KeypadModifier) != 0) {
                native = (native & ScanCodeMask) | ExtendedScanCodePrefix;
            }
            event.native_key_code = static_cast<int>(native);
#endif
            browserHost->SendKeyEvent(event);
            if (message.header.operation == eHTMLCommands_KeyUp) {
                keyModifiers_ &= ~EVENTFLAG_IS_KEY_PAD;
            }
            return;
        }
        break;
    case eHTMLCommands_KeyChar:
        if (reader.I32(a) && reader.Finish() && a >= 0 && a <= MaximumUnicodeScalar) {
            const auto send = [&](char16_t character) {
                CefKeyEvent event;
                event.type = KEYEVENT_CHAR;
                event.windows_key_code = character;
                event.character = character;
                event.unmodified_character = character;
                event.modifiers = keyModifiers_;
                browserHost->SendKeyEvent(event);
            };
            constexpr int MaximumUtf16Unit = 0xffff;
            constexpr int SupplementaryBase = 0x10000;
            constexpr int HighSurrogateBase = 0xd800;
            constexpr int LowSurrogateBase = 0xdc00;
            constexpr int SurrogateBits = 10;
            constexpr int SurrogateMask = 0x3ff;
            if (a <= MaximumUtf16Unit) {
                send(static_cast<char16_t>(a));
            } else {
                const int scalar = a - SupplementaryBase;
                send(static_cast<char16_t>(HighSurrogateBase + (scalar >> SurrogateBits)));
                send(static_cast<char16_t>(LowSurrogateBase + (scalar & SurrogateMask)));
            }
            return;
        }
        break;
    case eHTMLCommands_SetFocus:
        if (reader.Boolean(flag) && reader.Finish()) {
            browserHost->SetFocus(flag);
            return;
        }
        break;
    case eHTMLCommands_SetHorizontalScroll:
    case eHTMLCommands_SetVerticalScroll:
        if (reader.I32(a) && reader.Finish()) {
            text = message.header.operation == eHTMLCommands_SetHorizontalScroll ?
                "window.scrollTo(" + std::to_string(a) + ",window.scrollY)" :
                "window.scrollTo(window.scrollX," + std::to_string(a) + ")";
            frame->ExecuteJavaScript(text, frame->GetURL(), 0);
            return;
        }
        break;
    case eHTMLCommands_AddHeader:
        if (reader.String(text) && reader.String(second) && reader.Finish()) {
            std::lock_guard<std::mutex> lock(headersMutex_);
            headers_[text] = second;
            return;
        }
        break;
    case eHTMLCommands_NeedsPaintResponse:
        if (reader.I32(a) && reader.Finish() && painting_ && a == texture_) {
            painting_ = false;
            if (paintPending_) {
                Paint();
            }
            return;
        }
        break;
    case eHTMLCommands_JSDialogResponse:
        if (reader.I32(a) && reader.Finish() && (dialog_ || cancelledDialogReplies_ != 0)) {
            if (cancelledDialogReplies_ != 0) {
                --cancelledDialogReplies_;
                return;
            }
            CefRefPtr<CefJSDialogCallback> callback = dialog_;
            dialog_ = nullptr;
            callback->Continue(a != 0, "");
            return;
        }
        break;
    default:
        break;
    }

    host_.Fail("Malformed or unsupported browser command");
}

CookieVisitor::~CookieVisitor() {
    host_.Cookies(request_, value_);
}

class SetCookieCallback final : public CefSetCookieCallback {
public:
    explicit SetCookieCallback(Host& host) : host_(host) {}

    void OnComplete(bool success) override {
        if (!success) {
            host_.Fail("CEF did not store the supplied cookie");
        }
    }

private:
    Host& host_;
    IMPLEMENT_REFCOUNTING(SetCookieCallback);
};

class FlushCallback final : public CefCompletionCallback {
public:
    explicit FlushCallback(Host& host) : host_(host) {}
    void OnComplete() override {
        host_.Flushed();
    }
private:
    Host& host_;
    IMPLEMENT_REFCOUNTING(FlushCallback);
};

void Host::OnContextInitialized() {
    Writer payload;
    payload.U32(ProviderContractVersion);
    payload.U32(ProviderApiVersion);
    Send(Kind::Control, static_cast<std::uint32_t>(Control::Hello), 0, payload);
    reader_ = std::thread(&Host::ReaderLoop, this);
}

void Host::Ask(std::uint32_t identifier, Writer payload, std::function<void(bool)> continuation,
    std::uint64_t nativePopup) {
    bool first = false;
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(decisionMutex_);
        if (!closing_ && !stopReader_) {
            accepted = true;
            auto& queue = decisions_[identifier];
            first = queue.empty();
            queue.push_back({payload, std::move(continuation), nativePopup});
        }
    }

    if (!accepted) {
        if (continuation) {
            continuation(false);
        }
    } else if (first) {
        Event(identifier, eHTMLCommands_StartRequest, payload);
    }
}

void Host::Answer(std::uint32_t identifier, bool allow) {
    std::function<void(bool)> continuation;
    Writer next;
    bool hasNext = false;
    {
        std::lock_guard<std::mutex> lock(decisionMutex_);
        const auto found = decisions_.find(identifier);
        if (found == decisions_.end() || found->second.empty()) {
            // A browser removed while its callback was in flight may return one final answer.
            return;
        }
        continuation = std::move(found->second.front().continuation);
        found->second.pop_front();
        if (!found->second.empty()) {
            next = found->second.front().payload;
            hasNext = true;
        } else {
            decisions_.erase(found);
        }
    }

    continuation(allow);
    if (hasNext) {
        Event(identifier, eHTMLCommands_StartRequest, next);
    }
}

void Host::Cancel(std::uint32_t identifier, bool preserveResponse) {
    std::deque<Decision> queue;
    {
        std::lock_guard<std::mutex> lock(decisionMutex_);
        const auto found = decisions_.find(identifier);
        if (found != decisions_.end()) {
            queue.swap(found->second);
            if (preserveResponse && !queue.empty()) {
                // The fixed legacy response carries no correlation ID. Consume its in-flight answer before
                // presenting a later decision, so an answer to a stopped request cannot approve its replacement.
                found->second.push_back({queue.front().payload, [](bool) {}});
            } else {
                decisions_.erase(found);
            }
        }
    }

    for (auto& decision : queue) {
        decision.continuation(false);
    }
}

void Host::CancelPopupDecision(std::uint32_t identifier, std::uint64_t nativePopup) {
    std::vector<std::function<void(bool)>> callbacks;
    {
        std::lock_guard<std::mutex> lock(decisionMutex_);
        const auto found = decisions_.find(identifier);
        if (found == decisions_.end()) {
            return;
        }
        auto& queue = found->second;
        for (auto iterator = queue.begin(); iterator != queue.end();) {
            if (iterator->nativePopup != nativePopup) {
                ++iterator;
                continue;
            }
            callbacks.push_back(std::move(iterator->continuation));
            if (iterator == queue.begin()) {
                iterator->continuation = [](bool) {};
                iterator->nativePopup = 0;
                ++iterator;
            } else {
                iterator = queue.erase(iterator);
            }
        }
    }
    for (auto& callback : callbacks) {
        callback(false);
    }
}

void Host::CancelAll() {
    std::map<std::uint32_t, std::deque<Decision>> decisions;
    {
        std::lock_guard<std::mutex> lock(decisionMutex_);
        decisions.swap(decisions_);
    }

    for (auto& entry : decisions) {
        for (auto& decision : entry.second) {
            decision.continuation(false);
        }
    }
}

void Host::SignalStop(bool failure) {
    if (failure) {
        failed_ = true;
    }

    stopReader_ = true;
    CancelAll();
    CefPostTask(TID_UI, base::BindOnce(&Host::CloseAll, CefRefPtr<Host>(this)));
}

void Host::CloseAll() {
    closing_ = true;
    stopReader_ = true;
    CancelAll();
    // Keep the map stable if CEF immediately delivers a close callback.
    std::vector<CefRefPtr<Client>> clients;
    for (const auto& entry : clients_) {
        clients.push_back(entry.second);
    }

    for (const auto& client : clients) {
        client->Close();
    }

    MaybeQuit();
}

void Host::Closed(std::uint32_t identifier) {
    retired_.insert(identifier);
    clients_.erase(identifier);
    MaybeQuit();
}

CefRefPtr<Client> Host::OwnPopup(std::uint32_t parent, int opener, int popup, const std::string& agent,
    const std::string& url, const CefPopupFeatures& features) {
    const std::uint64_t identifier = nextPopup_++;
    CefRefPtr<Client> client = new Client(*this, parent, false, agent, identifier, url, features);
    popups_.emplace(identifier, PopupEntry{client, parent, opener, popup});
    return client;
}

void Host::PopupCreated(std::uint64_t identifier) {
    const auto found = popups_.find(identifier);
    if (found != popups_.end()) {
        found->second.created = true;
        std::fprintf(stderr, "cef_host_native_popup_created=%llu native_window=1\n", static_cast<unsigned long long>(identifier));
        const auto parent = clients_.find(found->second.parent);
        if (closing_ || parent == clients_.end() || parent->second->Closing()) {
            found->second.client->Close();
        }
    }
}

void Host::PopupClosed(std::uint64_t identifier) {
    popups_.erase(identifier);
    std::fprintf(stderr, "cef_host_native_popup_closed=%llu\n", static_cast<unsigned long long>(identifier));
    MaybeQuit();
}

void Host::PopupAborted(int opener, int popup) {
    for (auto iterator = popups_.begin(); iterator != popups_.end(); ++iterator) {
        if (iterator->second.opener == opener && iterator->second.popup == popup && !iterator->second.created) {
            popups_.erase(iterator);
            break;
        }
    }
    MaybeQuit();
}

void Host::ClosePopups(std::uint32_t parent) {
    std::vector<CefRefPtr<Client>> clients;
    for (const auto& entry : popups_) {
        if (entry.second.parent == parent) {
            clients.push_back(entry.second.client);
        }
    }
    for (const auto& client : clients) {
        client->Close();
    }
}

void Host::MaybeQuit() {
    if (!closing_ || !clients_.empty() || !popups_.empty()) {
        return;
    }

    if (!flushing_) {
        flushing_ = true;
        if (!CefCookieManager::GetGlobalManager(nullptr)->FlushStore(new FlushCallback(*this))) {
            failed_ = true;
            flushed_ = true;
        }
    }

    if (flushed_) {
        CefQuitMessageLoop();
    }
}

void Host::ReaderLoop() {
    Decoder decoder(CommandPayloadLimit);
    std::array<std::uint8_t, InputChunkBytes> bytes{};
    while (!stopReader_) {
        int count = 0;
#if defined(_WIN32)
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD available = 0;
        if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) {
            SignalStop(GetLastError() != ERROR_BROKEN_PIPE || !decoder.Finish());
            return;
        }
        if (available == 0) {
            constexpr auto InputPollInterval = std::chrono::milliseconds(1);
            std::this_thread::sleep_for(InputPollInterval);
            continue;
        }
        DWORD received = 0;
        if (!ReadFile(input, bytes.data(), static_cast<DWORD>(std::min<std::size_t>(available, bytes.size())),
            &received, nullptr)) {
            SignalStop(true);
            return;
        }
        count = static_cast<int>(received);
#else
        pollfd input{STDIN_FILENO, POLLIN, 0};
        constexpr int InputPollMilliseconds = 10;
        const int ready = poll(&input, 1, InputPollMilliseconds);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            SignalStop(true);
            return;
        }
        if (ready == 0) {
            continue;
        }
        count = static_cast<int>(read(STDIN_FILENO, bytes.data(), bytes.size()));
        if (count < 0 && errno == EINTR) {
            continue;
        }
#endif
        if (count <= 0) {
            SignalStop(count < 0 || !decoder.Finish());
            return;
        }
        std::size_t offset = 0;
        while (offset < static_cast<std::size_t>(count)) {
            Message message;
            std::size_t used = 0;
            const auto status = decoder.Feed(bytes.data() + offset, count - offset, used, message);
            offset += used;
            if (status == DecodeStatus::Invalid) {
                Fail("Invalid input frame");
                return;
            }
            if (status == DecodeStatus::Complete) {
                // All frames share the UI queue so an allow response cannot overtake an earlier
                // StopLoad or BrowserRemove. Navigation policy continuations never block the UI thread.
                CefPostTask(TID_UI, base::BindOnce(&Host::Dispatch, CefRefPtr<Host>(this),
                    std::move(message)));
            }
        }
    }
}

void Host::Dispatch(Message message) {
    if (closing_) {
        return;
    }

    if (message.header.kind == Kind::Control && message.header.browser == 0) {
        ControlMessage(message);
        return;
    }

    if (message.header.kind != Kind::Command || message.header.browser == 0 || message.header.request != 0) {
        Fail("Invalid command envelope");
        return;
    }

    if (message.header.operation == eHTMLCommands_StartRequestResponse) {
        Reader reader(message.payload);
        bool allow = false;
        if (!reader.Boolean(allow) || !reader.Finish()) {
            Fail("Invalid navigation decision");
            return;
        }
        Answer(message.header.browser, allow);
        return;
    }

    const auto found = clients_.find(message.header.browser);
    const bool staleResponse = message.header.operation == eHTMLCommands_NeedsPaintResponse ||
        message.header.operation == eHTMLCommands_JSDialogResponse;
    if (staleResponse && (retired_.find(message.header.browser) != retired_.end() ||
        (found != clients_.end() && found->second->Closing()))) {
        Reader reader(message.payload);
        std::int32_t result = 0;
        if (!reader.I32(result) || !reader.Finish()) {
            Fail("Malformed response for a retired browser");
        }
        // The owner may acknowledge a frame/dialog already in the output pipe when removal starts.
        // Keep this retirement tombstone separate from live IDs; new mutations remain invalid.
        return;
    }
    if (message.header.operation == eHTMLCommands_BrowserCreate) {
        Reader reader(message.payload);
        bool popup = false;
        std::string agent;
        if (found != clients_.end() || retired_.find(message.header.browser) != retired_.end() ||
            !reader.Boolean(popup) || !reader.String(agent) || !reader.Finish()) {
            Fail("Invalid browser creation");
            return;
        }
        CefRefPtr<Client> client = new Client(*this, message.header.browser, popup, agent);
        const CefWindowHandle parent = client->CreateNativeParent();
        if (parent == kNullWindowHandle || !IsCefHostNativeWindow(parent)) {
            Fail("Could not create the owned OSR native parent");
            return;
        }
        clients_[message.header.browser] = client;
        CefWindowInfo window;
        window.SetAsWindowless(parent);
        CefBrowserSettings settings;
        settings.windowless_frame_rate = frameRate_;
        settings.background_color = CefColorSetARGB(255, 255, 255, 255);
        auto extra = CefDictionaryValue::Create();
        extra->SetString("user_agent", agent);
        if (!CefBrowserHost::CreateBrowser(window, client, "about:blank", settings, extra, nullptr)) {
            clients_.erase(message.header.browser);
            Fail("CEF rejected browser creation");
        }
        return;
    }

    if (found == clients_.end()) {
        if (message.header.operation != eHTMLCommands_BrowserRemove) {
            Fail("Unknown browser ID");
        }
        return;
    }

    CefRefPtr<Client> client = found->second;
    client->Command(message);
}

void Host::ControlMessage(const Message& message) {
    Reader reader(message.payload);
    std::string url;
    std::string name;
    std::string value;
    std::string path;
    std::int32_t expiry = 0;
    std::uint32_t rate = 0;
    const auto operation = static_cast<Control>(message.header.operation);
    if (operation != Control::GetCookies && message.header.request != 0) {
        Fail("Unexpected control request ID");
        return;
    }

    switch (operation) {
    case Control::SetCookie:
        if (reader.String(url) && reader.String(name) && reader.String(value) && reader.String(path) &&
            reader.I32(expiry) && reader.Finish()) {
            CefCookie cookie;
            if (url.find("://") == std::string::npos) {
                if (!url.empty() && url.front() == '.') {
                    CefString(&cookie.domain) = url;
                    url.erase(url.begin());
                }
                url = "https://" + url;
            }
            CefString(&cookie.name) = name;
            CefString(&cookie.value) = value;
            CefString(&cookie.path) = path.empty() ? "/" : path;
            cookie.has_expires = expiry > 0;
            if (cookie.has_expires) {
                // Legacy expiry is an absolute Unix timestamp, in seconds.
                constexpr std::int64_t WindowsEpochOffsetSeconds = 11644473600LL;
                constexpr std::int64_t MicrosecondsPerSecond = 1000000;
                cookie.expires.val = (static_cast<std::int64_t>(expiry) + WindowsEpochOffsetSeconds) *
                    MicrosecondsPerSecond;
            }
            if (!CefCookieManager::GetGlobalManager(nullptr)->SetCookie(url, cookie, new SetCookieCallback(*this))) {
                Fail("CEF rejected cookie attributes");
            }
            return;
        }
        break;
    case Control::GetCookies:
        if (message.header.request != 0 && reader.String(url) && reader.String(name) && reader.Finish()) {
            if (!CefCookieManager::GetGlobalManager(nullptr)->VisitUrlCookies(url, true,
                new CookieVisitor(*this, message.header.request, name))) {
                Fail("CEF cookie store is unavailable");
            }
            return;
        }
        break;
    case Control::SetBuild:
        if (reader.U64(build_) && reader.Finish()) {
            return;
        }
        break;
    case Control::SetFrameRate:
        if (reader.U32(rate) && reader.Finish() && rate > 0 && rate <= MaximumFrameRate) {
            frameRate_ = static_cast<int>(rate);
            for (const auto& client : clients_) {
                client.second->SetRate(frameRate_);
            }
            return;
        }
        break;
    default:
        break;
    }

    Fail("Malformed or unsupported control command");
}
}

int RunCefHost(const CefMainArgs& arguments, CefRefPtr<CefCommandLine> commandLine, void* sandbox) {
    const std::string storage = commandLine->GetSwitchValue("cse-storage").ToString();
    const bool fixtureKeychain = commandLine->HasSwitch("cse-fixture-keychain");
    const std::string fixtureIdentifier = commandLine->GetSwitchValue("cse-fixture-keychain").ToString();
    const std::filesystem::path storagePath(std::u8string(storage.begin(), storage.end()));
    if (!commandLine->HasSwitch("cse-browser-host") || !storagePath.is_absolute() ||
        commandLine->HasSwitch("no-sandbox")) {
        std::fprintf(stderr, "CEF host requires --cse-browser-host and an absolute --cse-storage path\n");
        return 2;
    }
    if (fixtureKeychain && !ValidFixtureKeychainIdentifier(fixtureIdentifier)) {
        std::fprintf(stderr, "CEF fixture Keychain identifier must contain exactly 64 lowercase hex characters\n");
        return 2;
    }
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    std::error_code error;
    std::filesystem::create_directories(storagePath, error);
    if (error) {
        return 2;
    }

    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = false;
    settings.persist_session_cookies = true;
    CefString(&settings.root_cache_path) = storage;
    CefString(&settings.cache_path) = storage + "/profile";
    CefString(&settings.log_file) = storage + "/cef.log";
    CefString(&settings.user_agent_product) = "CounterStrikeExtended/1";
#if defined(__APPLE__)
    // This provider owns its encryption identity. Never request another Chromium application's key.
    if (fixtureKeychain) {
        // Build/test-scoped identities prevent unsigned development rebuilds from touching production keys.
        // CEF still creates a real encrypted Keychain item; the fixture owns its exact paired deletion.
        CefString(&settings.keychain_service_name) = "CounterStrikeExtended Browser Fixture " + fixtureIdentifier;
        CefString(&settings.keychain_account_name) = "CounterStrikeExtended Fixture";
        std::fprintf(stderr, "cef_host_fixture_keychain=%s\n", fixtureIdentifier.c_str());
    } else {
#if defined(NDEBUG)
        CefString(&settings.keychain_service_name) = "CounterStrikeExtended Browser Safe Storage";
#else
        CefString(&settings.keychain_service_name) = "CounterStrikeExtended Browser Debug Safe Storage";
#endif
        CefString(&settings.keychain_account_name) = "CounterStrikeExtended";
    }
#endif
    CefRefPtr<Host> host = new Host;
    currentHost = host.get();
    if (!CefInitialize(arguments, settings, host, sandbox)) {
        currentHost = nullptr;
        return CefGetExitCode() == 0 ? 1 : CefGetExitCode();
    }

    CefRunMessageLoop();
    host->Join();
    const bool failed = host->Failed();
    currentHost = nullptr;
    CefShutdown();
    host = nullptr;
    return failed ? 1 : 0;
}

void CloseCefHost() {
    if (currentHost) {
        currentHost->SignalStop(false);
    }
}
