#include "server_form_client.h"

#include "VGUI/server_form_panel.h"
#include "server_form_session.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <chrono>
#include <exception>
#include <memory>
#include <random>
#include <utility>

#include "hud.h"
#include "cl_util.h"
#include "../../common/demo_api.h"
#include "../../common/net_api.h"

namespace cse::forms {
namespace {

using Clock = std::chrono::steady_clock;
constexpr int kMaximumUserMessage = 192;
constexpr unsigned kShowBytes = 6;
constexpr unsigned kHideBytes = 5;

struct ClientState {
    Session session;
    std::string peer;
    Presentation presentation;
    View visible;
};

ClientState client;
cvar_t* enableForms;

bool Enabled() {
    static char allowDownload[] = "cl_allowdownload";
    return enableForms && enableForms->value != 0.0f && gEngfuncs.pfnGetCvarFloat(allowDownload) != 0.0f;
}

std::string CurrentPeer() {
    if (!gEngfuncs.pNetAPI || !gEngfuncs.pDemoAPI || gEngfuncs.pDemoAPI->IsPlayingback() ||
        gEngfuncs.IsSpectateOnly()) {
        return {};
    }
    net_status_t status{};
    gEngfuncs.pNetAPI->Status(&status);
    auto& endpoint = status.remote_address;
    return status.connected && (endpoint.type == NA_IP || endpoint.type == NA_LOOPBACK)
        ? gEngfuncs.pNetAPI->AdrToString(&endpoint) : "";
}

std::filesystem::path CacheRoot() {
    constexpr char application[] = "CounterStrikeExtended";
    const std::unique_ptr<char, decltype(&SDL_free)> preferences(SDL_GetPrefPath(application, application), SDL_free);
    if (!preferences) {
        return {};
    }
    return std::filesystem::u8path(preferences.get()) / "server-forms-v1";
}

bool GenerateNonce(Nonce& nonce) {
    // The supported macOS and MSVC standard libraries back random_device with platform entropy.
    try {
        std::random_device random;
        std::uniform_int_distribution<unsigned> byte(0, 255);
        for (auto& element : nonce) {
            element = static_cast<std::uint8_t>(byte(random));
        }
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

void Send(std::string command) {
    gEngfuncs.pfnServerCmd(command.data());
}

std::uint32_t ReadToken(std::string_view packet, unsigned index) {
    std::uint32_t token = 0;
    for (unsigned i = 0; i < sizeof(token); ++i) {
        token |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(packet[index + i])) << (8 * i);
    }
    return token;
}

const Image* LookupImage(std::string_view name) {
    return client.session.FindImage(name);
}

void ClearVisibleForm() {
    CloseFormPanel();
    client.visible = {};
}

void Begin(std::string peer) {
    Nonce nonce{};
    if (!GenerateNonce(nonce)) {
        return;
    }
    client.peer = std::move(peer);
    client.session.Begin(nonce, CacheRoot(), client.peer);
    Send("cseform_hello " + EncodeHex(nonce.data(), nonce.size()));
}

void HandleAsset(std::string_view packet) {
    const auto reply = client.session.Receive(packet);
    switch (reply.status) {
        case AssetStatus::Need: Send("cseform_asset " + std::to_string(reply.id) + " need"); break;
        case AssetStatus::Ready: Send("cseform_asset " + std::to_string(reply.id) + " ready"); break;
        case AssetStatus::Reject: Send("cseform_asset " + std::to_string(reply.id) + " reject"); break;
        case AssetStatus::Ignored: break;
    }
}

void PresentPending(Clock::time_point now) {
    const auto view = client.presentation.TakeReady(now);
    if (view.token) {
        if (const auto* form = client.session.FindForm(view.id)) {
            client.visible = view;
            ShowFormPanel(*form, view.token, LookupImage);
        }
    }
}

void HandleShow(std::string_view packet) {
    if (packet.size() != kShowBytes) {
        return;
    }
    const View view{ReadToken(packet, 2), static_cast<std::uint8_t>(packet[1])};
    const auto now = Clock::now();
    if (!client.session.FindForm(view.id) || !client.presentation.Queue(view, now)) {
        return;
    }
    // The server has retired the visible instance. Keep the latest replacement until the display interval ends.
    ClearVisibleForm();
    PresentPending(now);
}

int Receive(const char*, int size, void* data) {
    if (!Enabled() || !data || size <= 0 || size > kMaximumUserMessage) {
        return 1;
    }
    const std::string_view packet(static_cast<const char*>(data), static_cast<std::size_t>(size));
    auto peer = CurrentPeer();
    if (peer.empty()) {
        return 1;
    }
    if (client.session.Active() && peer != client.peer) {
        EndServerFormsSession();
    }
    const auto op = static_cast<Op>(packet.front());
    if (op == Op::Hello && packet.size() == 1 && !client.session.Active()) {
        Begin(std::move(peer));
    }
    if (!client.session.Active()) {
        return 1;
    }
    switch (op) {
        case Op::Offer:
        case Op::Chunk: HandleAsset(packet); break;
        case Op::Show: HandleShow(packet); break;
        case Op::Hide:
            if (packet.size() == kHideBytes) {
                const auto token = ReadToken(packet, 1);
                client.presentation.Cancel(token);
                if (token && token == client.visible.token) {
                    ClearVisibleForm();
                }
            }
            break;
        default: break;
    }
    return 1;
}

} // namespace

void RegisterServerForms() {
    static char name[] = "cl_cse_server_forms";
    static char defaultValue[] = "1";
    enableForms = gEngfuncs.pfnRegisterVariable(name, defaultValue, FCVAR_ARCHIVE | FCVAR_PRIVILEGED);
    gEngfuncs.pfnHookUserMsg(const_cast<char*>(kUserMessage), Receive);
}

void RunServerFormsFrame() {
    if (!client.session.Active()) {
        return;
    }
    if (!Enabled() || CurrentPeer() != client.peer) {
        EndServerFormsSession();
        return;
    }
    PresentPending(Clock::now());
}

void EndServerFormsSession() {
    ClearVisibleForm();
    client = {};
}

void SendFormAction(std::uint32_t token, unsigned control, std::string_view value) {
    if (!token || token != client.visible.token || !Enabled() || CurrentPeer() != client.peer) {
        return;
    }
    const auto* form = client.session.FindForm(client.visible.id);
    if (!form || (control ? !ValidAction(*form, control, value) : !value.empty())) {
        return;
    }
    if (!control) {
        client.visible = {};
        client.presentation.Dismiss(Clock::now());
    }
    auto encoded = value.empty() ? "-" : EncodeHex(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    Send("cseform_action " + std::to_string(token) + " " + std::to_string(control) + " " + encoded);
}

} // namespace cse::forms
