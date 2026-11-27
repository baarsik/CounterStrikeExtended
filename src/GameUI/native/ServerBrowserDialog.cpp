#include "ServerBrowserDialog.h"
#include "ServerBrowserSettingsDialog.h"
#include "../../vgui2/controls/SmokedGlass.h"
#include "localization_controls.h"
#include "Button.h"
#include "CheckButton.h"
#include "ToggleButton.h"
#include "RadioButton.h"
#include "MessageBox.h"
#include "vgui/ISystem.h"
#include "../../external/steamworks/isteamapps.h"
#include <deque>
#include <cstdio>
#include "ImageList.h"
#include "vgui/IScheme.h"
#include "ComboBox.h"
#include "Label.h"
#include "ListPanel.h"
#include "Menu.h"
#include "MenuItem.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "PropertyPage.h"
#include "PropertySheet.h"
#include "ScrollBar.h"
#include "TextEntry.h"
#include "controls.h"
#include "../../engine/controls.h"
#include "vgui/IInput.h"
#include "vgui/IVGui.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <functional>
#include <initializer_list>
#include <ctime>

namespace cse::server_browser {
namespace {
constexpr int ScrollWidth = 18;
constexpr size_t OriginalPasswordCapacity = 64;
constexpr AppId_t HalfLifeApp = 70;
const char *const TabLabels[]{"#ServerBrowser_InternetTab", "#ServerBrowser_FavoritesTab",
    "#ServerBrowser_HistoryTab", "#ServerBrowser_SpectateTab", "#ServerBrowser_LanTab", "#ServerBrowser_FriendsTab"};
const char *const PageNames[]{"InternetGames", "FavoriteGames", "HistoryGames", "SpectateGames", "LanGames", "FriendsGames"};
constexpr std::array<Column, 10> VisualColumns{Column::Password, Column::Bots, Column::Secure, Column::Players,
    Column::Name, Column::Game, Column::Map, Column::Ping, Column::Address, Column::LastPlayed};
const char *const ColumnNames[]{"Password", "Bots", "Secure", "Players", "Name", "GameDesc", "Map", "Ping",
    "IPAddr", "LastPlayed"};
const char *const ColumnLabels[]{"#ServerBrowser_Password", "#ServerBrowser_Bots", "#ServerBrowser_Secure",
    "#ServerBrowser_Players", "#ServerBrowser_Servers", "#ServerBrowser_Game", "#ServerBrowser_Map",
    "#ServerBrowser_Latency", "#ServerBrowser_IPAddress", "#ServerBrowser_LastPlayed"};
constexpr std::array<int, 10> ColumnWidths{16, 20, 16, 55, 50, 112, 90, 55, 64, 100};
constexpr int ReferencePageWidth = 624;
constexpr int ReferencePageHeight = 278;
constexpr int OriginalMinimumWidth = 640;
constexpr int OriginalMinimumHeight = 384;
constexpr double InternetCacheMaxAgeSeconds = 60.0;
bool UsesMasterserver(ListKind kind) {
    return kind == ListKind::Internet || kind == ListKind::Spectate;
}

uint32_t SavedListFlag(ListKind kind) {
    if (kind == ListKind::Favorites) {
        return k_unFavoriteFlagFavorite;
    }
    if (kind == ListKind::History) {
        return k_unFavoriteFlagHistory;
    }
    return k_unFavoriteFlagNone;
}

const char *EmptyListText(const Status &state) {
    switch (state.kind) {
    case ListKind::Favorites:
        return "#ServerBrowser_NoFavoriteServers";
    case ListKind::History:
        return "#ServerBrowser_NoServersPlayed";
    case ListKind::LAN:
        return "#ServerBrowser_NoLanServers";
    case ListKind::Friends:
        return "#ServerBrowser_NoFriendsServers";
    case ListKind::Internet:
    case ListKind::Spectate:
        if (state.completion == eNoServersListedOnMasterServer) {
            return "#ServerBrowser_MasterServerHasNoServersListed";
        }
        if (state.completion == eServerResponded && state.responding == 0) {
            return "#ServerBrowser_NoInternetGamesResponded";
        }
        return "#ServerBrowser_NoInternetGames";
    }
    return "";
}

int SlotOrder(vgui2::ListPanel *, const vgui2::ListPanelItem &left, const vgui2::ListPanelItem &right) {
    const int a = left.kv->GetInt("slot");
    const int b = right.kv->GetInt("slot");
    return (a > b) - (a < b);
}
void ResizeSlots(vgui2::ListPanel &list, std::vector<int> &slots, size_t capacity) {
    while (slots.size() > capacity) {
        list.RemoveItem(slots.back());
        slots.pop_back();
    }
    while (slots.size() < capacity) {
        auto *row = new KeyValues("row");
        row->SetInt("slot", static_cast<int>(slots.size()));
        slots.push_back(list.AddItem(row, 0, false, false));
        row->deleteThis();
    }
}
std::string Text(vgui2::TextEntry *entry) {
    constexpr size_t FilterTextBytes = 256;
    std::array<char, FilterTextBytes> buffer{};
    entry->GetText(buffer.data(), static_cast<int>(buffer.size()));
    return buffer.data();
}
// Draw the cog as geometry so it works with every installed font and needs no raster asset.
class BrowserSettingsButton final : public vgui2::Button {
public:
    explicit BrowserSettingsButton(vgui2::Panel *parent) : Button(parent, "BrowserSettings", "") {
        SetCommand("BrowserSettings");
        AddActionSignalTarget(parent);
    }

    Color GetButtonFgColor() override {
        if (OriginalInactive()) {
            return GetSchemeColor("TitleButtonDisabledFgColor", vgui2::scheme()->GetIScheme(GetScheme()));
        }
        return Button::GetButtonFgColor();
    }

    Color GetButtonBgColor() override {
        if (OriginalInactive()) {
            return GetSchemeColor("TitleButtonDisabledBgColor", vgui2::scheme()->GetIScheme(GetScheme()));
        }
        return Button::GetButtonBgColor();
    }

protected:
    void PaintBackground() override {
        // Focus can change without a layout pass; match the frame's title-button state immediately.
        SetBgColor(GetButtonBgColor());
        SetBorder(OriginalInactive() ? vgui2::scheme()->GetIScheme(GetScheme())->GetBorder("TitleButtonDisabledBorder") :
            Button::GetBorder(IsDepressed(), IsArmed(), IsSelected(), HasFocus()));
        Button::PaintBackground();
    }

    void Paint() override {
        Button::Paint();
        constexpr int CogPoints = 32;
        constexpr int PointsPerTooth = 4;
        constexpr int CrestPoints = 2;
        constexpr double FullCircle = 6.283185307179586;
        constexpr double OuterRadius = 6.5;
        constexpr double InnerRadius = 4.8;
        constexpr double HoleRadius = 2.2;
        const double centerX = GetWide() * 0.5;
        const double centerY = GetTall() * 0.5;
        std::array<int, CogPoints + 1> x{};
        std::array<int, CogPoints + 1> y{};
        vgui2::surface()->DrawSetColor(GetButtonFgColor());
        for (int point = 0; point <= CogPoints; ++point) {
            const double angle = FullCircle * point / CogPoints;
            const double radius = point % PointsPerTooth < CrestPoints ? OuterRadius : InnerRadius;
            x[point] = static_cast<int>(std::lround(centerX + std::cos(angle) * radius));
            y[point] = static_cast<int>(std::lround(centerY + std::sin(angle) * radius));
        }
        vgui2::surface()->DrawPolyLine(x.data(), y.data(), static_cast<int>(x.size()));
        for (int point = 0; point <= CogPoints; ++point) {
            const double angle = FullCircle * point / CogPoints;
            x[point] = static_cast<int>(std::lround(centerX + std::cos(angle) * HoleRadius));
            y[point] = static_cast<int>(std::lround(centerY + std::sin(angle) * HoleRadius));
        }
        vgui2::surface()->DrawPolyLine(x.data(), y.data(), static_cast<int>(x.size()));
    }

private:
    bool OriginalInactive() {
        const vgui2::VPANEL focus = vgui2::input()->GetFocus();
        return !cse::ui::ControlsSmokedGlass() && GetParent() &&
            !(focus && vgui2::ipanel()->HasParent(focus, GetParent()->GetVPanel()));
    }
};

class LogicalScroll final : public vgui2::ScrollBar {
public:
    LogicalScroll(vgui2::Panel *parent, std::function<void(const LogicalScroll *, size_t)> seek)
        : ScrollBar(parent, "LogicalScroll", true), seek_(std::move(seek)) {}
    void Synchronize(size_t count, size_t window, size_t first) {
        updating_ = true;
        const bool overflow = count > window;
        constexpr int InactiveScrollExtent = 1;
        SetRange(0, overflow ? static_cast<int>(std::min(count, static_cast<size_t>(INT_MAX))) : InactiveScrollExtent);
        SetRangeWindow(overflow ? static_cast<int>(window) : 0);
        SetValue(overflow ? static_cast<int>(first) : 0);
        SetEnabled(overflow);
        SetVisible(overflow);
        updating_ = false;
    }

protected:
    void SendSliderMoveMessage(int position) override {
        // Slider action signals are queued: an older value may arrive after synchronization.
        if (!updating_ && position == GetValue()) {
            seek_(this, static_cast<size_t>(std::max(position, 0)));
        }
    }

private:
    std::function<void(const LogicalScroll *, size_t)> seek_;
    bool updating_ = false;
};
void ClampFrame(vgui2::Frame &frame, int left, int top) {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    vgui2::surface()->GetWorkspaceBounds(x, y, width, height);
    if (width <= 0 || height <= 0) {
        vgui2::surface()->GetScreenSize(width, height);
    }
    constexpr int MinimumWidth = 640;
    constexpr int MinimumHeight = 384;
    const int minimumWidth = std::min(width, frame.GetProportionalScaledValue(MinimumWidth));
    const int minimumHeight = std::min(height, frame.GetProportionalScaledValue(MinimumHeight));
    frame.SetMinimumSize(minimumWidth, minimumHeight);
    frame.SetSize(std::clamp(frame.GetWide(), minimumWidth, width), std::clamp(frame.GetTall(), minimumHeight, height));
    frame.SetPos(std::clamp(left, x, x + width - frame.GetWide()), std::clamp(top, y, y + height - frame.GetTall()));
}
void ClampFrame(vgui2::Frame &frame) {
    int left = 0;
    int top = 0;
    frame.GetPos(left, top);
    ClampFrame(frame, left, top);
}
class SlotList : public vgui2::ListPanel {
public:
    SlotList(vgui2::Panel *parent, const char *name) : ListPanel(parent, name) {
        SetMultiSelectEnabled(true);
        // Every column compares the canonical viewport slot; ListPanel otherwise leaves sort indices unset.
        constexpr int SlotSortColumn = 0;
        SetSortColumn(SlotSortColumn);
    }
    size_t Capacity() {
        return static_cast<size_t>(std::max(1.0f, std::floor(GetRowsPerPage())));
    }

protected:
    int GetStartItem() override {
        return 0;
    }

};
void AddFallback(const char *token, const wchar_t *text) {
    if (!vgui2::localize()->Find(token)) {
        // ILocalize copies both token and value into its own symbol/string tables.
        vgui2::localize()->AddString(token, const_cast<wchar_t *>(text), "CSEServerBrowser");
    }
}
void RegisterFallbacks() {
#include "ServerBrowserEnglish.generated.inc"
}
}

BrowserSession::BrowserSession(ISteamMatchmakingServers *serverProvider, ISteamMatchmaking *favoriteProvider,
                               IFileSystem *filesystem, IConnectionEngine &engine, AppId_t app, const char *game,
                               std::string preferenceRoot, IFavoriteStore *injectedFavorites)
    : masterserverProvider(serverProvider), favorites(favoriteProvider),
      model({serverProvider ? &masterserverProvider : nullptr, injectedFavorites ? injectedFavorites : &favorites}),
      parkedModel({serverProvider ? &masterserverProvider : nullptr, injectedFavorites ? injectedFavorites : &favorites}),
      storage(filesystem, std::move(preferenceRoot)), config(storage), connection(engine, model, app, game),
      servers(serverProvider ? &masterserverProvider : nullptr) {
    config.Load();
    settings = config.Settings();
    masterserverProvider.Select(settings.masterservers.Selected());
    // Every session initially browses the running game. Saved remaining filters are retained.
    settings.filters.app = app;
    settings.filters.gameDirectory = game;
    model.SetSort(settings.sort);
    presentation.RequestSort(settings.sort);
}

bool BrowserSession::AddFavorite(AppId_t app, Address address) {
    const bool added = model.AddFavorite(app, address);
    if (added) {
        dirtySavedLists |= k_unFavoriteFlagFavorite;
    }
    return added;
}

class BrowserViewport : public SlotList {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserViewport, SlotList, "ListPanel");

public:
    BrowserViewport(vgui2::Panel *parent, ServerBrowserDialog &owner, bool history)
        : SlotList(parent, "gamelist"), owner_(owner) {
        const size_t count = history ? VisualColumns.size() : VisualColumns.size() - 1;
        for (size_t index = 0; index < count; ++index) {
            int flags = 0;
            if (VisualColumns[index] == Column::Password || VisualColumns[index] == Column::Secure) {
                flags = COLUMN_FIXEDSIZE | COLUMN_IMAGE;
            } else if (VisualColumns[index] == Column::Name) {
                flags = COLUMN_RESIZEWITHWINDOW | COLUMN_UNHIDABLE;
            } else if (VisualColumns[index] == Column::Address) {
                flags = COLUMN_HIDDEN;
            }
            AddColumnHeader(static_cast<int>(index), ColumnNames[index], ColumnLabels[index], ColumnWidths[index], flags);
            SetSortFunc(static_cast<int>(index), SlotOrder);
        }
        SetAllowUserModificationOfColumns(true);
        SetColumnHeaderTooltip(0, "#ServerBrowser_PasswordColumn_Tooltip");
        SetColumnHeaderTooltip(1, "#ServerBrowser_BotColumn_Tooltip");
        SetColumnHeaderTooltip(2, "#ServerBrowser_SecureColumn_Tooltip");
    }
    uint32_t HiddenColumns() {
        auto *settings = new KeyValues("columns");
        GetUserConfigSettings(settings);
        uint32_t hidden = 0;
        for (int column = 0; column < GetNumColumnHeaders(); ++column) {
            if (settings->GetInt((std::to_string(column) + "_hidden").c_str())) {
                hidden |= 1U << column;
            }
        }
        settings->deleteThis();
        return hidden;
    }
    void RestoreListFont() {
        auto *style = vgui2::scheme()->GetIScheme(GetScheme());
        auto font = style->GetFont("ListSmall", IsProportional());
        if (!font) {
            font = style->GetFont("DefaultSmall", IsProportional());
        }
        SetFont(font);
    }

protected:
    void ApplySchemeSettings(vgui2::IScheme *style) override {
        SlotList::ApplySchemeSettings(style);
        auto *images = new vgui2::ImageList(false);
        for (const char *name : {"servers/icon_password", "servers/icon_bots", "servers/icon_robotron",
                 "servers/icon_secure_deny", "servers/icon_password_column", "servers/icon_bots_column",
                 "servers/icon_robotron_column"}) {
            images->AddImage(vgui2::scheme()->GetImage(name, false));
        }
        SetImageList(images, true);
        constexpr int PasswordHeaderImage = 5;
        constexpr int BotsHeaderImage = 6;
        constexpr int SecureHeaderImage = 7;
        SetColumnHeaderImage(0, PasswordHeaderImage);
        SetColumnHeaderImage(1, BotsHeaderImage);
        SetColumnHeaderImage(2, SecureHeaderImage);
        RestoreListFont();
    }
    void PerformLayout() override {
        // Ordinary ListPanel owns column resize/visibility. Slots contain only the bounded viewport.
        vgui2::ListPanel::PerformLayout();
        if (auto *bar = dynamic_cast<vgui2::ScrollBar *>(FindChildByName("VertScrollBar", false))) {
            bar->SetVisible(false);
            bar->SetRange(0, 1);
            bar->SetRangeWindow(0);
            bar->SetValue(0);
        }
    }
    MESSAGE_FUNC_INT(OnBrowserSort, "SetSortColumn", column) {
        owner_.ChooseSort(column);
    }
    void OnMousePressed(vgui2::MouseCode code) override {
        SlotList::OnMousePressed(code);
        Select();
        if (code == vgui2::MOUSE_RIGHT) {
            owner_.ShowContextMenu();
        }
    }
    void OnMouseDoublePressed(vgui2::MouseCode code) override {
        Select();
        if (code == vgui2::MOUSE_LEFT) {
            owner_.ConnectSelection();
        }
    }
    void OnMouseWheeled(int delta) override {
        owner_.ScrollViewport(delta);
    }
    void OnKeyCodeTyped(vgui2::KeyCode code) override {
        switch (code) {
        case vgui2::KEY_ENTER:
            owner_.ConnectSelection();
            break;
        case vgui2::KEY_UP:
            owner_.MoveSelection(-1);
            break;
        case vgui2::KEY_DOWN:
            owner_.MoveSelection(1);
            break;
        case vgui2::KEY_PAGEUP:
            owner_.MoveSelection(-1, false, true);
            break;
        case vgui2::KEY_PAGEDOWN:
            owner_.MoveSelection(1, false, true);
            break;
        case vgui2::KEY_HOME:
            owner_.MoveSelection(-1, true);
            break;
        case vgui2::KEY_END:
            owner_.MoveSelection(1, true);
            break;
        default:
            SlotList::OnKeyCodeTyped(code);
            break;
        }
    }

private:
    void Select() {
        std::vector<size_t> selected;
        for (int index = 0; index < GetSelectedItemsCount(); ++index) {
            selected.push_back(static_cast<size_t>(GetItem(GetSelectedItem(index))->GetInt("slot")));
        }
        owner_.SelectVisibleItems(selected);
    }
    ServerBrowserDialog &owner_;
};

class BrowserPasswordDialog final : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserPasswordDialog, vgui2::Frame, "Frame");
public:
    BrowserPasswordDialog(vgui2::Panel *parent, BrowserSession &session, const Row &row,
                          std::function<void(std::string_view)> submit)
        : Frame(parent, "DialogServerPassword", false, false), session_(session), submit_(std::move(submit)) {
        SetDeleteSelfOnClose(true);
        SetSizeable(false);
        SetTitle("#ServerBrowser_ServerRequiresPasswordTitle", true);
        entry_ = new vgui2::TextEntry(this, "PasswordEntry");
        LoadControlSettings("servers/DialogServerPassword.res", "PLATFORM");
        entry_->SetTextHidden(true);
        // Original UI owns a 64-byte buffer, independently of the engine's 128-byte capacity.
        entry_->SetMaximumCharCount(static_cast<int>(OriginalPasswordCapacity - 1));
        entry_->SetAllowNonAsciiCharacters(true);
        SetControlString("GameLabel", row.name.empty() ? ConnectionAddress(row.key).c_str() : row.name.c_str());
        MoveToCenterOfScreen();
        entry_->RequestFocus(0);
    }
    void Retire() {
        if (!retiring_) {
            retiring_ = true;
            submit_ = {};
            Clear();
            Frame::OnClose();
        }
    }
protected:
    void OnCommand(const char *command) override {
        BrowserUse use(session_);
        if (retiring_ || session_.closing || !session_.active) {
            Retire();
            return;
        }
        if (std::strcmp(command, "Connect") == 0) {
            std::array<char, OriginalPasswordCapacity> buffer{};
            entry_->GetText(buffer.data(), static_cast<int>(buffer.size()));
            auto submit = std::move(submit_);
            // Invalidate before calling the owner: duplicate queued commands cannot connect twice.
            Retire();
            if (submit) {
                submit(buffer.data());
            }
            volatile char *bytes = buffer.data();
            for (size_t index = 0; index < buffer.size(); ++index) {
                bytes[index] = 0;
            }
        } else {
            Frame::OnCommand(command);
        }
    }
    void OnClose() override {
        Retire();
    }
private:
    void Clear() {
        std::array<wchar_t, OriginalPasswordCapacity> blank{};
        std::fill(blank.begin(), blank.end() - 1, L' ');
        entry_->SetText(blank.data());
        entry_->SaveUndoState();
        entry_->SetText(L"");
        entry_->SaveUndoState();
    }
    BrowserSession &session_;
    std::function<void(std::string_view)> submit_;
    vgui2::TextEntry *entry_;
    bool retiring_ = false;
};

std::wstring LocalizedRuleValue(const char *token) {
    const auto *text = vgui2::localize()->Find(token);
    return text ? std::wstring(text) : std::wstring{};
}

std::wstring RuleChoice(double value, std::initializer_list<const char *> choices) {
    if (value < 0 || value >= static_cast<double>(choices.size()) || std::trunc(value) != value) {
        return {};
    }
    return LocalizedRuleValue(choices.begin()[static_cast<size_t>(value)]);
}

constexpr double SecondsPerRuleMinute = 60;
std::wstring RuleDuration(double seconds) {
    if (seconds < 0 || !std::isfinite(seconds)) {
        return {};
    }
    const double minutes = std::floor(seconds / SecondsPerRuleMinute);
    const double remainder = std::fmod(seconds, SecondsPerRuleMinute);
    std::array<wchar_t, 32> minutesText{};
    std::array<wchar_t, 32> secondsText{};
    std::array<wchar_t, 128> result{};
    std::swprintf(minutesText.data(), minutesText.size(), L"%g", minutes);
    std::swprintf(secondsText.data(), secondsText.size(), L"%g", remainder);
    auto *localize = vgui2::localize();
    if (minutes == 0) {
        localize->ConstructString(result.data(), sizeof(result), localize->Find("#CSEServerBrowser_RuleSeconds"),
            1, secondsText.data());
    } else if (remainder == 0) {
        localize->ConstructString(result.data(), sizeof(result), localize->Find("#CSEServerBrowser_RuleMinutes"),
            1, minutesText.data());
    } else {
        localize->ConstructString(result.data(), sizeof(result),
            localize->Find("#CSEServerBrowser_RuleMinutesSeconds"), 2, minutesText.data(), secondsText.data());
    }
    return result.data();
}

std::wstring RuleValue(ServerRule rule, double value, std::string_view gameDirectory) {
    constexpr const char *Off = "#CSEServerBrowser_RuleOff";
    constexpr const char *On = "#CSEServerBrowser_RuleOn";
    switch (rule) {
    case ServerRule::FreeForAll:
        return RuleChoice(value, {"#CSEServerBrowser_RuleTeams", "#CSEServerBrowser_RuleFFA"});
    case ServerRule::Respawn:
        if (gameDirectory.empty()) {
            return {};
        }
        if (value <= 0) {
            return LocalizedRuleValue(Off);
        }
        return GameDirectoriesMatch(gameDirectory, "valve") ? LocalizedRuleValue(On) : RuleDuration(value);
    case ServerRule::InfiniteAmmo:
        return RuleChoice(value, {Off, "#CSEServerBrowser_RuleUnlimitedClip", "#CSEServerBrowser_RuleUnlimitedReserve"});
    case ServerRule::BuyAnywhere:
        return RuleChoice(value, {"#CSEServerBrowser_RuleBuyZones", "#CSEServerBrowser_RuleAnywhere",
            "#CSEServerBrowser_RuleAnywhereT", "#CSEServerBrowser_RuleAnywhereCT"});
    case ServerRule::FriendlyFire:
        return RuleChoice(value, {Off, On});
    case ServerRule::RoundTime:
        return RuleDuration(value * SecondsPerRuleMinute);
    case ServerRule::BuyTime: {
        constexpr double UnlimitedBuyTime = -1;
        if (value == UnlimitedBuyTime) {
            return LocalizedRuleValue("#CSEServerBrowser_RuleUnlimited");
        }
        return RuleDuration(value * SecondsPerRuleMinute);
    }
    case ServerRule::FreezeTime:
    case ServerRule::BombTimer:
        return RuleDuration(value);
    case ServerRule::Count:
        return {};
    }
    return {};
}

std::wstring EngineVersionText(const std::string &version) {
    // ReHLDS publishes the game patch, protocol and engine build as one comma-separated value.
    const std::string_view report(version);
    const auto first = report.find(',');
    const auto last = report.rfind(',');
    if (first == std::string_view::npos || first == 0 || first == last || last + 1 == report.size()) {
        return cse::localization::WideLiteral(version.c_str());
    }
    const auto protocol = report.substr(first + 1, last - first - 1);
    constexpr std::string_view DecimalDigits = "0123456789";
    if (protocol.empty() || protocol.find_first_not_of(DecimalDigits) != std::string_view::npos ||
        report.substr(last + 1).find_first_not_of(DecimalDigits) != std::string_view::npos) {
        return cse::localization::WideLiteral(version.c_str());
    }
    const auto build = cse::localization::WideLiteral(version.c_str() + last + 1);
    auto *localize = vgui2::localize();
    if (auto *format = localize->Find("#CSEServerBrowser_VersionBuild")) {
        std::array<wchar_t, 96> result{};
        localize->ConstructString(result.data(), sizeof(result), format, 1, build.c_str());
        return result.data();
    }
    return build;
}

class BrowserDetailsHeading final : public vgui2::Label {
public:
    explicit BrowserDetailsHeading(vgui2::Panel *parent) : Label(parent, "ServerText", "") {}

protected:
    void ApplySchemeSettings(vgui2::IScheme *scheme) override {
        Label::ApplySchemeSettings(scheme);
        if (cse::ui::ControlsSmokedGlass()) {
            constexpr int HeadingFontHeight = 20;
            SetFont(vgui2::smoked::TextFont(this, scheme, GetFont(), true, vgui2::smoked::Pixels(HeadingFontHeight)));
        }
    }
};

class BrowserDetailsDialog : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserDetailsDialog, vgui2::Frame, "Frame");
public:
    BrowserDetailsDialog(ServerBrowserDialog &parent, BrowserSession &session, const Row &row)
        : Frame(&parent, "DialogGameInfo", false, false), parent_(parent), session_(session), row_(row),
          acceptedSerial_(session.connection.AcceptanceSerial()) {
        SetDeleteSelfOnClose(false);
        SetMinimumSize(OriginalInfoMinimumWidth, OriginalInfoMinimumHeight);
        SetTitle("#ServerBrowser_GameInfoWithNameTitle", true);
        SetDialogVariable("game", row.name.c_str());
        for (const char *name : {"Connect", "Close", "Refresh"}) {
            auto *button = new vgui2::Button(this, name, "");
            button->SetCommand(name);
            button->AddActionSignalTarget(this);
        }
        retry_ = new vgui2::ToggleButton(this, "AutoRetry", "");
        retry_->AddActionSignalTarget(this);
        alert_ = new vgui2::RadioButton(this, "AutoRetryAlert", "");
        join_ = new vgui2::RadioButton(this, "AutoRetryJoin", "");
        new BrowserDetailsHeading(this);
        address_ = new vgui2::Label(this, "ServerAddress", "");
        address_->SetVisible(false);
        engineVersion_ = CreateVersionField("EngineVersionLabel", "#CSEServerBrowser_VersionEngine", "EngineVersion");
        gameDllVersion_ = CreateVersionField("GameDllVersionLabel", "#CSEServerBrowser_VersionGameDll", "GameDllVersion");
        taggedVersions_ = ReadVersionTags(row_.tags);
        // Both original resource variants place and resize the tab body through this field.
        tabs_ = new vgui2::PropertySheet(this, "PlayerList");
        constexpr int DetailsTabWidth = 94;
        tabs_->SetTabWidth(DetailsTabWidth);
        list_ = new vgui2::ListPanel(tabs_, "PlayersList");
        list_->AddColumnHeader(0, "PlayerName", "#ServerBrowser_PlayerName", 156,
            vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW);
        list_->AddColumnHeader(1, "Score", "#ServerBrowser_Score", 64, 0);
        list_->AddColumnHeader(2, "Time", "#ServerBrowser_Time", 64, 0);
        list_->SetSortFunc(2, [](vgui2::ListPanel *, const vgui2::ListPanelItem &a, const vgui2::ListPanelItem &b) {
            const float left = a.kv->GetFloat("TimeSec");
            const float right = b.kv->GetFloat("TimeSec");
            return (left < right) - (left > right);
        });
        list_->SetSortColumn(1);
        rulesList_ = new vgui2::ListPanel(tabs_, "RulesList");
        constexpr int RuleNameWidth = 186;
        constexpr int RuleValueWidth = 150;
        rulesList_->AddColumnHeader(0, "Rule", "#CSEServerBrowser_RuleName", RuleNameWidth,
            vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW);
        rulesList_->AddColumnHeader(1, "Value", "#CSEServerBrowser_Value", RuleValueWidth,
            vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW);
        rulesList_->SetColumnSortable(0, false);
        rulesList_->SetColumnSortable(1, false);
        rulesList_->SetMultiSelectEnabled(false);
        tabs_->AddPage(list_, "#ServerBrowser_Players");
        tabs_->AddPage(rulesList_, "#CSEServerBrowser_Rules");
        LoadControlSettings("servers/DialogGameInfo.res", "PLATFORM");
        alert_->SetSelected(true);
        Refresh();
        UpdateLayoutStyle();
        UpdateText();
        UpdateVersions();
        UpdateRulesPresentation();
    }
    void RefreshLanguagePresentation() {
        UpdateLocalizedText();
        UpdateVersions();
        UpdateRulesPresentation();
        InvalidateLayout(false, false);
    }
    RowKey Key() const {
        return row_.key;
    }
    bool Closing() const {
        return closing_;
    }
    void Connect() {
        if (!closing_) {
            connectPending_ = true;
            failed_ = false;
            full_ = false;
            Refresh();
        }
    }
    void Pump() {
        if (closing_) {
            if (BeginClose()) {
                MarkForDeletion();
            }
            return;
        }
        if (UpdateLayoutStyle()) {
            UpdateText();
        }
        if (acceptedSerial_ != session_.connection.AcceptanceSerial()) {
            acceptedSerial_ = session_.connection.AcceptanceSerial();
            const auto current = session_.connection.CurrentServer();
            if (current && current->address.ip == row_.key.ip &&
                current->address.connectionPort == row_.key.connectionPort) {
                OnClose();
                return;
            }
        }
        const double now = vgui2::system()->GetCurrentTime();
        if (deadline_ > 0.0 && now >= deadline_) {
            Refresh();
        }
        if (refreshPending_) {
            if (!details_) {
                details_.reset(new ServerBrowserDetails(session_.servers));
                details_->Start(row_.key);
            } else if (!details_->RefreshMetadata()) {
                return;
            }
            refreshPending_ = false;
            deadline_ = now + GameInfoRefreshSeconds;
        }
        for (auto &metadata : details_->TakeMetadata()) {
            const bool tagsChanged = metadata.tags != row_.tags;
            const bool gameChanged = !GameDirectoriesMatch(metadata.gameDirectory, row_.gameDirectory);
            row_ = std::move(metadata);
            if (tagsChanged) {
                taggedVersions_ = ReadVersionTags(row_.tags);
                UpdateVersions();
            }
            if (gameChanged && rulesSnapshot_) {
                UpdateRulesPresentation();
            }
            responded_ = true;
            failed_ = false;
            if (connectPending_) {
                DispatchConnect();
            } else if (retry_->IsSelected() && row_.players < row_.maxPlayers) {
                vgui2::surface()->PlaySound("Servers/game_ready.wav");
                vgui2::surface()->FlashWindow(GetVPanel(), true);
                if (join_->IsSelected()) {
                    DispatchConnect();
                }
            } else {
                if (details_->StartPlayers()) {
                    // Admission retires the old display even if the new result is empty.
                    // Merely starting metadata above leaves its independent players owned.
                    clearPlayers_ = true;
                    replacementPlayers_ = true;
                    playerGeneration_ = NoPlayerGeneration;
                }
            }
            if (closing_ || !session_.active) {
                return;
            }
        }
        if (details_->MetadataFailed()) {
            connectPending_ = false;
            responded_ = false;
            failed_ = true;
        }
        for (auto &player : details_->TakeUpdates()) {
            if (replacementPlayers_) {
                clearPlayers_ = true;
                replacementPlayers_ = false;
                playerGeneration_ = player.generation;
            }
            if (clearPlayers_) {
                // Player results are drained on the same bounded pump; old display items are
                // removed incrementally below before the first replacement is applied.
                pendingPlayers_.push_back(std::move(player));
            } else {
                AddPlayer(player);
            }
        }
        if (clearPlayers_) {
            size_t budget = DetailFrameBudget;
            while (budget && list_->GetItemCount()) {
                list_->RemoveItem(list_->GetItemIDFromRow(0));
                --budget;
            }
            if (!list_->GetItemCount()) {
                clearPlayers_ = false;
            }
        }
        size_t work = 0;
        while (!clearPlayers_ && !pendingPlayers_.empty() && work++ < DetailFrameBudget) {
            if (pendingPlayers_.front().generation == playerGeneration_) {
                AddPlayer(pendingPlayers_.front());
            }
            pendingPlayers_.pop_front();
        }
        if (rulesRefreshPending_ && !connectPending_ && details_->StartRules()) {
            rulesRefreshPending_ = false;
            if (rulesSnapshot_ && !rulesSnapshot_->responded) {
                rulesSnapshot_.reset();
                UpdateVersions();
                UpdateRulesPresentation();
            }
        }
        if (auto result = details_->TakeRules()) {
            rulesSnapshot_ = std::move(*result);
            UpdateVersions();
            UpdateRulesPresentation();
        }
        UpdateText();
    }
    bool BeginClose() {
        BrowserUse use(session_);
        closing_ = true;
        connectPending_ = false;
        SetVisible(false);
        if (password_.Get()) {
            password_->Retire();
        }
        ErasePassword();
        if (!queryClosed_ && details_) {
            queryClosed_ = details_->BeginClose();
            return false;
        }
        size_t work = 0;
        while (!pendingPlayers_.empty() && work++ < DetailFrameBudget) {
            pendingPlayers_.pop_front();
        }
        while (list_->GetItemCount() && work++ < DetailFrameBudget) {
            list_->RemoveItem(list_->GetItemIDFromRow(0));
        }
        return pendingPlayers_.empty() && !list_->GetItemCount() && !password_.Get();
    }
protected:
    void PerformLayout() override {
        Frame::PerformLayout();
        if (!smokedLayout_) {
            int x = 0;
            int y = 0;
            int wide = 0;
            int tall = 0;
            tabs_->GetBounds(x, y, wide, tall);
            constexpr int OriginalVersionHeight = 20;
            constexpr int OriginalVersionGap = 6;
            const int line = GetProportionalScaledValue(OriginalVersionHeight);
            const int gap = GetProportionalScaledValue(OriginalVersionGap);
            int top = y - line * 2;
            if (auto *ping = FindChildByName("PingText", false)) {
                int left = 0;
                ping->GetPos(left, top);
                top += ping->GetTall();
            }
            const int sheetTop = top + line * 2;
            LayoutVersion(engineVersion_, x, top, wide, line, gap);
            LayoutVersion(gameDllVersion_, x, top + line, wide, line, gap);
            tabs_->SetBounds(x, sheetTop, wide, std::max(0, y + tall - sheetTop));
            return;
        }

        using vgui2::smoked::Pixels;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        GetClientArea(x, y, width, height);
        const int gap = Pixels(8);
        const int line = Pixels(22);
        const int actions = Pixels(cse::ui::ControlHeight);
        const int footer = y + height - actions;
        const auto place = [this](const char *name, int left, int top, int wide, int tall) {
            if (auto *control = FindChildByName(name, false)) {
                control->SetBounds(left, top, wide, tall);
                if (auto *label = dynamic_cast<vgui2::Label *>(control)) {
                    label->SetContentAlignment(vgui2::Label::a_west);
                }
            }
        };
        place("ServerText", x, y, width, line);
        const int leftWidth = width * 3 / 5;
        const int leftLabel = Pixels(92);
        const int rightLabel = Pixels(96);
        const int dataTop = y + line + gap;
        const auto pair = [&](const char *label, const char *value, int left, int top, int total, int labelWidth) {
            place(label, left, top, labelWidth, line);
            place(value, left + labelWidth, top, std::max(0, total - labelWidth), line);
        };
        pair("GameLabel", "GameText", x, dataTop, leftWidth - gap, leftLabel);
        pair("MapLabel", "MapText", x, dataTop + line, leftWidth - gap, leftLabel);
        pair("ServerIPLabel", "ServerAddress", x, dataTop + line * 2, leftWidth - gap, leftLabel);
        const int right = x + leftWidth;
        const int rightWidth = width - leftWidth;
        pair("PlayersLabel", "PlayersText", right, dataTop, rightWidth, rightLabel);
        pair("PingLabel", "PingText", right, dataTop + line, rightWidth, rightLabel);
        pair("Label1", "SecureText", right, dataTop + line * 2, rightWidth, rightLabel);

        const int versionTop = dataTop + line * 3;
        LayoutVersions(x, versionTop, width, line, gap);
        const int sheetTop = versionTop + line + gap;
        const int statusTop = footer - gap - line * (retryShown_ ? 3 : 1);
        tabs_->SetBounds(x, sheetTop, width, std::max(0, statusTop - gap - sheetTop));
        place("InfoLabel", x, statusTop, width, line);
        alert_->SetBounds(x, statusTop + line, width, line);
        join_->SetBounds(x, statusTop + line * 2, width, line);
        int edge = x + width;
        for (const char *name : {"Close", "Refresh", "Connect"}) {
            const int buttonWidth = Pixels(std::strcmp(name, "Connect") == 0 ? 106 : 88);
            place(name, edge - buttonWidth, footer, buttonWidth, actions);
            edge -= buttonWidth + gap;
        }
        retry_->SetBounds(x, footer, std::max(0, edge - x), actions);
    }
    void OnCommand(const char *command) override {
        BrowserUse use(session_);
        if (closing_ || session_.closing || !session_.active) {
            return;
        }
        if (std::strcmp(command, "Connect") == 0) {
            Connect();
        } else if (std::strcmp(command, "Refresh") == 0) {
            rulesRefreshPending_ = true;
            Refresh();
        } else {
            Frame::OnCommand(command);
        }
    }
    void OnClose() override {
        closing_ = true;
        connectPending_ = false;
        SetVisible(false);
    }
    MESSAGE_FUNC(OnRetryToggled, "ButtonToggled") {
        if (retryShown_ == retry_->IsSelected()) {
            return;
        }
        retryShown_ = retry_->IsSelected();
        if (smokedLayout_) {
            alert_->SetVisible(retryShown_);
            join_->SetVisible(retryShown_);
            alert_->SetSelected(true);
            retry_->SetVisible(full_ || retryShown_);
            UpdateText();
            InvalidateLayout(false, false);
            return;
        }
        int x = 0;
        int y = 0;
        GetPos(x, y);
        const int width = GetWide();
        constexpr int RetryHeight = 60;
        const int height = GetTall() + (retryShown_ ? RetryHeight : -RetryHeight);
        LoadControlSettings(retryShown_ ? "servers/DialogGameInfo_AutoRetry.res" :
            "servers/DialogGameInfo.res", "PLATFORM");
        SetBounds(x, y, width, height);
        alert_->SetSelected(true);
        retry_->SetVisible(full_ || retryShown_);
        UpdateText();
    }
private:
    struct VersionField {
        vgui2::Label *label = nullptr;
        vgui2::Label *value = nullptr;
    };

    VersionField CreateVersionField(const char *label, const char *token, const char *value) {
        return {new vgui2::Label(this, label, token), new vgui2::Label(this, value, "")};
    }

    void LayoutVersion(VersionField field, int x, int y, int wide, int tall, int gap) {
        int labelWidth = 0;
        int labelHeight = 0;
        field.label->GetContentSize(labelWidth, labelHeight);
        labelWidth = std::min(labelWidth + gap, wide / 2);
        field.label->SetBounds(x, y, labelWidth, tall);
        field.value->SetBounds(x + labelWidth, y, wide - labelWidth, tall);
    }

    void LayoutVersions(int x, int y, int wide, int tall, int gap) {
        const int half = std::max(0, (wide - gap) / 2);
        LayoutVersion(engineVersion_, x, y, half, tall, gap);
        LayoutVersion(gameDllVersion_, x + half + gap, y, half, tall, gap);
    }

    void UpdateVersions() {
        const char *status = "#CSEServerBrowser_VersionChecking";
        if (rulesSnapshot_) {
            status = rulesSnapshot_->responded ? "#CSEServerBrowser_VersionNotAdvertised" :
                "#CSEServerBrowser_VersionUnknown";
        }
        if (!taggedVersions_.engine.empty()) {
            engineVersion_.value->SetText(cse::localization::WideLiteral(taggedVersions_.engine.c_str()).c_str());
        } else if (rulesSnapshot_ && !rulesSnapshot_->versions.engine.empty()) {
            engineVersion_.value->SetText(EngineVersionText(rulesSnapshot_->versions.engine).c_str());
        } else {
            engineVersion_.value->SetText(status);
        }
        const auto &gameDll = !taggedVersions_.gameDll.empty() || !rulesSnapshot_ ? taggedVersions_.gameDll :
            rulesSnapshot_->versions.gameDll;
        if (!gameDll.empty()) {
            gameDllVersion_.value->SetText(cse::localization::WideLiteral(gameDll.c_str()).c_str());
        } else {
            gameDllVersion_.value->SetText(status);
        }
    }

    bool UpdateLayoutStyle() {
        const bool smoked = cse::ui::ControlsSmokedGlass();
        if (smokedLayout_ == smoked) {
            return false;
        }
        smokedLayout_ = smoked;
        InvalidateLayout(false, true);
        SetMenuButtonVisible(!smoked);
        address_->SetVisible(smoked);
        if (smoked) {
            using vgui2::smoked::Pixels;
            constexpr int PreferredWidth = 524;
            constexpr int PreferredHeight = 480;
            constexpr int MinimumWidth = 420;
            constexpr int MinimumHeight = 340;
            SetMinimumSize(Pixels(MinimumWidth), Pixels(MinimumHeight));
            SetSize(Pixels(PreferredWidth), Pixels(PreferredHeight));
            MoveToCenterOfScreen();
            SetTitle("#ServerBrowser_GameInfoTitle", true);
            for (const char *name : {"ServerLabel", "ServerIPText"}) {
                if (auto *control = FindChildByName(name, false)) {
                    control->SetVisible(false);
                }
            }
            SetControlString("Label1", "VAC");
            alert_->SetVisible(retryShown_);
            join_->SetVisible(retryShown_);
        } else {
            SetMinimumSize(OriginalInfoMinimumWidth, OriginalInfoMinimumHeight);
            LoadControlSettings(retryShown_ ? "servers/DialogGameInfo_AutoRetry.res" :
                "servers/DialogGameInfo.res", "PLATFORM");
            SetTitle("#ServerBrowser_GameInfoWithNameTitle", true);
            SetDialogVariable("game", row_.name.c_str());
        }
        retry_->SetVisible(full_ || retryShown_);
        InvalidateLayout(false, false);
        return true;
    }

    void Refresh() {
        refreshPending_ = true;
    }
    void ErasePassword() {
        volatile char *bytes = secret_.data();
        for (size_t index = 0; index < secret_.size(); ++index) {
            bytes[index] = 0;
        }
    }
    void DispatchConnect() {
        connectPending_ = false;
        if (row_.secure && session_.apps && session_.apps->BIsVACBanned()) {
            class Refusal final : public vgui2::Frame {
            public:
                explicit Refusal(vgui2::Panel *parent) : Frame(parent, "VACBannedConnRefusedDialog", false, false) {
                    SetDeleteSelfOnClose(true);
                    LoadControlSettings("servers/VACBannedConnRefusedDialog.res", "PLATFORM");
                }
            };
            auto *refusal = new Refusal(&parent_);
            parent_.OwnMessage(refusal);
            refusal->DoModal();
            OnClose();
            return;
        }
        if (row_.password && !secret_[0]) {
            if (!password_.Get()) {
                password_ = new BrowserPasswordDialog(this, session_, row_, [this](std::string_view value) {
                    if (!closing_ && session_.active && !session_.closing) {
                        ErasePassword();
                        std::copy_n(value.begin(), std::min(value.size(), secret_.size() - 1), secret_.begin());
                        Connect();
                    }
                });
                password_->DoModal();
            }
            return;
        }
        if (row_.players >= row_.maxPlayers) {
            full_ = true;
            retry_->SetVisible(true);
            return;
        }
        parent_.ConnectRow(row_, secret_.data());
    }
    void AddPlayer(const DetailUpdate &player) {
        auto *item = new KeyValues("player");
        item->SetString("PlayerName", player.name.c_str());
        item->SetInt("__cse_literal_PlayerName", 1);
        item->SetInt("Score", player.score);
        item->SetFloat("TimeSec", player.seconds);
        constexpr int SecondsPerMinute = 60;
        constexpr int SecondsPerHour = 3600;
        const int seconds = static_cast<int>(std::min(player.seconds, static_cast<float>(INT_MAX / 2)));
        char duration[32]{};
        if (seconds >= SecondsPerHour) {
            std::snprintf(duration, sizeof(duration), "%d:%02d:%02d", seconds / SecondsPerHour,
                seconds / SecondsPerMinute % SecondsPerMinute, seconds % SecondsPerMinute);
        } else {
            std::snprintf(duration, sizeof(duration), "%d:%02d", seconds / SecondsPerMinute, seconds % SecondsPerMinute);
        }
        item->SetString("Time", duration);
        list_->AddItem(item, 0, false, true);
        item->deleteThis();
    }
    void UpdateText() {
        const auto literal = [this](const char* name, const char* value) {
            const std::wstring text = cse::localization::WideLiteral(value);
            vgui2::Panel* control = FindChildByName(name, false);
            if (auto* label = dynamic_cast<vgui2::Label*>(control)) {
                label->SetText(text.c_str());
            } else if (auto* entry = dynamic_cast<vgui2::TextEntry*>(control)) {
                entry->SetText(text.c_str());
            }
        };
        const std::string address = ConnectionAddress(row_.key);
        literal("ServerText", row_.name.empty() ? address.c_str() : row_.name.c_str());
        literal("GameText", row_.gameDescription.c_str());
        literal("MapText", row_.map.c_str());
        literal("GameTags", row_.tags.c_str());
        SetControlString("ServerIPText", address.c_str());
        address_->SetText(address.c_str());
        SetControlString("PingText", responded_ ? std::to_string(row_.ping).c_str() : "");
        const auto humans = std::max<int64_t>(static_cast<int64_t>(row_.players) - row_.bots, 0);
        SetControlString("PlayersText", row_.maxPlayers > 0 ?
            (std::to_string(humans) + " / " + std::to_string(row_.maxPlayers)).c_str() : "");
        SetControlEnabled("Connect", row_.key.ip && row_.key.queryPort);
        UpdateLocalizedText();
    }
    void UpdateLocalizedText() {
        SetControlString("SecureText", responded_ ?
            (row_.secure ? "#ServerBrowser_Secure" : "#ServerBrowser_NotSecure") : "");
        const auto humans = std::max<int64_t>(static_cast<int64_t>(row_.players) - row_.bots, 0);
        SetControlString("InfoLabel", failed_ ? "#ServerBrowser_ServerNotResponding" :
            retry_->IsSelected() ? (join_->IsSelected() ? "#ServerBrowser_JoinWhenSlotIsFree" :
                "#ServerBrowser_AlertWhenSlotIsFree") : full_ ? "#ServerBrowser_CouldNotConnectServerFull" :
                smokedLayout_ ? "" : "#ServerBrowser_PressJoinToConnect");
        list_->SetEmptyListText(responded_ && humans == 0 ? "#ServerBrowser_ServerHasNoPlayers" :
            "#ServerBrowser_ServerNotResponding");
    }
    void UpdateRulesPresentation() {
        rulesList_->DeleteAllItems();
        if (!rulesSnapshot_) {
            rulesList_->SetEmptyListText("#CSEServerBrowser_RulesLoading");
            return;
        }
        if (!rulesSnapshot_->responded) {
            rulesList_->SetEmptyListText("#CSEServerBrowser_RulesUnavailable");
            return;
        }
        rulesList_->SetEmptyListText("#CSEServerBrowser_RulesEmpty");
        const auto addRule = [this](const char *name, const std::wstring &value) {
            if (!value.empty()) {
                auto *item = new KeyValues("rule");
                item->SetString("Rule", name);
                item->SetWString("Value", value.c_str());
                // ListPanel's column index can convert wide cells to UTF-8 before painting them.
                item->SetInt("__cse_literal_Value", 1);
                rulesList_->AddItem(item, 0, false, false);
                item->deleteThis();
            }
        };
        constexpr std::array<const char *, static_cast<size_t>(ServerRule::Count)> names{
            "#CSEServerBrowser_RuleMode", "#CSEServerBrowser_RuleRespawn", "#CSEServerBrowser_RuleAmmo",
            "#CSEServerBrowser_RuleBuyLocation", "#CSEServerBrowser_RuleFriendlyFire", "#CSEServerBrowser_RuleRoundTime",
            "#CSEServerBrowser_RuleFreezeTime", "#CSEServerBrowser_RuleBuyTime", "#CSEServerBrowser_RuleBombTimer"
        };
        for (size_t index = 0; index < names.size(); ++index) {
            const auto rule = static_cast<ServerRule>(index);
            if (auto value = rulesSnapshot_->Value(rule)) {
                addRule(names[index], RuleValue(rule, *value, row_.gameDirectory));
            }
        }
    }
    ServerBrowserDialog &parent_;
    BrowserSession &session_;
    Row row_;
    std::unique_ptr<ServerBrowserDetails> details_;
    vgui2::DHANDLE<BrowserPasswordDialog> password_;
    vgui2::Label *address_;
    VersionField engineVersion_;
    VersionField gameDllVersion_;
    DetailVersions taggedVersions_;
    vgui2::PropertySheet *tabs_;
    vgui2::ListPanel *list_;
    vgui2::ListPanel *rulesList_;
    vgui2::ToggleButton *retry_;
    vgui2::RadioButton *alert_;
    vgui2::RadioButton *join_;
    std::array<char, OriginalPasswordCapacity> secret_{};
    std::deque<DetailUpdate> pendingPlayers_;
    std::optional<DetailRules> rulesSnapshot_;
    double deadline_ = 0.0;
    uint64_t acceptedSerial_;
    // No queued player is admitted until a genuine successor callback supplies its generation.
    static constexpr uint64_t NoPlayerGeneration = 0;
    static constexpr int OriginalInfoMinimumWidth = 416;
    static constexpr int OriginalInfoMinimumHeight = 360;
    uint64_t playerGeneration_ = NoPlayerGeneration;
    bool clearPlayers_ = false;
    bool replacementPlayers_ = false;
    bool queryClosed_ = false;
    bool refreshPending_ = false;
    bool rulesRefreshPending_ = true;
    bool connectPending_ = false;
    bool responded_ = false;
    bool failed_ = false;
    bool full_ = false;
    bool retryShown_ = false;
    bool smokedLayout_ = false;
    bool closing_ = false;
};

class BrowserAddServerDialog : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserAddServerDialog, vgui2::Frame, "Frame");
public:
    BrowserAddServerDialog(ServerBrowserDialog &parent, BrowserSession &session)
        : Frame(&parent, "DialogAddServer", false, false), session_(session) {
        SetDeleteSelfOnClose(false);
        SetSizeable(false);
        SetTitle("#ServerBrowser_AddServersTitle", true);
        tabs_ = new vgui2::PropertySheet(this, "GameTabs");
        entry_ = new vgui2::TextEntry(this, "ServerNameText");
        entry_->AddActionSignalTarget(this);
        list_ = new vgui2::ListPanel(tabs_, "Servers");
        const char *names[]{"Password", "Bots", "Secure", "Players", "Name", "IPAddr", "GameDesc", "Map", "Ping"};
        const int widths[]{16, 16, 16, 60, 20, 60, 150, 80, 60};
        for (int index = 0; index < static_cast<int>(std::size(names)); ++index) {
            int flags = index < 3 ? vgui2::ListPanel::COLUMN_FIXEDSIZE | vgui2::ListPanel::COLUMN_IMAGE : 0;
            if (index == 4) {
                flags = vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW;
            } else if (index == 5) {
                flags = vgui2::ListPanel::COLUMN_HIDDEN;
            }
            const std::string token = std::string("#ServerBrowser_") +
                (index == 4 ? "Servers" : index == 5 ? "IPAddress" : index == 6 ? "Game" :
                    index == 8 ? "Latency" : names[index]);
            list_->AddColumnHeader(index, names[index], token.c_str(), widths[index], flags);
        }
        list_->SetSortColumn(8);
        list_->AddActionSignalTarget(this);
        tabs_->AddPage(list_, "#ServerBrowser_ServersRespondingLocal");
        LoadControlSettings("servers/DialogAddServer.res", "PLATFORM");
        static_cast<vgui2::Button *>(FindChildByName("SelectedOKButton", false))->SetCommand("addselected");
        SetControlEnabled("OKButton", false);
        SetControlEnabled("TestServersButton", false);
        SetControlEnabled("SelectedOKButton", false);
        tabs_->SetVisible(false);
        FindChildByName("SelectedOKButton", false)->SetVisible(false);
        SetTall(CollapsedHeight);
        entry_->RequestFocus(0);
    }
    void RefreshLanguagePresentation() {
        if (!tabs_->IsVisible()) {
            return;
        }
        std::array<wchar_t, 512> title{};
        std::array<wchar_t, 256> address{};
        vgui2::localize()->ConvertANSIToUnicode(input_.c_str(), address.data(), sizeof(address));
        auto *format = vgui2::localize()->Find("#ServerBrowser_ServersResponding");
        if (format) {
            vgui2::localize()->ConstructString(title.data(), sizeof(title), format, 1, address.data());
        }
        if (auto *tab = dynamic_cast<vgui2::Label *>(tabs_->GetActiveTab())) {
            tab->SetText(format ? title.data() : address.data());
        }
    }
    void Pump() {
        if (closing_) {
            if (BeginClose()) {
                MarkForDeletion();
            }
            return;
        }
        if (restart_) {
            if (!RetireQueries()) {
                return;
            }
            resolver_.reset(new ServerAddressResolver);
            resolver_->Start(input_);
            restart_ = false;
        }
        if (resolver_ && resolver_->Complete()) {
            auto address = resolver_->Result();
            resolver_->BeginClose();
            resolver_.reset();
            if (!address) {
                auto *error = new vgui2::MessageBox("#ServerBrowser_AddServerErrorTitle",
                    "#ServerBrowser_AddServerError", GetParent());
                static_cast<ServerBrowserDialog *>(GetParent())->OwnMessage(error);
                error->DoModal(nullptr);
                if (direct_) {
                    OnClose();
                }
                return;
            }
            if (direct_) {
                if (!address->connectionPort) {
                    address->connectionPort = DefaultServerPort;
                    address->queryPort = DefaultServerPort;
                }
                session_.AddFavorite(session_.settings.filters.app, *address);
                OnClose();
                return;
            }
            // A test searches query ports; the returned connection port may legitimately differ.
            address->connectionPort = 0;
            ping_.reset(new ServerBrowserPing(session_.servers));
            ping_->Start(*address, true);
        }
        if (ping_) {
            for (const auto &row : ping_->TakeUpdates()) {
                auto *item = new KeyValues("server");
                item->SetString("Name", row.name.c_str());
                item->SetInt("__cse_literal_Name", 1);
                item->SetInt("__cse_literal_GameDesc", 1);
                item->SetInt("__cse_literal_Map", 1);
                item->SetString("IPAddr", ConnectionAddress(row.key).c_str());
                item->SetString("GameDesc", row.gameDescription.c_str());
                item->SetString("Map", row.map.c_str());
                item->SetInt("Ping", row.ping);
                item->SetString("Players", (std::to_string(row.players) + " / " + std::to_string(row.maxPlayers)).c_str());
                item->SetInt("Password", row.password ? 1 : 0);
                item->SetInt("Bots", row.bots > 0 ? 2 : 0);
                item->SetInt("Secure", row.secure ? (session_.apps && session_.apps->BIsVACBanned() ? 4 : 3) : 0);
                const int id = list_->AddItem(item, 0, false, true);
                rows_.emplace(id, row);
                item->deleteThis();
                if (!list_->GetSelectedItemsCount()) {
                    list_->SetSingleSelectedItem(id);
                }
            }
            if (ping_->Failed()) {
                list_->SetEmptyListText("#ServerBrowser_ServerNotResponding");
            }
        }
        SetControlEnabled("SelectedOKButton", list_->GetSelectedItemsCount() > 0);
    }
    bool BeginClose() {
        closing_ = true;
        SetVisible(false);
        return RetireQueries();
    }
protected:
    void ApplySchemeSettings(vgui2::IScheme *scheme) override {
        Frame::ApplySchemeSettings(scheme);
        auto *images = new vgui2::ImageList(false);
        for (const char *name : {"servers/icon_password", "servers/icon_bots", "servers/icon_robotron",
                 "servers/icon_secure_deny", "servers/icon_password_column", "servers/icon_bots_column",
                 "servers/icon_robotron_column"}) {
            images->AddImage(vgui2::scheme()->GetImage(name, false));
        }
        list_->SetImageList(images, true);
        constexpr int PasswordHeaderImage = 5;
        constexpr int BotsHeaderImage = 6;
        constexpr int SecureHeaderImage = 7;
        list_->SetColumnHeaderImage(0, PasswordHeaderImage);
        list_->SetColumnHeaderImage(1, BotsHeaderImage);
        list_->SetColumnHeaderImage(2, SecureHeaderImage);
        list_->SetColumnHeaderTooltip(0, "#ServerBrowser_PasswordColumn_Tooltip");
        list_->SetColumnHeaderTooltip(1, "#ServerBrowser_BotColumn_Tooltip");
        list_->SetColumnHeaderTooltip(2, "#ServerBrowser_SecureColumn_Tooltip");
        auto font = scheme->GetFont("ListSmall", IsProportional());
        list_->SetFont(font ? font : scheme->GetFont("DefaultSmall", IsProportional()));
    }
    MESSAGE_FUNC(OnTextChanged, "TextChanged") {
        const bool enabled = !Text(entry_).empty();
        SetControlEnabled("OKButton", enabled);
        SetControlEnabled("TestServersButton", enabled);
    }
    void OnCommand(const char *command) override {
        BrowserUse use(session_);
        if (closing_ || session_.closing || !session_.active) {
            return;
        }
        if (std::strcmp(command, "OK") == 0 || std::strcmp(command, "TestServers") == 0) {
            input_ = Text(entry_);
            direct_ = std::strcmp(command, "OK") == 0;
            restart_ = true;
            if (!direct_) {
                list_->DeleteAllItems();
                rows_.clear();
                list_->SetEmptyListText("");
                SetTall(ExpandedHeight);
                tabs_->SetVisible(true);
                FindChildByName("SelectedOKButton", false)->SetVisible(true);
                RefreshLanguagePresentation();
            }
        } else if (std::strcmp(command, "addselected") == 0) {
            if (list_->GetSelectedItemsCount()) {
                const int id = list_->GetSelectedItem(0);
                const auto found = rows_.find(id);
                if (found != rows_.end() && session_.AddFavorite(found->second.app, found->second.key)) {
                    list_->RemoveItem(id);
                    rows_.erase(found);
                    list_->SetEmptyListText("");
                }
            }
        } else {
            Frame::OnCommand(command);
        }
    }
    void OnClose() override {
        closing_ = true;
        SetVisible(false);
    }
private:
    bool RetireQueries() {
        const bool resolverClosed = !resolver_ || resolver_->BeginClose();
        const bool pingClosed = !ping_ || ping_->BeginClose();
        if (resolverClosed && pingClosed) {
            resolver_.reset();
            ping_.reset();
            return true;
        }
        return false;
    }
    static constexpr int CollapsedHeight = 205;
    static constexpr int ExpandedHeight = 471;
    BrowserSession &session_;
    vgui2::PropertySheet *tabs_;
    vgui2::TextEntry *entry_;
    vgui2::ListPanel *list_;
    std::map<int, Row> rows_;
    std::unique_ptr<ServerAddressResolver> resolver_;
    std::unique_ptr<ServerBrowserPing> ping_;
    std::string input_;
    bool restart_ = false;
    bool direct_ = false;
    bool closing_ = false;
};

class BrowserPage final : public vgui2::PropertyPage {
public:
    BrowserPage(vgui2::Panel *parent, ServerBrowserDialog &owner, const char *name, ListKind kind)
        : PropertyPage(parent, name, true) {
        SetSize(ReferencePageWidth, ReferencePageHeight);
        list = new BrowserViewport(this, owner, kind == ListKind::History);
        scroll = new LogicalScroll(list, [&owner](const LogicalScroll *source, size_t position) {
            owner.Seek(position, source);
        });
        auto button = [&](const char *controlName) {
            auto *control = new vgui2::Button(this, controlName, "");
            control->AddActionSignalTarget(&owner);
            return control;
        };
        connect = button("ConnectButton");
        refresh = button("RefreshButton");
        quick = button("RefreshQuickButton");
        addServer = button("AddServerButton");
        addCurrent = button("AddCurrentServerButton");
        toggle = new vgui2::ToggleButton(this, "Filter", "");
        summary = new vgui2::Label(this, "FilterString", "");
        game = new vgui2::ComboBox(this, "GameFilter", 6, false);
        game->AddItem("Counter-Strike", nullptr);
        game->AddItem("Half-Life", nullptr);
        map = new vgui2::TextEntry(this, "MapFilter");
        constexpr int MapFilterCharacters = 30;
        map->SetMaximumCharCount(MapFilterCharacters);
        ping = new vgui2::ComboBox(this, "PingFilter", 6, false);
        for (const char *label : {"#ServerBrowser_All", "#ServerBrowser_LessThan50", "#ServerBrowser_LessThan100",
                 "#ServerBrowser_LessThan150", "#ServerBrowser_LessThan250", "#ServerBrowser_LessThan350",
                 "#ServerBrowser_LessThan600"}) {
            ping->AddItem(label, nullptr);
        }
        secure = new vgui2::ComboBox(this, "SecureFilter", 3, false);
        for (const char *label : {"#ServerBrowser_All", "#ServerBrowser_SecureOnly", "#ServerBrowser_InsecureOnly"}) {
            secure->AddItem(label, nullptr);
        }
        nonempty = new vgui2::CheckButton(this, "ServerEmptyFilterCheck", "");
        notFull = new vgui2::CheckButton(this, "ServerFullFilterCheck", "");
        password = new vgui2::CheckButton(this, "NoPasswordFilterCheck", "");
        account = new vgui2::CheckButton(this, "ValidSteamAccountFilterCheck", "");
        for (vgui2::Panel *control : std::initializer_list<vgui2::Panel *>{toggle, game, map, ping, secure,
                 nonempty, notFull, password, account}) {
            control->AddActionSignalTarget(&owner);
        }
        Reload(false);
    }
    void Reload(bool expanded) {
        int width = 0;
        int height = 0;
        GetSize(width, height);
        SetSize(ReferencePageWidth, ReferencePageHeight);
        LoadControlSettings(expanded ? "servers/InternetGamesPage_Filters.res" : "servers/InternetGamesPage.res",
                            "PLATFORM");
        // The expanded file's labels are absent from the collapsed file; hide them explicitly.
        for (const char *name : {"GameFilterLabel", "MapFilterLabel", "PingFilterLabel", "SecureFilterLabel"}) {
            if (auto *label = FindChildByName(name, false)) {
                label->SetVisible(expanded);
            }
        }
        toggle->SetSelected(expanded);
        game->SetEnabled(false); // The host supplies the active mod, as in UpdateGameFilter.
        connect->SetAsDefaultButton(true);
        list->RestoreListFont();
        SetSize(width, height);
        InvalidateLayout(true, false);
    }
    BrowserViewport *list = nullptr;
    LogicalScroll *scroll = nullptr;
    vgui2::Button *connect = nullptr;
    vgui2::Button *refresh = nullptr;
    vgui2::Button *quick = nullptr;
    vgui2::Button *addServer = nullptr;
    vgui2::Button *addCurrent = nullptr;
    vgui2::ToggleButton *toggle = nullptr;
    vgui2::Label *summary = nullptr;
    vgui2::ComboBox *game = nullptr;
    vgui2::TextEntry *map = nullptr;
    vgui2::ComboBox *ping = nullptr;
    vgui2::ComboBox *secure = nullptr;
    vgui2::CheckButton *nonempty = nullptr;
    vgui2::CheckButton *notFull = nullptr;
    vgui2::CheckButton *password = nullptr;
    vgui2::CheckButton *account = nullptr;

protected:
    void PerformLayout() override {
        PropertyPage::PerformLayout();
        scroll->SetBounds(std::max(0, list->GetWide() - ScrollWidth), 0, ScrollWidth, list->GetTall());
    }
};

ServerBrowserDialog::ServerBrowserDialog(BrowserSession &session)
    : Frame(nullptr, "CServerBrowserDialog", true, false), session_(session) {
    // Original resources use logical pixels; do not inherit the host's HD proportional form scaling.
    SetHDProportional(vgui2::Panel::HD_PROPORTIONAL_FORCE_DISABLED);
    SetProportional(false);
    RegisterFallbacks();
    vgui2::localize()->AddFile(vgui2::filesystem(), "servers/serverbrowser_%language%.txt");
    SetTitle("#ServerBrowser_Servers", true);
    SetDeleteSelfOnClose(false);
    SetSizeable(true);
    SetMinimumSize(OriginalMinimumWidth, OriginalMinimumHeight);
    SetSize(OriginalMinimumWidth, OriginalMinimumHeight);
    tabs_ = new vgui2::PropertySheet(this, "GameTabs");
    tabs_->SetTabWidth(72);
    status_ = new vgui2::Label(this, "StatusLabel", "");
    for (size_t index = 0; index < BrowserTabKinds.size(); ++index) {
        tabs_->AddPage(new BrowserPage(tabs_, *this, PageNames[index], BrowserTabKinds[index]), TabLabels[index]);
        tabStates_[index].filters.app = session.settings.filters.app;
        tabStates_[index].filters.gameDirectory = session.settings.filters.gameDirectory;
        const auto &stored = session.settings.pages[static_cast<size_t>(BrowserTabKinds[index])];
        if (stored.initialized) {
            tabStates_[index].filters = stored.filters;
            tabStates_[index].filters.app = session.settings.filters.app;
            tabStates_[index].filters.gameDirectory = session.settings.filters.gameDirectory;
            tabStates_[index].filtersExpanded = stored.filtersExpanded;
        }
        auto *page = static_cast<BrowserPage *>(tabs_->GetPage(static_cast<int>(index)));
        for (int column = 0; column < page->list->GetNumColumnHeaders(); ++column) {
            if (VisualColumns[static_cast<size_t>(column)] != Column::Name) {
                page->list->SetColumnVisible(column, !(stored.hiddenColumns & (1U << column)));
            }
        }
        if (BrowserTabKinds[index] == ListKind::History) {
            tabStates_[index].sort = {Column::LastPlayed, false};
        }
    }
    LoadControlSettings("servers/DialogServerBrowser.res", "PLATFORM");
    settingsButton_ = new BrowserSettingsButton(this);
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    GetDefaultScreenPosition(x, y, width, height);
    SetSize(session.settings.savedSize ? session.settings.width : width,
            session.settings.savedSize ? session.settings.height : height);
    // Clamp persisted coordinates before VPanel narrows positions to signed shorts.
    ClampFrame(*this, session.settings.savedPosition ? session.settings.x : x,
               session.settings.savedPosition ? session.settings.y : y);
    const auto saved = std::find(BrowserTabKinds.begin(), BrowserTabKinds.end(), session.settings.kind);
    const int tab = static_cast<int>(saved - BrowserTabKinds.begin());
    tabs_->SetActivePage(tabs_->GetPage(tab));
    auto &savedTab = tabStates_[static_cast<size_t>(tab)];
    savedTab.filters = session.settings.filters;
    savedTab.sort = session.settings.sort;
    savedTab.filtersExpanded = session.settings.filtersExpanded;
    BindPage();
    ReloadPage();
    RestoreFilters();
    SetVisible(false);
}

ServerBrowserDialog::~ServerBrowserDialog() = default;

bool ServerBrowserDialog::GetDefaultScreenPosition(int &x, int &y, int &width, int &height) {
    int workspaceX = 0;
    int workspaceY = 0;
    vgui2::surface()->GetWorkspaceBounds(workspaceX, workspaceY, width, height);
    constexpr double LeftFraction = 0.05;
    constexpr double TopFraction = 0.40;
    constexpr double WidthFraction = 0.50;
    constexpr double HeightFraction = 0.55;
    x = workspaceX + static_cast<int>(width * LeftFraction);
    y = workspaceY + static_cast<int>(height * TopFraction);
    width = static_cast<int>(width * WidthFraction);
    height = static_cast<int>(height * HeightFraction);
    return true;
}

void ServerBrowserDialog::BindPage() {
    page_ = static_cast<BrowserPage *>(tabs_->GetActivePage());
    list_ = page_->list;
    scroll_ = page_->scroll;
    connect_ = page_->connect;
    refresh_ = page_->refresh;
    quick_ = page_->quick;
    addServer_ = page_->addServer;
    addCurrent_ = page_->addCurrent;
    filterToggle_ = page_->toggle;
    filterSummary_ = page_->summary;
    game_ = page_->game;
    map_ = page_->map;
    ping_ = page_->ping;
    secure_ = page_->secure;
    nonempty_ = page_->nonempty;
    notFull_ = page_->notFull;
    password_ = page_->password;
    validAccount_ = page_->account;
}

void ServerBrowserDialog::ReloadPage() {
    page_->Reload(session_.settings.filtersExpanded);
    UpdateActions();
}

void ServerBrowserDialog::PerformLayout() {
    ClampFrame(*this);
    Frame::PerformLayout();
    const bool smoked = cse::ui::ControlsSmokedGlass();
    int clientX = 0;
    int clientY = 0;
    int clientWidth = 0;
    int clientHeight = 0;
    GetClientArea(clientX, clientY, clientWidth, clientHeight);
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    tabs_->GetBounds(x, y, width, height);
    const int sourceTop = y - tabsTopAdjustment_;
    const int adjustment = smoked ? clientY - sourceTop : 0;
    const int shift = adjustment - tabsTopAdjustment_;
    if (shift != 0) {
        tabs_->SetBounds(x, y + shift, width, height - shift);
        tabsTopAdjustment_ = adjustment;
    }
    constexpr int OriginalTitleButtonSize = 18;
    constexpr int OriginalTitleButtonStep = 20;
    constexpr int OriginalTitleButtonMargin = 5;
    constexpr int OriginalTitleButtonTop = 8;
    const int side = smoked ? vgui2::smoked::Pixels(cse::ui::TitleButtonSize) : OriginalTitleButtonSize;
    const int step = smoked ? side + vgui2::smoked::Pixels(cse::ui::TitleButtonGap) : OriginalTitleButtonStep;
    const int margin = smoked ? vgui2::smoked::Pixels(cse::ui::WindowInset) : OriginalTitleButtonMargin;
    const int top = smoked ? (vgui2::smoked::Pixels(cse::ui::WindowHeaderHeight) - side) / 2
        : OriginalTitleButtonTop;
    settingsButton_->SetBounds(GetWide() - margin - step - side, top, side, side);
    settingsButton_->MoveToFront();
}

void ServerBrowserDialog::Activate() {
    BrowserUse use(session_);
    if (session_.closing) {
        return;
    }
    if (deactivating_) {
        reopenRequested_ = true;
        return;
    }
    if (!session_.active) {
        session_.active = true;
        refreshOnOpen_ = true;
    }
    Frame::Activate();
}

void ServerBrowserDialog::Deactivate() {
    BrowserUse use(session_);
    reopenRequested_ = false;
    if (session_.closing || !session_.active) {
        return;
    }
    session_.active = false;
    deactivating_ = true;
    inactiveDispatchObserved_ = false;
    vgui2::ivgui()->PostMessage(GetVPanel(), new KeyValues("BrowserInactiveDispatch"), 0, 0);
    SaveSettings();
    Frame::OnClose();
    session_.model.Stop();
    internetRefreshPending_ = false;
    RetireChildren();
}

void ServerBrowserDialog::OnMessage(const KeyValues *message, vgui2::VPANEL sender) {
    if (std::strcmp(message->GetName(), "BrowserInactiveDispatch") == 0) {
        inactiveDispatchObserved_ = true;
        return;
    }
    auto *source = static_cast<vgui2::Panel *>(const_cast<KeyValues *>(message)->GetPtr("panel", nullptr));
    if (session_.active && !session_.closing && source && source->GetParent() == page_) {
        if (std::strcmp(message->GetName(), "ButtonToggled") == 0 && source == filterToggle_) {
            session_.settings.filtersExpanded = filterToggle_->IsSelected();
            ReloadPage();
            RestoreFilters();
            SaveSettings();
            return;
        }
        if (std::strcmp(message->GetName(), "TextChanged") == 0 ||
            std::strcmp(message->GetName(), "CheckButtonChecked") == 0 ||
            std::strcmp(message->GetName(), "ButtonToggled") == 0) {
            ApplyFilters();
            return;
        }
    }
    Frame::OnMessage(message, sender);
}

void ServerBrowserDialog::OnClose() {
    Deactivate();
}

void ServerBrowserDialog::RestoreFilters() {
    restoringFilters_ = true;
    const auto &filters = session_.settings.filters;
    game_->ActivateItem(filters.app == HalfLifeApp ? 1 : 0);
    game_->SetEnabled(false);
    map_->SetText(filters.map.c_str());
    const auto ping = std::find(BrowserPingLimits.begin(), BrowserPingLimits.end(), filters.maxPing);
    ping_->ActivateItem(ping == BrowserPingLimits.end() ? 0 : static_cast<int>(ping - BrowserPingLimits.begin()));
    password_->SetSelected(filters.password == Requirement::No);
    secure_->ActivateItem(static_cast<int>(filters.secure));
    nonempty_->SetSelected(filters.nonempty);
    notFull_->SetSelected(filters.notFull);
    validAccount_->SetSelected(filters.validSteamAccount);
    filterToggle_->SetSelected(session_.settings.filtersExpanded);
    restoringFilters_ = false;
    UpdateFilterSummary();
}

void ServerBrowserDialog::UpdateFilterSummary() {
    const auto &filters = session_.settings.filters;
    std::wstring summary;
    const auto localized = [](const char *token) {
        const auto *text = vgui2::localize()->Find(token);
        return text ? std::wstring(text) : std::wstring{};
    };
    if (!filters.gameDirectory.empty()) {
        std::array<wchar_t, 128> game{};
        game_->GetText(game.data(), static_cast<int>(sizeof(game)));
        summary += game.data();
        summary += L"; ";
    }
    if (filters.secure != Requirement::Any) {
        summary += localized(filters.secure == Requirement::Yes ? "#ServerBrowser_FilterDescSecureOnly" :
                             "#ServerBrowser_FilterDescInsecureOnly") + L"; ";
    }
    if (filters.maxPing) {
        summary += localized("#ServerBrowser_FilterDescLatency") + L" < " + std::to_wstring(filters.maxPing) + L"; ";
    }
    for (const auto &entry : {std::pair{filters.notFull, "#ServerBrowser_FilterDescNotFull"},
             std::pair{filters.nonempty, "#ServerBrowser_FilterDescNotEmpty"},
             std::pair{filters.password == Requirement::No, "#ServerBrowser_FilterDescNoPassword"},
             std::pair{filters.validSteamAccount, "#ServerBrowser_FilterDescValidSteamAccount"}}) {
        if (entry.first) {
            summary += localized(entry.second) + L"; ";
        }
    }
    std::array<wchar_t, 256> map{};
    vgui2::localize()->ConvertANSIToUnicode(filters.map.c_str(), map.data(), static_cast<int>(sizeof(map)));
    summary += map.data();
    filterSummary_->SetText(summary.c_str());
}

void ServerBrowserDialog::UpdateActions() {
    const bool internet = UsesMasterserver(session_.settings.kind);
    const bool favorites = session_.settings.kind == ListKind::Favorites;
    const bool refreshing = session_.model.GetStatus().phase == Phase::Refreshing;
    refresh_->SetText(refreshing ? "#ServerBrowser_StopRefreshingList" :
                      (internet ? "#ServerBrowser_RefreshAll" : "#ServerBrowser_Refresh"));
    refresh_->SetCommand(refreshing ? "stoprefresh" : "GetNewList");
    quick_->SetVisible(internet);
    quick_->SetEnabled(!refreshing && session_.presentation.VisibleCount() > 0);
    addServer_->SetVisible(favorites);
    addCurrent_->SetVisible(favorites);
    addCurrent_->SetEnabled(session_.connection.CurrentServer().has_value());
}

bool ServerBrowserDialog::ChangeTab(int tab) {
    if (tab < 0 || static_cast<size_t>(tab) >= tabStates_.size()) {
        return false;
    }
    if (activeTab_ >= 0) {
        auto &outgoing = tabStates_[static_cast<size_t>(activeTab_)];
        outgoing.filters = session_.settings.filters;
        outgoing.sort = session_.settings.sort;
        outgoing.filtersExpanded = session_.settings.filtersExpanded;
        // A bounded reset clears the live selection before its saved RowKey can be restored.
        if (!restoreSelection_) {
            outgoing.selection = session_.model.Selection();
            if (session_.settings.kind == ListKind::Internet) {
                internetSelectedKeys_ = selectedKeys_;
                internetKeyboardCursor_ = keyboardCursor_;
                internetRangeAnchor_ = rangeAnchor_;
            }
        }
    }
    const auto kind = BrowserTabKinds[static_cast<size_t>(tab)];
    const auto previousKind = activeTab_ < 0 ? ListKind::Internet : session_.settings.kind;
    const bool swapLists = (kind == ListKind::Internet) != (previousKind == ListKind::Internet);
    auto &destination = tabStates_[static_cast<size_t>(tab)];
    if (kind != ListKind::Internet && kind != ListKind::Spectate) {
        destination.filters.region = Filters{}.region;
    }
    if (swapLists) {
        // Only the Internet list is cached. Its provider request can be retired
        // independently of the copied rows, ordering, and scroll position.
        if (previousKind == ListKind::Internet) {
            session_.model.Stop();
            internetRefreshPending_ = false;
        } else {
            session_.model.ClearResults();
            session_.presentation.Apply({{}, false, true, 0});
        }
        session_.model.SwapState(session_.parkedModel);
        session_.presentation.SwapState(session_.parkedPresentation);
    }
    const double now = vgui2::system()->GetCurrentTime();
    const bool internetFresh = kind == ListKind::Internet && internetRefreshedAt_ &&
        now - *internetRefreshedAt_ < InternetCacheMaxAgeSeconds;
    const bool hasInternetRows = kind == ListKind::Internet && session_.presentation.CanAcceptUpdates() &&
        session_.presentation.VisibleCount() > 0;
    const bool accepted = internetFresh || RefreshPage(kind, destination.filters);
    const auto state = session_.model.GetStatus();
    // A provider start failure has already installed this query identity; an early refusal has not.
    if (session_.servers && !accepted && state.kind != kind) {
        if (swapLists) {
            session_.model.SwapState(session_.parkedModel);
            session_.presentation.SwapState(session_.parkedPresentation);
        }
        if (activeTab_ >= 0) {
            tabs_->SetActivePage(tabs_->GetPage(activeTab_));
        }
        RestoreFilters();
        return false;
    }
    transitioning_ = activeTab_ >= 0 && activeTab_ != tab && !hasInternetRows;
    selectedKeys_.clear();
    keyboardCursor_.reset();
    rangeAnchor_.reset();
    activeTab_ = tab;
    session_.settings.kind = kind;
    session_.settings.filters = destination.filters;
    session_.settings.sort = destination.sort;
    session_.settings.filtersExpanded = destination.filtersExpanded;
    session_.model.SetSort(destination.sort);
    session_.presentation.RequestSort(destination.sort);
    restoreSelection_ = destination.selection.has_value();
    if (hasInternetRows) {
        selectedKeys_ = internetSelectedKeys_;
        keyboardCursor_ = internetKeyboardCursor_;
        rangeAnchor_ = internetRangeAnchor_;
        ReconcileSelection();
        restoreSelection_ = restoreSelection_ && !session_.model.Selection();
    }
    list_->RemoveAll();
    slots_.clear();
    visible_.clear();
    viewportDirty_ = true;
    BindPage();
    ReloadPage();
    RestoreFilters();
    InvalidateLayout(false, false);
    return true;
}

bool ServerBrowserDialog::RefreshPage(ListKind kind, const Filters &filters, bool clearResults) {
    // A notification received inside the provider call belongs to a later refresh.
    session_.dirtySavedLists &= ~SavedListFlag(kind);
    const bool accepted = session_.model.Refresh(kind, filters, clearResults);
    if (kind == ListKind::Internet) {
        internetRefreshPending_ = accepted;
        if (!accepted) {
            internetRefreshedAt_.reset();
        }
    }
    return accepted;
}

void ServerBrowserDialog::Pump() {
    if (session_.closing || session_.useDepth) {
        return;
    }
    BrowserUse use(session_);
    if (session_.active) {
        const int tab = tabs_->GetActivePageNum();
        if (tab != activeTab_ || refreshOnOpen_) {
            refreshOnOpen_ = false;
            ChangeTab(tab);
        }
    }
    const auto kind = session_.settings.kind;
    if (session_.active && (session_.dirtySavedLists & SavedListFlag(kind)) &&
        session_.model.GetStatus().phase != Phase::Refreshing) {
        RefreshPage(kind, session_.settings.filters);
    }
    session_.masterserverProvider.Pump(BrowserFrameBudget);
    session_.parkedModel.Pump(BrowserFrameBudget);
    session_.parkedPresentation.Pump(BrowserFrameBudget);
    if (session_.parkedPresentation.CanAcceptUpdates()) {
        for (const auto &update : session_.parkedModel.TakeUpdates(BrowserFrameBudget)) {
            session_.parkedPresentation.Apply(update);
        }
    }
    session_.model.Pump(BrowserFrameBudget);
    session_.presentation.Pump(BrowserFrameBudget);
    if (session_.presentation.CanAcceptUpdates()) {
        for (const auto &update : session_.model.TakeUpdates(BrowserFrameBudget)) {
            session_.presentation.Apply(update);
            if (!update.visible && !update.reset) {
                selectedKeys_.erase(std::remove(selectedKeys_.begin(), selectedKeys_.end(), update.row.key),
                    selectedKeys_.end());
                if (keyboardCursor_ == update.row.key) {
                    keyboardCursor_.reset();
                }
                if (rangeAnchor_ == update.row.key) {
                    rangeAnchor_.reset();
                }
            }
            if (update.reset) {
                selectedKeys_.clear();
                keyboardCursor_.reset();
                rangeAnchor_.reset();
            }
        }
    }
    const auto state = session_.model.GetStatus();
    if (internetRefreshPending_ && state.phase != Phase::Refreshing) {
        if (state.phase == Phase::Complete && !state.pending && !state.viewUpdating) {
            internetRefreshedAt_ = vgui2::system()->GetCurrentTime();
            internetRefreshPending_ = false;
        } else if (state.phase != Phase::Complete) {
            if (state.phase == Phase::Failed) {
                internetRefreshedAt_.reset();
            }
            internetRefreshPending_ = false;
        }
    }
    if (!session_.active) {
        if (deactivating_) {
            const bool childrenRetired = RetireChildren();
            if (childrenRetired && state.requestRetired && inactiveDispatchObserved_) {
                deactivating_ = false;
                if (reopenRequested_) {
                    reopenRequested_ = false;
                    Activate();
                }
            }
        }
        return;
    }
    if (transitioning_ && !state.viewUpdating && !state.pending && !session_.presentation.IsUpdating()) {
        transitioning_ = false;
        viewportDirty_ = true;
    }
    if (restoreSelection_) {
        const auto selection = tabStates_[static_cast<size_t>(activeTab_)].selection;
        if (selection && session_.model.IsVisible(*selection)) {
            session_.model.Select(selection);
            selectedKeys_ = {*selection};
            keyboardCursor_ = selection;
            rangeAnchor_ = selection;
            restoreSelection_ = false;
            viewportDirty_ = true;
        }
    }
    // VGUI dispatches queued clicks after the GameUI pump, including a press and release in one frame.
    const auto selectionGesture = [](vgui2::MouseCode button) {
        auto *input = vgui2::input();
        return input->IsMouseDown(button) || input->WasMousePressed(button) || input->WasMouseDoublePressed(button);
    };
    if (!selectionGesture(vgui2::MOUSE_LEFT) && !selectionGesture(vgui2::MOUSE_RIGHT)) {
        UpdateRows();
    }
    UpdateActions();
    auto liveDialogs = detailsDialogs_;
    for (auto &dialog : liveDialogs) {
        if (dialog.Get()) {
            dialog->Pump();
        }
    }
    detailsDialogs_.erase(std::remove_if(detailsDialogs_.begin(), detailsDialogs_.end(),
        [](auto &dialog) { return !dialog.Get(); }), detailsDialogs_.end());
    if (addServerDialog_.Get()) {
        addServerDialog_->Pump();
    }
    const char *text = "";
    if (UsesMasterserver(state.kind) && state.completion == eServerFailedToRespond && !state.error.empty()) {
        text = "#ServerBrowser_MasterServerNotResponsive";
    } else if (!state.error.empty()) {
        text = state.error.c_str();
    } else if (!session_.servers || state.phase == Phase::Unavailable) {
        text = "#CSEServerBrowser_Unavailable";
    } else if (session_.presentation.IsUpdating()) {
        text = "#CSEServerBrowser_Sorting";
    } else if (state.viewUpdating) {
        text = "#CSEServerBrowser_Updating";
    } else if (state.phase == Phase::Refreshing) {
        text = "#ServerBrowser_RefreshingServerList";
    } else if (!session_.presentation.VisibleCount()) {
        text = EmptyListText(state);
    }
    // Resolve the one visible label before comparing its cache. An unchanged failure
    // must retain priority while the underlying query enters or leaves refresh/sort states.
    const char *effective = actionStatus_.empty() ? state.actionError.c_str() : actionStatus_.c_str();
    if (!*effective && !session_.config.Status().error.empty()) {
        effective = "#CSEServerBrowser_Unsaved";
    }
    if (!*effective) {
        effective = text;
    }
    if (shownStatus_ != effective) {
        shownStatus_ = effective;
        status_->SetText(effective);
        InvalidateLayout(false, false);
    }
}

void ServerBrowserDialog::UpdateRows() {
    const size_t capacity = list_->Capacity();
    const size_t revision = session_.presentation.ViewportRevision();
    const bool vacBanned = session_.apps && session_.apps->BIsVACBanned();
    if (!viewportDirty_ && capacity == displayedCapacity_ && revision == displayedViewportRevision_ &&
        vacBanned == displayedVACBan_) {
        return;
    }
    displayedCapacity_ = capacity;
    displayedViewportRevision_ = revision;
    displayedVACBan_ = vacBanned;
    viewportDirty_ = false;
    visible_ = transitioning_ ? std::vector<Row>{} : session_.presentation.ReadViewport(capacity);
    ResizeSlots(*list_, slots_, capacity);
    list_->ClearSelectedItems();
    const auto selection = session_.model.Selection();
    const Row *selectedRow = selection && session_.model.IsVisible(*selection)
        ? session_.model.Find(*selection) : nullptr;
    const bool canConnect = selectedRow && selectedRow->responded && !selectedRow->failed &&
        session_.connection.CanConnect(*selectedRow);
    if (selectedRow && !session_.connection.CanConnect(*selectedRow)) {
        actionStatus_ = "#CSEServerBrowser_CurrentGame";
    }
    for (size_t slot = 0; slot < slots_.size(); ++slot) {
        list_->SetItemVisible(slots_[slot], slot < visible_.size());
        if (slot >= visible_.size()) {
            continue;
        }
        const auto &row = visible_[slot];
        auto *item = list_->GetItem(slots_[slot]);
        item->SetString("Name", row.name.empty() ? ConnectionAddress(row.key).c_str() : row.name.c_str());
        item->SetInt("__cse_literal_Name", 1);
        item->SetInt("__cse_literal_GameDesc", row.responded ? 1 : 0);
        item->SetInt("__cse_literal_Map", 1);
        item->SetString("GameDesc", row.responded ? row.gameDescription.c_str() : "#ServerBrowser_NotResponding");
        item->SetString("Map", row.responded ? row.map.c_str() : "");
        item->SetString("IPAddr", ConnectionAddress(row.key).c_str());
        constexpr int PasswordImage = 1;
        constexpr int SecureImage = 3;
        item->SetInt("Password", row.responded && row.password ? PasswordImage : 0);
        constexpr int SecureDeniedImage = 4;
        item->SetInt("Secure", row.responded && row.secure ?
            (vacBanned ? SecureDeniedImage : SecureImage) : 0);
        item->SetString("Bots", row.responded && row.bots > 0 ? std::to_string(row.bots).c_str() : "");
        const auto humans = std::max<int64_t>(static_cast<int64_t>(row.players) - row.bots, 0);
        item->SetString("Players", row.responded
            ? (std::to_string(humans) + " / " + std::to_string(row.maxPlayers)).c_str() : "");
        item->SetString("Ping", row.responded ? std::to_string(row.ping).c_str() : "");
        std::array<char, 64> played{};
        const std::time_t playedTime = row.lastPlayed;
        if (row.lastPlayed) {
            if (const auto *local = std::localtime(&playedTime)) {
                std::strftime(played.data(), played.size(), "%a %d %b %I:%M%p", local);
            }
        }
        const size_t length = std::strlen(played.data());
        for (size_t index = length > 4 ? length - 4 : 0; index < length; ++index) {
            if (played[index] >= 'A' && played[index] <= 'Z') {
                played[index] += 'a' - 'A';
            }
        }
        item->SetString("LastPlayed", played.data());
        list_->ApplyItemChanges(slots_[slot]);
        if (std::find(selectedKeys_.begin(), selectedKeys_.end(), row.key) != selectedKeys_.end()) {
            list_->AddSelectedItem(slots_[slot]);
        }
    }
    list_->SortList();
    connect_->SetEnabled(canConnect);
    static_cast<LogicalScroll *>(scroll_)->Synchronize(session_.presentation.VisibleCount(), capacity,
                                                       session_.presentation.FirstVisibleRank());
}

size_t ServerBrowserDialog::WindowSize() const {
    return list_->Capacity();
}
void ServerBrowserDialog::ChooseSort(int column) {
    BrowserUse use(session_);
    if (session_.closing || !session_.active || column < 0 || column >= static_cast<int>(std::size(ColumnNames))) {
        return;
    }
    auto &sort = session_.settings.sort;
    const auto selectedColumn = VisualColumns[static_cast<size_t>(column)];
    const auto descendingComparison = [](Column value) {
        return value == Column::Bots || value == Column::Players || value == Column::LastPlayed;
    };
    const bool primaryAscending = sort.ascending != descendingComparison(sort.column);
    if (sort.column == selectedColumn) {
        sort.ascending = !sort.ascending;
    } else {
        sort.secondaryColumn = sort.column;
        sort.secondaryAscending = sort.ascending;
        sort.ascending = primaryAscending != descendingComparison(selectedColumn);
        sort.column = selectedColumn;
    }
    session_.model.SetSort(sort);
    session_.presentation.RequestSort(sort);
    SaveSettings();
}
void ServerBrowserDialog::Seek(size_t position, const vgui2::ScrollBar *source) {
    BrowserUse use(session_);
    if (source && (source != scroll_ || tabs_->GetActivePage() != page_)) {
        return;
    }
    if (!session_.closing && session_.active) {
        // Synchronization also queues slider signals after streaming updates adjust the anchor's rank.
        if (position != session_.presentation.FirstVisibleRank()) {
            session_.presentation.SeekForUserAction(position);
        }
        UpdateRows();
    }
}
void ServerBrowserDialog::SelectVisible(size_t slot) {
    BrowserUse use(session_);
    if (!session_.closing && session_.active && slot < visible_.size() &&
        session_.model.IsVisible(visible_[slot].key)) {
        restoreSelection_ = false;
        selectedKeys_ = {visible_[slot].key};
        viewportDirty_ = true;
        keyboardCursor_ = visible_[slot].key;
        rangeAnchor_ = visible_[slot].key;
        session_.model.Select(visible_[slot].key);
        actionStatus_.clear();
    }
}
void ServerBrowserDialog::SelectVisibleItems(const std::vector<size_t> &slots) {
    BrowserUse use(session_);
    if (session_.closing || !session_.active || transitioning_) {
        return;
    }
    viewportDirty_ = true;
    const bool extend = vgui2::input()->IsKeyDown(vgui2::KEY_LCONTROL) ||
        vgui2::input()->IsKeyDown(vgui2::KEY_RCONTROL) || vgui2::input()->IsKeyDown(vgui2::KEY_LSHIFT) ||
        vgui2::input()->IsKeyDown(vgui2::KEY_RSHIFT);
    if (!extend) {
        selectedKeys_.clear();
    } else {
        // Reconcile only the current viewport; offscreen selections retain their RowKeys.
        for (const auto &row : visible_) {
            selectedKeys_.erase(std::remove(selectedKeys_.begin(), selectedKeys_.end(), row.key), selectedKeys_.end());
        }
    }
    for (const size_t slot : slots) {
        if (slot < visible_.size() && session_.model.IsVisible(visible_[slot].key)) {
            selectedKeys_.push_back(visible_[slot].key);
        }
    }
    restoreSelection_ = false;
    session_.model.Select(selectedKeys_.empty() ? std::nullopt : std::optional<RowKey>{selectedKeys_.front()});
    keyboardCursor_ = selectedKeys_.empty() ? std::nullopt : std::optional<RowKey>{selectedKeys_.back()};
    rangeAnchor_ = session_.model.Selection();
    actionStatus_.clear();
}
void ServerBrowserDialog::MoveSelection(int delta, bool endpoint, bool page) {
    BrowserUse use(session_);
    const size_t count = session_.presentation.VisibleCount();
    if (session_.closing || !session_.active || !count) {
        return;
    }
    restoreSelection_ = false;
    const size_t capacity = WindowSize();
    const size_t firstVisible = session_.presentation.FirstVisibleRank();
    size_t rank = 0;
    const auto selected = session_.model.Selection();
    const auto cursor = keyboardCursor_ ? keyboardCursor_ : selected;
    if (cursor) {
        if (const auto cursorRank = session_.presentation.RankForUserAction(*cursor)) {
            rank = *cursorRank;
        }
    }
    long long target = static_cast<long long>(rank) + delta;
    if (endpoint) {
        target = delta < 0 ? 0 : static_cast<long long>(count - 1);
    } else if (page) {
        const auto pageStep = static_cast<long long>(capacity - 1);
        if (delta < 0) {
            target = firstVisible >= rank ? static_cast<long long>(rank) - pageStep
                                          : static_cast<long long>(firstVisible);
        } else {
            target = static_cast<long long>(firstVisible) + pageStep;
            if (target <= static_cast<long long>(rank)) {
                target = static_cast<long long>(rank) + pageStep;
            }
        }
    }
    const size_t targetRank = static_cast<size_t>(
        std::clamp(target, 0LL, static_cast<long long>(count - 1)));
    const auto targetKey = session_.presentation.KeyForUserAction(targetRank);
    if (targetKey && session_.model.IsVisible(*targetKey)) {
        const bool extend = vgui2::input()->IsKeyDown(vgui2::KEY_LSHIFT) ||
            vgui2::input()->IsKeyDown(vgui2::KEY_RSHIFT);
        const auto anchor = rangeAnchor_ ? rangeAnchor_ : selected;
        const auto anchorRank = anchor ? session_.presentation.RankForUserAction(*anchor) : std::nullopt;
        if (extend && anchorRank && !session_.model.GetStatus().viewUpdating &&
            !session_.presentation.IsUpdating()) {
            const auto keys = session_.model.OrderedKeys();
            if (keys.empty()) {
                return;
            }
            // The range is replaced, not accumulated: reversing the moving cursor shrinks it.
            // Keep the anchor first so Connect/Info retain the first-selected identity.
            selectedKeys_ = {*anchor};
            const size_t first = std::min(*anchorRank, targetRank);
            const size_t last = std::min(std::max(*anchorRank, targetRank), keys.size() - 1);
            for (size_t index = first; index <= last; ++index) {
                if (!(keys[index] == *anchor)) {
                    selectedKeys_.push_back(keys[index]);
                }
            }
            rangeAnchor_ = anchor;
            session_.model.Select(anchor);
        } else {
            selectedKeys_ = {*targetKey};
            rangeAnchor_ = targetKey;
            session_.model.Select(targetKey);
        }
        keyboardCursor_ = targetKey;
        viewportDirty_ = true;
        size_t first = firstVisible;
        if (targetRank < first) {
            first = targetRank;
        } else if (targetRank - first >= capacity) {
            first = targetRank - capacity + 1;
        }
        const size_t lastPage = count > capacity ? count - capacity : 0;
        session_.presentation.SeekForUserAction(std::min(first, lastPage));
    }
    UpdateRows();
}
void ServerBrowserDialog::ScrollViewport(int delta) {
    BrowserUse use(session_);
    if (session_.closing || !session_.active || transitioning_) {
        return;
    }
    constexpr long long WheelRows = 3;
    const size_t count = session_.presentation.VisibleCount();
    const size_t capacity = WindowSize();
    const size_t lastPage = count > capacity ? count - capacity : 0;
    const auto position = std::clamp(static_cast<long long>(session_.presentation.FirstVisibleRank()) -
        static_cast<long long>(delta) * WheelRows, 0LL, static_cast<long long>(lastPage));
    session_.presentation.SeekForUserAction(static_cast<size_t>(position));
    UpdateRows();
}

void ServerBrowserDialog::ConnectSelection() {
    BrowserUse use(session_);
    if (session_.closing || !session_.active) {
        return;
    }
    const auto selected = session_.model.Selection();
    const Row *row = selected && session_.model.IsVisible(*selected) ? session_.model.Find(*selected) : nullptr;
    if (!row) {
        return;
    }
    if (!session_.connection.CanConnect(*row)) {
        ShowActionStatus("#CSEServerBrowser_CurrentGame");
        return;
    }
    if (auto *dialog = OpenDetails()) {
        dialog->Connect();
    }
}

void ServerBrowserDialog::OwnMessage(vgui2::Frame *message) {
    messageDialogs_.emplace_back();
    messageDialogs_.back() = message;
}
void ServerBrowserDialog::ConnectRow(const Row &row, std::string_view password) {
    BrowserUse use(session_);
    if (session_.closing || !session_.active) {
        return;
    }
    const auto result = session_.connection.Begin(row, password);
    if (result == ConnectResult::Started) {
        Deactivate();
    } else {
        ShowActionStatus(result == ConnectResult::OtherGame         ? "#CSEServerBrowser_CurrentGame"
                         : result == ConnectResult::InvalidPassword ? "#CSEServerBrowser_PasswordInvalid"
                                                                    : "#ServerBrowser_ServerNotResponding");
    }
}
BrowserDetailsDialog *ServerBrowserDialog::OpenDetails() {
    const auto selected = session_.model.Selection();
    const Row *row = selected && session_.model.IsVisible(*selected) ? session_.model.Find(*selected) : nullptr;
    if (!row) {
        return nullptr;
    }
    const Row captured = *row;
    session_.model.Stop();
    internetRefreshPending_ = false;
    if (session_.closing || !session_.active) {
        return nullptr;
    }
    for (auto &dialog : detailsDialogs_) {
        if (dialog.Get() && !dialog->Closing() && dialog->Key() == captured.key) {
            dialog->Activate();
            return dialog.Get();
        }
    }
    auto *dialog = new BrowserDetailsDialog(*this, session_, captured);
    detailsDialogs_.emplace_back();
    detailsDialogs_.back() = dialog;
    dialog->Activate();
    return dialog;
}
void ServerBrowserDialog::ShowContextMenu() {
    BrowserUse use(session_);
    if (session_.closing || !session_.active) {
        return;
    }
    const bool selected = session_.model.Selection().has_value();
    const auto kind = session_.settings.kind;
    if (!selected && kind != ListKind::Favorites && kind != ListKind::History) {
        return;
    }
    if (!contextMenu_.Get()) {
        contextMenu_ = new vgui2::Menu(this, "ServerContext");
    }
    auto *menu = contextMenu_.Get();
    menu->DeleteAllItems();
    if (selected) {
        menu->AddMenuItem("Connect", "#ServerBrowser_Connect", "ConnectToServer", this, nullptr);
        menu->AddMenuItem("Details", "#ServerBrowser_GameInfoTitle", "ViewGameInfo", this, nullptr);
        menu->AddMenuItem("Refresh", "#ServerBrowser_Refresh", "RefreshServer", this, nullptr);
        if (kind != ListKind::Favorites && kind != ListKind::LAN) {
            menu->AddMenuItem("Favorite", "#ServerBrowser_AddToFavorites", "AddToFavorites", this, nullptr);
        }
        if (kind == ListKind::Favorites) {
            menu->AddMenuItem("Remove", "#ServerBrowser_RemoveServerFromFavorites", "RemoveFromFavorites", this, nullptr);
        } else if (kind == ListKind::History) {
            menu->AddMenuItem("Remove", "#ServerBrowser_RemoveServerFromHistory", "RemoveFromHistory", this, nullptr);
        }
    }
    if (kind == ListKind::Favorites) {
        menu->AddMenuItem("AddServer", "#ServerBrowser_AddServerByIP", "AddServerByName", this, nullptr);
    }
    int x = 0;
    int y = 0;
    vgui2::input()->GetCursorPos(x, y);
    menu->SetPos(x, y);
    menu->SetVisible(true);
}
void ServerBrowserDialog::ApplyFilters() {
    if (restoringFilters_ || !session_.active || session_.closing) {
        return;
    }
    restoreSelection_ = false;
    auto filters = session_.settings.filters;
    filters.name.clear();
    filters.region = -1;
    filters.map = Text(map_);
    const int ping = ping_->GetActiveItem();
    filters.maxPing = ping >= 0 && static_cast<size_t>(ping) < BrowserPingLimits.size() ? BrowserPingLimits[ping] : 0;
    filters.password = password_->IsSelected() ? Requirement::No : Requirement::Any;
    filters.secure = static_cast<Requirement>(secure_->GetActiveItem());
    filters.nonempty = nonempty_->IsSelected();
    filters.notFull = notFull_->IsSelected();
    filters.validSteamAccount = validAccount_->IsSelected();
    const auto &old = session_.settings.filters;
    if (std::tie(filters.map, filters.maxPing, filters.password, filters.secure, filters.nonempty,
                 filters.notFull, filters.validSteamAccount) ==
        std::tie(old.map, old.maxPing, old.password, old.secure, old.nonempty, old.notFull, old.validSteamAccount)) {
        return;
    }
    if (session_.settings.kind == ListKind::Internet &&
        (filters.nonempty != old.nonempty || filters.notFull != old.notFull ||
         (filters.secure == Requirement::Yes) != (old.secure == Requirement::Yes) || filters.region != old.region)) {
        internetRefreshedAt_.reset();
        internetRefreshPending_ = false;
    }
    session_.settings.filters = filters;
    session_.model.SetFilters(filters);
    ReconcileSelection();
    UpdateFilterSummary();
    SaveSettings();
}

void ServerBrowserDialog::ReconcileSelection() {
    viewportDirty_ = true;
    selectedKeys_.erase(std::remove_if(selectedKeys_.begin(), selectedKeys_.end(), [this](const RowKey &key) {
        return !session_.model.IsVisible(key);
    }), selectedKeys_.end());
    if (keyboardCursor_ && !session_.model.IsVisible(*keyboardCursor_)) {
        keyboardCursor_.reset();
    }
    if (rangeAnchor_ && !session_.model.IsVisible(*rangeAnchor_)) {
        rangeAnchor_.reset();
    }
    const auto selected = session_.model.Selection();
    if (!selected || !session_.model.IsVisible(*selected)) {
        session_.model.Select(selectedKeys_.empty() ? std::nullopt : std::optional<RowKey>{selectedKeys_.front()});
    }
}

void ServerBrowserDialog::OnCommand(const char *command) {
    BrowserUse use(session_);
    if (session_.closing || !session_.active) {
        return;
    }
    actionStatus_.clear();
    const auto selected = session_.model.Selection();
    std::vector<Row> selectedRows;
    for (const auto &key : selectedKeys_) {
        if (session_.model.IsVisible(key)) {
            if (const auto *row = session_.model.Find(key)) {
                selectedRows.push_back(*row);
            }
        }
    }
    if (selectedRows.empty() && selected && session_.model.IsVisible(*selected)) {
        if (const auto *row = session_.model.Find(*selected)) {
            selectedRows.push_back(*row);
        }
    }
    if (std::strcmp(command, "BrowserSettings") == 0) {
        if (!settingsDialog_.Get()) {
            settingsDialog_ = new BrowserSettingsDialog(this, session_,
                [this](const MasterserverSettings& settings) { ApplyMasterservers(settings); });
        }
        settingsDialog_->DoModal();
    } else if (std::strcmp(command, "GetNewList") == 0) {
        RefreshPage(session_.settings.kind, session_.settings.filters);
    } else if (std::strcmp(command, "stoprefresh") == 0) {
        session_.model.Stop();
        internetRefreshPending_ = false;
    } else if (std::strcmp(command, "refresh") == 0) {
        session_.model.QuickRefresh();
    } else if (std::strcmp(command, "QuickSelected") == 0 || std::strcmp(command, "RefreshServer") == 0) {
        for (const auto &row : selectedRows) {
            session_.model.QuickRefresh(row.key);
        }
    } else if (std::strcmp(command, "connect") == 0 || std::strcmp(command, "Connect") == 0 ||
               std::strcmp(command, "ConnectToServer") == 0) {
        ConnectSelection();
    } else if (std::strcmp(command, "Details") == 0 || std::strcmp(command, "ViewGameInfo") == 0) {
        OpenDetails();
    } else if (std::strcmp(command, "Favorite") == 0 || std::strcmp(command, "AddToFavorites") == 0) {
        for (const auto &row : selectedRows) {
            session_.AddFavorite(row.app, row.key);
        }
    } else if (std::strcmp(command, "RemoveFavorite") == 0 || std::strcmp(command, "RemoveFromFavorites") == 0) {
        for (const auto &row : selectedRows) {
            session_.model.RemoveFavorite(row.app, row.key);
        }
        RefreshPage(session_.settings.kind, session_.settings.filters);
    } else if (std::strcmp(command, "RemoveFromHistory") == 0) {
        for (auto row = selectedRows.rbegin(); row != selectedRows.rend(); ++row) {
            session_.model.RemoveHistory(row->app, row->key);
        }
        RefreshPage(session_.settings.kind, session_.settings.filters);
    } else if (std::strcmp(command, "AddServerByName") == 0) {
        if (!addServerDialog_.Get()) {
            addServerDialog_ = new BrowserAddServerDialog(*this, session_);
        }
        addServerDialog_->Activate();
    } else if (std::strcmp(command, "AddCurrentServer") == 0) {
        const auto current = session_.connection.CurrentServer();
        if (current) {
            session_.AddFavorite(current->app, current->address);
        }
    } else {
        SaveSettings();
        Frame::OnCommand(command);
    }
}
void ServerBrowserDialog::ApplyMasterservers(const MasterserverSettings& settings) {
    if (!settings.Valid() || session_.closing || !session_.active) {
        return;
    }
    const auto previous = session_.settings.masterservers.Selected();
    const auto selected = settings.Selected();
    session_.settings.masterservers = settings;
    session_.masterserverProvider.Select(selected);
    SaveSettings();
    if (previous.id == selected.id && previous.endpoints == selected.endpoints) {
        return;
    }
    internetRefreshedAt_.reset();
    internetSelectedKeys_.clear();
    internetKeyboardCursor_.reset();
    internetRangeAnchor_.reset();
    if (session_.settings.kind != ListKind::Internet) {
        session_.parkedModel.ClearResults();
        session_.parkedPresentation.Apply({{}, false, true, 0});
    }
    for (size_t index = 0; index < BrowserTabKinds.size(); ++index) {
        if (UsesMasterserver(BrowserTabKinds[index])) {
            tabStates_[index].selection.reset();
        }
    }
    if (!UsesMasterserver(session_.settings.kind)) {
        return;
    }
    selectedKeys_.clear();
    keyboardCursor_.reset();
    rangeAnchor_.reset();
    restoreSelection_ = false;
    actionStatus_.clear();
    transitioning_ = true;
    list_->RemoveAll();
    slots_.clear();
    viewportDirty_ = true;
    visible_.clear();
    RefreshPage(session_.settings.kind, session_.settings.filters, true);
}

void ServerBrowserDialog::ShowActionStatus(const char *token) {
    actionStatus_ = token;
    shownStatus_ = token;
    status_->SetText(token);
    InvalidateLayout(false, false);
}
void ServerBrowserDialog::SaveSettings() {
    GetPos(session_.settings.x, session_.settings.y);
    session_.settings.savedPosition = true;
    session_.settings.savedSize = true;
    session_.settings.width = GetWide();
    session_.settings.height = GetTall();
    if (activeTab_ >= 0) {
        auto &active = tabStates_[static_cast<size_t>(activeTab_)];
        active.filters = session_.settings.filters;
        active.filtersExpanded = session_.settings.filtersExpanded;
    }
    for (size_t index = 0; index < BrowserTabKinds.size(); ++index) {
        auto &stored = session_.settings.pages[static_cast<size_t>(BrowserTabKinds[index])];
        stored.filters = tabStates_[index].filters;
        stored.filtersExpanded = tabStates_[index].filtersExpanded;
        stored.initialized = true;
        auto *page = static_cast<BrowserPage *>(tabs_->GetPage(static_cast<int>(index)));
        stored.hiddenColumns = page->list->HiddenColumns();
    }
    session_.config.SetSettings(session_.settings);
    session_.config.Save();
}
bool ServerBrowserDialog::RetireChildren() {
    if (contextMenu_.Get()) {
        contextMenu_->SetVisible(false);
        contextMenu_->MarkForDeletion();
    }
    bool retired = !contextMenu_.Get();
    if (settingsDialog_.Get()) {
        settingsDialog_->Retire();
        retired = false;
    }
    for (auto &message : messageDialogs_) {
        if (message.Get()) {
            message->SetVisible(false);
            message->MarkForDeletion();
            retired = false;
        }
    }
    for (auto &dialog : detailsDialogs_) {
        if (dialog.Get()) {
            if (dialog->BeginClose()) {
                dialog->MarkForDeletion();
            }
            retired = false;
        }
    }
    if (addServerDialog_.Get()) {
        if (addServerDialog_->BeginClose()) {
            addServerDialog_->MarkForDeletion();
        }
        retired = false;
    }
    return retired;
}

bool ServerBrowserDialog::BeginClose() {
    session_.active = false;
    SetVisible(false);
    return RetireChildren();
}

}

namespace cse::server_browser {
void ServerBrowserDialog::RefreshLanguagePresentation() {
    // Reformat presentation from retained settings only. Do not touch queries,
    // selected row keys, logical viewport, pending connection or typed filters.
    shownStatus_.clear();
    viewportDirty_ = true;
    UpdateFilterSummary();
    for (auto& handle : detailsDialogs_) {
        if (auto* details = handle.Get()) {
            details->RefreshLanguagePresentation();
        }
    }
    if (auto* add = addServerDialog_.Get()) {
        add->RefreshLanguagePresentation();
    }
}
}
