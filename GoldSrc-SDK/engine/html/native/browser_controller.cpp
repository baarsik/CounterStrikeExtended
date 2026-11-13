#include "browser_controller.h"
#include "browser_paint.h"
#include "browser_commands.h"
#include "process_channel.h"
#include "../../utlstring.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <utility>

struct HTMLCommandBuffer_t {
    cse::browser::Header header{};
    cse::browser::Writer payload;
};

namespace cse::browser {
namespace {
constexpr Uint64 ProviderStartupMilliseconds = 30000;
constexpr std::size_t IncomingPayloadLimit = MaximumPaintPayloadBytes;
constexpr int MaximumWindowsVirtualKey = 0xff;

std::string Text(const char* value) {
    return value ? value : "";
}

class Controller;
class Serializer final : public IHTMLSerializer {
public:
    Serializer(Controller& owner, std::uint32_t identifier, IHTMLResponses_HL1* responses) :
        owner_(owner), identifier_(identifier), responses_(responses) {}

    IHTMLResponses* GetResponseTarget() override {
        return reinterpret_cast<IHTMLResponses*>(this);
    }

    int BrowserGetIndex() override {
        return ready_ ? static_cast<int>(identifier_) : -1;
    }

    std::uint32_t Identifier() const {
        return identifier_;
    }

    void MarkReady() {
        ready_ = true;
    }

    void BrowserPosition(int x, int y) override {
        Writer payload;
        payload.I32(x);
        payload.I32(y);
        Send(eHTMLCommands_BrowserPosition, payload);
    }

    void PostURL(const char* url, const char* data) override {
        Writer payload;
        payload.String(Text(url));
        payload.String(Text(data));
        Send(eHTMLCommands_PostURL, payload);
    }

    void BrowserErrorStrings(const char* title, const char* connection, const char* retry, const char* unknown,
        const char* cache, const char* badUrl, const char* detail) override {
        Writer payload;
        payload.String(Text(title));
        payload.String(Text(connection));
        payload.String(Text(retry));
        payload.String(Text(unknown));
        payload.String(Text(cache));
        payload.String(Text(badUrl));
        payload.String(Text(detail));
        Send(eHTMLCommands_BrowserErrorStrings, payload);
    }

    void StopLoad() override {
        Writer payload;
        Send(eHTMLCommands_StopLoad, payload);
    }

    void Reload() override {
        Writer payload;
        Send(eHTMLCommands_Reload, payload);
    }

    void GoBack() override {
        Writer payload;
        Send(eHTMLCommands_GoBack, payload);
    }

    void GoForward() override {
        Writer payload;
        Send(eHTMLCommands_GoForward, payload);
    }

    void MouseDown(int code) override {
        Writer payload;
        payload.I32(code);
        Send(eHTMLCommands_MouseDown, payload);
    }

    void MouseUp(int code) override {
        Writer payload;
        payload.I32(code);
        Send(eHTMLCommands_MouseUp, payload);
    }

    void MouseWheel(int delta) override {
        Writer payload;
        payload.I32(delta);
        Send(eHTMLCommands_MouseWheel, payload);
    }

    void MouseMove(int x, int y) override {
        Writer payload;
        payload.I32(x);
        payload.I32(y);
        Send(eHTMLCommands_MouseMove, payload);
    }

    void MouseDoubleClick(int code) override {
        Writer payload;
        payload.I32(code);
        Send(eHTMLCommands_MouseDblClick, payload);
    }

    void BrowserSize(int width, int height) override {
        Writer payload;
        payload.I32(width);
        payload.I32(height);
        Send(eHTMLCommands_BrowserSize, payload);
    }

    void KeyUp(int key, int modifiers) override {
        if (key <= 0 || key > MaximumWindowsVirtualKey) {
            return;
        }
        Writer payload;
        payload.I32(key);
        payload.I32(modifiers);
        Send(eHTMLCommands_KeyUp, payload);
    }

    void KeyDown(int key, int modifiers) override {
        if (key <= 0 || key > MaximumWindowsVirtualKey) {
            return;
        }
        Writer payload;
        payload.I32(key);
        payload.I32(modifiers);
        Send(eHTMLCommands_KeyDown, payload);
    }

    void KeyChar(int character) override {
        Writer payload;
        payload.I32(character);
        Send(eHTMLCommands_KeyChar, payload);
    }

    void RunJavaScript(const char* script) override {
        Writer payload;
        payload.String(Text(script));
        Send(eHTMLCommands_ExecuteJavaScript, payload);
    }

    void SetHorizontalScroll(int value) override {
        Writer payload;
        payload.I32(value);
        Send(eHTMLCommands_SetHorizontalScroll, payload);
    }

    void SetVerticalScroll(int value) override {
        Writer payload;
        payload.I32(value);
        Send(eHTMLCommands_SetVerticalScroll, payload);
    }

    void SetFocus(bool state) override {
        Writer payload;
        payload.Boolean(state);
        Send(eHTMLCommands_SetFocus, payload);
    }

    void AddHeader(const char* name, const char* value) override {
        Writer payload;
        payload.String(Text(name));
        payload.String(Text(value));
        Send(eHTMLCommands_AddHeader, payload);
    }

    void NeedsPaintResponse(int texture) override {
        Writer payload;
        payload.I32(texture);
        Send(eHTMLCommands_NeedsPaintResponse, payload);
    }

    void StartRequestResponse(bool allow) override {
        Writer payload;
        payload.Boolean(allow);
        Send(eHTMLCommands_StartRequestResponse, payload);
    }

    void RequestBrowserSizes() override {
        Writer payload;
        Send(eHTMLCommands_FullRepaint, payload);
    }

    void JSDialogResponse(int result) override {
        Writer payload;
        payload.I32(result);
        Send(eHTMLCommands_JSDialogResponse, payload);
    }

    void GetLinkAtPosition(int x, int y) override {
        Writer payload;
        payload.I32(x);
        payload.I32(y);
        Send(eHTMLCommands_LinkAtPosition, payload);
    }

    void Detach() {
        responses_ = nullptr;
        ready_ = false;
    }

    IHTMLResponses_HL1* Responses() const {
        return responses_;
    }

    IHTMLResponses_HL1* CloseResponses() {
        IHTMLResponses_HL1* responses = responses_;
        Detach();
        return responses;
    }

private:
    void Send(EHTMLCommands operation, const Writer& payload);
    Controller& owner_;
    std::uint32_t identifier_;
    IHTMLResponses_HL1* responses_;
    bool ready_ = false;
};

class Controller final : public IHTMLChromeController {
public:
    explicit Controller(ControllerConfig config) : config_(std::move(config)), channel_(IncomingPayloadLimit) {}
    ~Controller() override {
        Shutdown();
    }

    bool Init(const char*, const char*) override {
        if (initialized_) {
            return !failed_ && !preparingShutdown_;
        }

        if (config_.diagnostics) {
            *config_.diagnostics = {};
        }

        failed_ = false;
        failureNotified_ = false;
        preparingShutdown_ = false;
        hello_ = false;
        pending_.clear();
        cookieReplies_.clear();
        activeCookieRequest_ = 0;
        std::vector<std::string> arguments{config_.executable, "--cse-browser-host",
            "--cse-storage=" + config_.profileDirectory};
        if (!config_.fixtureKeychainId.empty()) {
            arguments.push_back("--cse-fixture-keychain=" + config_.fixtureKeychainId);
        }
        if (config_.executable.empty() || config_.profileDirectory.empty() ||
            !channel_.Start(arguments)) {
            return false;
        }

        const Uint64 deadline = SDL_GetTicks() + ProviderStartupMilliseconds;
        while (!hello_ && !failed_ && SDL_GetTicks() < deadline) {
            Collect();
            if (!hello_) {
                SDL_Delay(1);
            }

        }

        initialized_ = hello_ && !failed_;
        if (!initialized_) {
            Shutdown();
        }

        return initialized_;
    }

    void Shutdown() override {
        for (auto& browser : browsers_) {
            browser.second->Detach();
        }

        browsers_.clear();
        pending_.clear();
        const bool hadProcess = channel_.Started();
        int exitCode = 0;
        const bool finished = channel_.Finish(exitCode);
        if (!finished || exitCode != 0) {
            failed_ = true;
        }

        if (config_.diagnostics && hadProcess) {
            config_.diagnostics->cleanShutdown = finished && exitCode == 0 && !failed_;
            config_.diagnostics->exitCode = exitCode;
        }

        initialized_ = false;
        hello_ = false;
    }

    bool Quiescent() const {
        return !channel_.Started() && browsers_.empty() && pending_.empty();
    }

    void BeginShutdown() {
        if (preparingShutdown_) {
            return;
        }
        preparingShutdown_ = true;
        std::vector<std::shared_ptr<Serializer>> snapshot;
        for (const auto& browser : browsers_) {
            snapshot.push_back(browser.second);
        }
        for (const auto& browser : snapshot) {
            // Detach first: BrowserClose may remove another widget or pump a nested VGUI frame.
            if (auto* responses = browser->CloseResponses()) {
                responses->BrowserClose();
            }
        }
    }

    bool RunFrame() override {
        Collect();
        if (failed_) {
            StopFailedProvider();
            return false;
        }

        std::vector<Message> messages;
        messages.swap(pending_);
        for (const auto& message : messages) {
            Dispatch(message);
            if (failed_) {
                StopFailedProvider();
                return false;
            }

        }

        return !messages.empty();
    }

    bool BHasPendingMessages() override {
        Collect();
        return !pending_.empty() || (failed_ && !failureNotified_);
    }

    void SetWebCookie(const char* host, const char* name, const char* value, const char* path, int expires) override {
        Writer payload;
        payload.String(Text(host));
        payload.String(Text(name));
        payload.String(Text(value));
        payload.String(Text(path));
        payload.I32(expires);
        SendControl(Control::SetCookie, payload);
    }

    void GetWebCookiesForURL(CUtlString* cookies, const char* url, const char* host) override;

    void SetClientBuildID(std::uint64_t build) override {
        Writer payload;
        payload.U64(build);
        SendControl(Control::SetBuild, payload);
    }

    void CreateBrowser(IHTMLResponses* target, bool popup, const char* agent) override {
        if (preparingShutdown_) {
            return;
        }
        const auto browser = Find(target);
        if (!browser) {
            return;
        }

        Writer payload;
        payload.Boolean(popup);
        payload.String(Text(agent));
        Send(browser->Identifier(), eHTMLCommands_BrowserCreate, payload);
    }

    void RemoveBrowser(IHTMLResponses* target) override {
        const auto browser = Find(target);
        if (!browser) {
            return;
        }

        const auto identifier = browser->Identifier();
        browser->Detach();
        browsers_.erase(identifier);
        Writer payload;
        Send(identifier, eHTMLCommands_BrowserRemove, payload);
    }

    void WakeThread() override {
        Collect();
    }

    HTMLCommandBuffer_t* GetFreeCommandBuffer(EHTMLCommands operation, int browser) override {
        if (!initialized_ || failed_ || browser < 0 || operation < 0 || operation >= eHTMLCommands_None) {
            return nullptr;
        }

        auto* result = new HTMLCommandBuffer_t;
        result->header = {Kind::Command, static_cast<std::uint32_t>(operation),
            static_cast<std::uint32_t>(browser), 0, 0};
        return result;
    }

    void PushCommand(HTMLCommandBuffer_t* command) override {
        std::unique_ptr<HTMLCommandBuffer_t> owner(command);
        if (owner && owner->payload.Good()) {
            channel_.Queue(owner->header, owner->payload.Data());
        }

    }

    void SetCefThreadTargetFrameRate(std::uint32_t rate) override {
        Writer payload;
        payload.U32(rate);
        SendControl(Control::SetFrameRate, payload);
    }

    IHTMLSerializer* CreateSerializer(IHTMLResponses_HL1* responses) override {
        constexpr auto MaxBrowserIndex = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
        if (!initialized_ || failed_ || preparingShutdown_ || !responses || nextIdentifier_ >= MaxBrowserIndex) {
            return nullptr;
        }

        const auto identifier = nextIdentifier_++;
        auto browser = std::make_shared<Serializer>(*this, identifier, responses);
        browsers_.emplace(identifier, browser);
        return browser.get();
    }

    void Send(std::uint32_t browser, EHTMLCommands operation, const Writer& payload) {
        if (failed_ || !initialized_) {
            return;
        }

        if (!payload.Good() || !channel_.Queue({Kind::Command, static_cast<std::uint32_t>(operation), browser, 0, 0},
            payload.Data())) {
            failed_ = true;
        }

    }

private:
    std::shared_ptr<Serializer> Find(IHTMLResponses* target) const {
        for (const auto& browser : browsers_) {
            if (browser.second->GetResponseTarget() == target) {
                return browser.second;
            }

        }

        return {};
    }

    void SendControl(Control operation, const Writer& payload) {
        if (failed_ || !initialized_) {
            return;
        }

        if (!payload.Good() || !channel_.Queue({Kind::Control, static_cast<std::uint32_t>(operation), 0, 0, 0},
            payload.Data())) {
            failed_ = true;
        }

    }

    void StopFailedProvider();
    void Collect();
    void Dispatch(const Message& message);

    ControllerConfig config_;
    ProcessChannel channel_;
    std::map<std::uint32_t, std::shared_ptr<Serializer>> browsers_;
    std::vector<Message> pending_;
    std::map<std::uint32_t, std::string> cookieReplies_;
    std::uint32_t nextIdentifier_ = 1;
    std::uint32_t nextRequest_ = 1;
    std::uint32_t activeCookieRequest_ = 0;
    bool hello_ = false;
    bool initialized_ = false;
    bool failed_ = false;
    bool failureNotified_ = false;
    bool preparingShutdown_ = false;
};

void Serializer::Send(EHTMLCommands operation, const Writer& payload) {
    owner_.Send(identifier_, operation, payload);
}

void Controller::StopFailedProvider() {
    if (failureNotified_) {
        return;
    }

    failureNotified_ = true;
    std::vector<std::shared_ptr<Serializer>> snapshot;
    for (const auto& browser : browsers_) {
        snapshot.push_back(browser.second);
    }

    for (const auto& browser : snapshot) {
        if (auto* responses = browser->CloseResponses()) {
            responses->BrowserClose();
        }
    }

    // Widget owners retain their controls after a close notification. Keep serializer handles valid until removal.
    pending_.clear();
    int exitCode = 0;
    channel_.Finish(exitCode);
}

void Controller::Collect() {
    if (!channel_.Started()) {
        return;
    }

    constexpr Uint64 FramePumpMilliseconds = 4;
    const Uint64 deadline = SDL_GetTicks() + FramePumpMilliseconds;
    for (;;) {
        std::vector<Message> messages;
        const auto status = channel_.Poll(messages);
        for (auto& message : messages) {
            if (message.header.kind == Kind::Control) {
                if (message.header.browser != 0) {
                    failed_ = true;
                    break;
                }

                Reader reader(message.payload);
                if (message.header.operation == static_cast<std::uint32_t>(Control::Hello)) {
                    std::uint32_t contract = 0;
                    std::uint32_t api = 0;
                    if (hello_ || message.header.request != 0 || !reader.U32(contract) || !reader.U32(api) || !reader.Finish() ||
                        contract != ProviderContractVersion || api != ProviderApiVersion) {
                        failed_ = true;
                    } else {
                        hello_ = true;
                    }

                } else if (message.header.operation == static_cast<std::uint32_t>(Control::Cookies)) {
                    std::string value;
                    if (activeCookieRequest_ == 0 || message.header.request != activeCookieRequest_ ||
                        cookieReplies_.find(message.header.request) != cookieReplies_.end() ||
                        !reader.String(value) || !reader.Finish()) {
                        failed_ = true;
                    } else {
                        cookieReplies_[message.header.request] = std::move(value);
                    }

                } else {
                    failed_ = true;
                }

            } else if (!hello_) {
                failed_ = true;
            } else {
                pending_.push_back(std::move(message));
            }

        }

        if (status == ChannelStatus::Failed || status == ChannelStatus::End) {
            failed_ = true;
        }

        if (status != ChannelStatus::Progress || failed_ || SDL_GetTicks() >= deadline) {
            break;
        }

    }
}

void Controller::GetWebCookiesForURL(CUtlString* cookies, const char* url, const char* host) {
    if (!cookies || !initialized_ || failed_) {
        return;
    }

    if (nextRequest_ == 0) {
        nextRequest_ = 1;
    }

    const std::uint32_t request = nextRequest_++;
    activeCookieRequest_ = request;
    Writer payload;
    payload.String(Text(url));
    payload.String(Text(host));
    if (!payload.Good() || !channel_.Queue({Kind::Control, static_cast<std::uint32_t>(Control::GetCookies),
        0, request, 0}, payload.Data())) {
        failed_ = true;
        return;
    }

    const Uint64 deadline = SDL_GetTicks() + ProviderStartupMilliseconds;
    while (!failed_ && cookieReplies_.find(request) == cookieReplies_.end() && SDL_GetTicks() < deadline) {
        Collect();
        SDL_Delay(1);
    }

    const auto response = cookieReplies_.find(request);
    if (response == cookieReplies_.end()) {
        failed_ = true;
        return;
    }

    const std::string value = std::move(response->second);
    cookieReplies_.erase(response);
    activeCookieRequest_ = 0;
    auto* copy = static_cast<char*>(std::malloc(value.size() + 1));
    if (!copy) {
        failed_ = true;
        return;
    }

    std::memcpy(copy, value.c_str(), value.size() + 1);
    std::free(cookies->m_pchString);
    cookies->m_pchString = copy;
}

void Controller::Dispatch(const Message& message) {
    const auto found = browsers_.find(message.header.browser);
    if (found == browsers_.end()) {
        return;
    }

    // Keep the serializer alive if a callback removes its own browser. The GUI target is detached immediately.
    const auto browser = found->second;
    IHTMLResponses_HL1* responses = browser->Responses();
    if (!responses) {
        return;
    }

    Reader reader(message.payload);
    if (message.header.operation >= static_cast<std::uint32_t>(eHTMLCommands_None)) {
        failed_ = true;
        return;
    }

    const auto operation = static_cast<EHTMLCommands>(message.header.operation);
    const Kind expectedKind = operation == eHTMLCommands_NeedsPaint ? Kind::Pixels : Kind::Event;
    if (message.header.kind != expectedKind) {
        failed_ = true;
        return;
    }

    switch (operation) {
    case eHTMLCommands_BrowserReady: {
        if (reader.Finish()) {
            browser->MarkReady();
            responses->BrowserReady();
            return;
        }

        break;
    }

    case eHTMLCommands_StartRequest: {
        std::string url;
        std::string target;
        std::string post;
        bool redirect = false;
        if (reader.String(url) && reader.String(target) && reader.String(post) && reader.Boolean(redirect) && reader.Finish()) {
            responses->BrowserStartRequest(url.c_str(), target.c_str(), post.c_str(), redirect);
            return;
        }

        break;
    }

    case eHTMLCommands_URLChanged: {
        std::string url;
        std::string post;
        bool redirect = false;
        if (reader.String(url) && reader.String(post) && reader.Boolean(redirect) && reader.Finish()) {
            responses->BrowserURLChanged(url.c_str(), post.c_str(), redirect);
            return;
        }

        break;
    }

    case eHTMLCommands_FinishedRequest: {
        std::string url;
        std::string title;
        if (reader.String(url) && reader.String(title) && reader.Finish()) {
            responses->BrowserFinishedRequest(url.c_str(), title.c_str());
            return;
        }

        break;
    }

    case eHTMLCommands_ShowPopup: {
        if (reader.Finish()) {
            responses->BrowserShowPopup();
            return;
        }

        break;
    }

    case eHTMLCommands_HidePopup: {
        if (reader.Finish()) {
            responses->BrowserHidePopup();
            return;
        }

        break;
    }

    case eHTMLCommands_SizePopup: {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t width = 0;
        std::int32_t height = 0;
        if (reader.I32(x) && reader.I32(y) && reader.I32(width) && reader.I32(height) && reader.Finish()) {
            responses->BrowserSizePopup(x, y, width, height);
            return;
        }

        break;
    }

    case eHTMLCommands_CanGoBackandForward: {
        bool back = false;
        bool forward = false;
        if (reader.Boolean(back) && reader.Boolean(forward) && reader.Finish()) {
            responses->BrowserCanGoBackandForward(back, forward);
            return;
        }

        break;
    }

    case eHTMLCommands_JSAlert: {
        std::string dialogText;
        if (reader.String(dialogText) && reader.Finish()) {
            responses->BrowserJSAlert(dialogText.c_str());
            return;
        }

        break;
    }

    case eHTMLCommands_JSConfirm: {
        std::string dialogText;
        if (reader.String(dialogText) && reader.Finish()) {
            responses->BrowserJSConfirm(dialogText.c_str());
            return;
        }

        break;
    }

    case eHTMLCommands_PopupHTMLWindow: {
        std::string url;
        std::int32_t width = 0;
        std::int32_t height = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;
        if (reader.String(url) && reader.I32(width) && reader.I32(height) && reader.I32(x) && reader.I32(y) && reader.Finish()) {
            responses->BrowserPopupHTMLWindow(url.c_str(), width, height, x, y);
            return;
        }

        break;
    }

    case eHTMLCommands_SetHTMLTitle: {
        std::string title;
        if (reader.String(title) && reader.Finish()) {
            responses->BrowserSetHTMLTitle(title.c_str());
            return;
        }

        break;
    }

    case eHTMLCommands_LoadingResource: {
        if (reader.Finish()) {
            responses->BrowserLoadingResource();
            return;
        }

        break;
    }

    case eHTMLCommands_StatusText: {
        std::string text;
        if (reader.String(text) && reader.Finish()) {
            responses->BrowserStatusText(text.c_str());
            return;
        }

        break;
    }

    case eHTMLCommands_SetCursor: {
        std::int32_t cursor = 0;
        if (reader.I32(cursor) && reader.Finish()) {
            responses->BrowserSetCursor(cursor);
            return;
        }

        break;
    }

    case eHTMLCommands_FileLoadDialog: {
        if (reader.Finish()) {
            responses->BrowserFileLoadDialog();
            return;
        }

        break;
    }

    case eHTMLCommands_ShowToolTip: {
        std::string text;
        if (reader.String(text) && reader.Finish()) {
            responses->BrowserShowToolTip(text.c_str());
            return;
        }

        break;
    }

    case eHTMLCommands_UpdateToolTip: {
        std::string text;
        if (reader.String(text) && reader.Finish()) {
            responses->BrowserUpdateToolTip(text.c_str());
            return;
        }

        break;
    }

    case eHTMLCommands_HideToolTip: {
        if (reader.Finish()) {
            responses->BrowserHideToolTip();
            return;
        }

        break;
    }

    case eHTMLCommands_Close: {
        if (reader.Finish()) {
            if (auto* closing = browser->CloseResponses()) {
                closing->BrowserClose();
            }
            return;
        }

        break;
    }

    case eHTMLCommands_LinkAtPositionResponse: {
        std::string url;
        std::int32_t x = 0;
        std::int32_t y = 0;
        if (reader.String(url) && reader.I32(x) && reader.I32(y) && reader.Finish()) {
            responses->BrowserLinkAtPositionResponse(url.c_str(), x, y);
            return;
        }

        break;
    }

    case eHTMLCommands_HorizontalScrollBarSizeResponse: {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t width = 0;
        std::int32_t height = 0;
        std::int32_t scroll = 0;
        std::int32_t maximum = 0;
        std::uint32_t zoomBits = 0;
        if (reader.I32(x) && reader.I32(y) && reader.I32(width) && reader.I32(height) && reader.I32(scroll) &&
            reader.I32(maximum) && reader.U32(zoomBits) && reader.Finish()) {
            float zoom = 0;
            static_assert(sizeof(zoom) == sizeof(zoomBits));
            std::memcpy(&zoom, &zoomBits, sizeof(zoom));
            responses->BrowserHorizontalScrollBarSizeResponse(x, y, width, height, scroll, maximum, zoom);
            return;
        }

        break;
    }

    case eHTMLCommands_VerticalScrollBarSizeResponse: {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t width = 0;
        std::int32_t height = 0;
        std::int32_t scroll = 0;
        std::int32_t maximum = 0;
        std::uint32_t zoomBits = 0;
        if (reader.I32(x) && reader.I32(y) && reader.I32(width) && reader.I32(height) && reader.I32(scroll) &&
            reader.I32(maximum) && reader.U32(zoomBits) && reader.Finish()) {
            float zoom = 0;
            static_assert(sizeof(zoom) == sizeof(zoomBits));
            std::memcpy(&zoom, &zoomBits, sizeof(zoom));
            responses->BrowserVerticalScrollBarSizeResponse(x, y, width, height, scroll, maximum, zoom);
            return;
        }

        break;
    }

    case eHTMLCommands_GetZoomResponse: {
        std::uint32_t bits = 0;
        if (reader.U32(bits) && reader.Finish()) {
            float zoom = 0;
            static_assert(sizeof(zoom) == sizeof(bits));
            std::memcpy(&zoom, &bits, sizeof(zoom));
            responses->BrowserGetZoomResponse(zoom);
            return;
        }

        break;
    }

    case eHTMLCommands_NeedsPaint: {
        std::int32_t texture = 0;
        std::int32_t width = 0;
        std::int32_t height = 0;
        PaintBounds update;
        std::int32_t popupWidth = 0;
        std::int32_t popupHeight = 0;
        std::vector<std::uint8_t> pixels;
        std::vector<std::uint8_t> popup;
        if (reader.I32(texture) && reader.I32(width) && reader.I32(height) &&
            reader.I32(update.x) && reader.I32(update.y) && reader.I32(update.width) && reader.I32(update.height) &&
            reader.I32(popupWidth) && reader.I32(popupHeight) && reader.Bytes(pixels) && reader.Bytes(popup) &&
            reader.Finish() &&
            width > 0 && width <= MaximumBrowserDimension && height > 0 && height <= MaximumBrowserDimension &&
            popupWidth >= 0 && popupWidth <= MaximumBrowserDimension &&
            popupHeight >= 0 && popupHeight <= MaximumBrowserDimension) {
            constexpr std::uint64_t PixelBytes = PaintBytesPerPixel;
            const std::uint64_t expected = static_cast<std::uint64_t>(width) * height * PixelBytes;
            const std::uint64_t expectedPopup = static_cast<std::uint64_t>(popupWidth) * popupHeight * PixelBytes;
            if (pixels.size() == expected && popup.size() == expectedPopup &&
                ValidPaintBounds(update, width, height)) {
                responses->BrowserNeedsPaint(texture, width, height, pixels.data(),
                    update.x, update.y, update.width, update.height,
                    popupWidth, popupHeight, popup.empty() ? nullptr : popup.data());
                return;
            }

        }

        break;
    }

    default:
        break;
    }

    failed_ = true;
}
}

std::unique_ptr<IHTMLChromeController> CreateController(const ControllerConfig& config) {
    return std::make_unique<Controller>(config);
}

bool ControllerQuiescent(IHTMLChromeController& controller) {
    return static_cast<Controller&>(controller).Quiescent();
}

void BeginControllerShutdown(IHTMLChromeController& controller) {
    // This owned-local helper intentionally does not extend the legacy vtable or the process protocol.
    static_cast<Controller&>(controller).BeginShutdown();
}
}
