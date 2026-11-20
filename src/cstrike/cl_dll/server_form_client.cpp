#include "server_form_client.h"

#include "VGUI/counterstrikeviewport.h"
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
constexpr unsigned kMenuShowBytes = kShowBytes + sizeof(std::uint32_t);
constexpr auto kMenuReplyTimeout = std::chrono::seconds(1);

struct MenuTarget {
    StockMenu menu{};
    int vguiId{};

    bool IsVgui(int id) const {
        return vguiId && (!id || vguiId == id);
    }

    bool IsText() const {
        return menu != StockMenu::None && !vguiId;
    }

    bool IsBuy() const {
        return menu == StockMenu::Buy || menu == StockMenu::BuyEquipment;
    }
};

struct MenuRequest {
    MenuTarget target;
    std::uint32_t serial{};
    Clock::time_point expires{};
};

struct ClientState {
    Session session;
    std::string peer;
    Presentation presentation;
    View visible;
    MenuTarget visibleMenu;
    MenuRequest menuRequest;
    std::uint32_t nextMenuSerial{};
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
    if (client.visibleMenu.vguiId == kVguiBuyMenu && CurrentPeer() == client.peer) {
        Send("client_buy_close\n");
    }
    client.visible = {};
    client.visibleMenu = {};
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

bool MatchesMenuRequest(View view, Clock::time_point now) {
    return view.menuRequest && view.menuRequest == client.menuRequest.serial && now < client.menuRequest.expires &&
        view.id == client.session.MenuForm(client.menuRequest.target.menu);
}

bool ConcealStockMenu(const MenuTarget& target) {
    if (target.vguiId) {
        return CS_ConcealStockMenu(target.vguiId);
    }
    auto& menu = gHUD.m_Menu;
    if (!menu.m_fMenuDisplayed || menu.m_fWaitingForMore ||
        (menu.m_flShutoffTime > 0 && gHUD.m_flTime >= menu.m_flShutoffTime)) {
        return false;
    }
    menu.m_fMenuDisplayed = 0;
    menu.m_bitsValidSlots = 0;
    menu.m_iFlags &= ~HUD_ACTIVE;
    return true;
}

void PresentPending(Clock::time_point now) {
    const auto view = client.presentation.TakeReady(now);
    const auto* form = view.token ? client.session.FindForm(view.id) : nullptr;
    if (!form) {
        return;
    }
    if (view.menuRequest) {
        if (!MatchesMenuRequest(view, now)) {
            return;
        }
        const auto request = std::exchange(client.menuRequest, {});
        if (!ConcealStockMenu(request.target)) {
            return;
        }
        client.visibleMenu = request.target;
    }
    client.visible = view;
    ShowFormPanel(*form, view.token, LookupImage);
}

void HandleShow(std::string_view packet, bool menuShow) {
    if (packet.size() != (menuShow ? kMenuShowBytes : kShowBytes)) {
        return;
    }
    const View view{ReadToken(packet, 2), static_cast<std::uint8_t>(packet[1]),
                    menuShow ? ReadToken(packet, kShowBytes) : 0};
    const auto now = Clock::now();
    if ((menuShow && !MatchesMenuRequest(view, now)) || !client.session.FindForm(view.id) ||
        !client.presentation.Queue(view, now)) {
        return;
    }
    if (!menuShow) {
        client.menuRequest = {};
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
        case Op::MenuBindings: client.session.ReceiveMenuBindings(packet); break;
        case Op::Show: HandleShow(packet, false); break;
        case Op::MenuShow: HandleShow(packet, true); break;
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

void CancelOverride(bool pending, bool visible) {
    if (pending) {
        client.menuRequest = {};
    }
    if (visible) {
        SendFormAction(client.visible.token, 0, {});
        if (client.visible.token) {
            ClearVisibleForm();
        }
    }
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
    CloseFormPanel();
    client = {};
}

void RequestMenuOverride(StockMenu menu, int vguiMenu) {
    if (!client.session.MenuForm(menu) || !Enabled() || CurrentPeer() != client.peer) {
        return;
    }
    const auto now = Clock::now();
    const auto& pending = client.menuRequest;
    if (pending.target.menu == menu && pending.target.vguiId == vguiMenu && now < pending.expires) {
        return;
    }
    ++client.nextMenuSerial;
    if (!client.nextMenuSerial) {
        ++client.nextMenuSerial;
    }
    client.menuRequest = {{menu, vguiMenu}, client.nextMenuSerial, now + kMenuReplyTimeout};
    Send("cseform_menu " + std::to_string(static_cast<int>(menu)) + " " + std::to_string(client.nextMenuSerial));
}

void CancelVguiMenuOverride(int vguiMenu) {
    CancelOverride(client.menuRequest.target.IsVgui(vguiMenu), client.visibleMenu.IsVgui(vguiMenu));
}

void CancelTextMenuOverride() {
    CancelOverride(client.menuRequest.target.IsText(), client.visibleMenu.IsText());
}

void CancelBuyMenuOverride() {
    CancelOverride(client.menuRequest.target.IsBuy(), client.visibleMenu.IsBuy());
}

bool IsVguiMenuOverridden(int vguiMenu) {
    return client.visibleMenu.IsVgui(vguiMenu);
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
        ClearVisibleForm();
        client.presentation.Dismiss(Clock::now());
    }
    auto encoded = value.empty() ? "-" : EncodeHex(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    Send("cseform_action " + std::to_string(token) + " " + std::to_string(control) + " " + encoded);
}

} // namespace cse::forms
