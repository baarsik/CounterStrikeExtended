#include "ServerBrowserDialog.h"
#include "Button.h"
#include "CheckButton.h"
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
#include <functional>

namespace cse::server_browser {
namespace {
constexpr int Margin = 8;
constexpr int ControlHeight = 24;
constexpr int ColumnWidth = 90;
constexpr int NameColumnWidth = 200;
constexpr int ScrollWidth = 18;
constexpr AppId_t CounterStrikeApp = 10;
constexpr AppId_t HalfLifeApp = 70;
const char *const TabLabels[]{"#ServerBrowser_InternetTab", "#ServerBrowser_FavoritesTab",
                              "#ServerBrowser_HistoryTab",  "#ServerBrowser_LanTab",
                              "#ServerBrowser_FriendsTab",  "#ServerBrowser_SpectateTab"};
const char *const ColumnNames[]{"password", "secure", "name", "game", "map", "players", "ping", "address"};
const char *const ColumnLabels[]{"#ServerBrowser_PasswordLabel", "#ServerBrowser_ValidSteamAccount",
                                 "#ServerBrowser_ServerLabel",   "#ServerBrowser_Game",
                                 "#ServerBrowser_Map",           "#ServerBrowser_Players",
                                 "#ServerBrowser_Latency",       "#ServerBrowser_IPAddressLabel"};
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
class LogicalScroll final : public vgui2::ScrollBar {
public:
    LogicalScroll(vgui2::Panel *parent, std::function<void(size_t)> seek)
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
        if (!updating_) {
            seek_(static_cast<size_t>(std::max(position, 0)));
        }
    }

private:
    std::function<void(size_t)> seek_;
    bool updating_ = false;
};
struct Extent {
    int width;
    int height;
};
Extent LabelExtent(vgui2::Label &label) {
    Extent extent{};
    label.GetContentSize(extent.width, extent.height);
    extent.height = std::max(extent.height, vgui2::surface()->GetFontTall(label.GetFont()));
    return extent;
}
int Padding(vgui2::Panel &panel) {
    return std::max(Margin, panel.GetProportionalScaledValue(Margin));
}
void ClampFrame(vgui2::Frame &frame) {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    vgui2::surface()->GetWorkspaceBounds(x, y, width, height);
    if (width <= 0 || height <= 0) {
        vgui2::surface()->GetScreenSize(width, height);
    }
    constexpr int MinimumWidth = 640;
    constexpr int MinimumHeight = 480;
    const int minimumWidth = std::min(width, frame.GetProportionalScaledValue(MinimumWidth));
    const int minimumHeight = std::min(height, frame.GetProportionalScaledValue(MinimumHeight));
    frame.SetMinimumSize(minimumWidth, minimumHeight);
    frame.SetSize(std::clamp(frame.GetWide(), minimumWidth, width), std::clamp(frame.GetTall(), minimumHeight, height));
    int left = 0;
    int top = 0;
    frame.GetPos(left, top);
    frame.SetPos(std::clamp(left, x, x + width - frame.GetWide()), std::clamp(top, y, y + height - frame.GetTall()));
}
class BrowserSheet final : public vgui2::PropertySheet {
public:
    explicit BrowserSheet(vgui2::Panel *parent) : PropertySheet(parent, "GameTabs") {}

    void AddPage(vgui2::Panel *page, const char *title) override {
        const int previousCount = GetNumPages();
        PropertySheet::AddPage(page, title);
        if (GetNumPages() == previousCount) {
            return;
        }
        // Capture each of the six tabs when its logical page is added. Selecting
        // a page changes child stacking order but must not change visual order.
        for (int index = 0; index < GetChildCount(); ++index) {
            auto *tab = dynamic_cast<vgui2::Label *>(GetChild(index));
            if (tab && !std::strcmp(tab->GetName(), "tab") &&
                std::find(tabs_.begin(), tabs_.end(), tab) == tabs_.end()) {
                tabs_.push_back(tab);
                break;
            }
        }
    }

    int TabHeight(int width) {
        int x = 0;
        int y = 0;
        int rowHeight = 0;
        const int gap = Padding(*this);
        // Exactly the six browser tabs, independent of the number of server rows.
        for (auto *tab : tabs_) {
            const auto size = LabelExtent(*tab);
            const int tabWidth = size.width + 2 * gap;
            const int tabHeight = size.height + gap;
            if (x && x + tabWidth > width) {
                x = 0;
                y += rowHeight + 1;
                rowHeight = 0;
            }
            tab->SetBounds(x, y, tabWidth, tabHeight);
            x += tabWidth + 1;
            rowHeight = std::max(rowHeight, tabHeight);
        }
        return y + rowHeight + gap;
    }
    int MinimumWidth() {
        int width = 0;
        for (auto *tab : tabs_) {
            width = std::max(width, LabelExtent(*tab).width + 2 * Padding(*this));
        }
        return width;
    }

protected:
    void PerformLayout() override {
        PropertySheet::PerformLayout();
        const int top = TabHeight(GetWide());
        if (auto *page = GetActivePage()) {
            page->SetBounds(0, top, GetWide(), std::max(0, GetTall() - top));
        }
    }

private:
    std::vector<vgui2::Label *> tabs_;
};

class SlotList : public vgui2::ListPanel {
public:
    SlotList(vgui2::Panel *parent, const char *name) : ListPanel(parent, name) {
        SetMultiSelectEnabled(false);
        // Every column compares the canonical viewport slot; ListPanel otherwise leaves sort indices unset.
        constexpr int SlotSortColumn = 0;
        SetSortColumn(SlotSortColumn);
    }
    void AddMeasuredColumn(int index, const char *name, const char *text, int width) {
        AddColumnHeader(index, name, text, width, 0);
        headers_.push_back(static_cast<vgui2::Label *>(FindChildByName(name, false)));
        widths_.push_back(width);
    }
    int MinimumWidth() {
        int width = 2 * Padding(*this);
        for (size_t index = 0; index < headers_.size(); ++index) {
            width += std::max(GetProportionalScaledValue(widths_[index]),
                              LabelExtent(*headers_[index]).width + 2 * Padding(*this));
        }
        return width;
    }
    size_t Capacity() {
        return static_cast<size_t>(std::max(1.0f, std::floor(GetRowsPerPage())));
    }

protected:
    int GetStartItem() override {
        return 0;
    }
    void PerformLayout() override {
        int headerHeight = 0;
        for (auto *header : headers_) {
            headerHeight = std::max(headerHeight, LabelExtent(*header).height + Padding(*this));
        }
        SetColumnHeaderHeight(headerHeight);
        ListPanel::PerformLayout();
        int left = 0;
        for (size_t index = 0; index < headers_.size(); ++index) {
            const int width = std::max(GetProportionalScaledValue(widths_[index]),
                                       LabelExtent(*headers_[index]).width + 2 * Padding(*this));
            headers_[index]->SetBounds(left, 0, width, headerHeight);
            left += width;
        }
        if (auto *internal = dynamic_cast<vgui2::ScrollBar *>(FindChildByName("VertScrollBar", false))) {
            internal->SetVisible(false);
            // Slots are already the visible window. The retained internal bar is
            // unused, including when a scheme reload explicitly lays out hidden children.
            constexpr int InactiveScrollExtent = 1;
            internal->SetRange(0, InactiveScrollExtent);
            internal->SetRangeWindow(0);
            internal->SetValue(0);
        }
    }

private:
    std::vector<vgui2::Label *> headers_;
    std::vector<int> widths_;
};
void AddFallback(const char *token, const wchar_t *text) {
    if (!vgui2::localize()->Find(token)) {
        // ILocalize copies both token and value into its own symbol/string tables.
        vgui2::localize()->AddString(token, const_cast<wchar_t *>(text), "CSEServerBrowser");
    }
}
void RegisterFallbacks() {
    AddFallback("CSEServerBrowser_CurrentGame", L"Connect is available for the current game only.");
    AddFallback("CSEServerBrowser_Updating", L"Updating list...");
    AddFallback("CSEServerBrowser_Sorting", L"Sorting...");
    AddFallback("CSEServerBrowser_Unsaved", L"Could not save settings.");
    AddFallback("CSEServerBrowser_Unavailable", L"Steam server browsing is unavailable.");
    AddFallback("CSEServerBrowser_PasswordInvalid", L"Enter a valid server password.");
    AddFallback("CSEServerBrowser_Name", L"Server name");
    AddFallback("CSEServerBrowser_Secure", L"Secure");
    AddFallback("CSEServerBrowser_Yes", L"Yes");
    AddFallback("CSEServerBrowser_No", L"No");
    AddFallback("CSEServerBrowser_Apply", L"Apply filters");
    AddFallback("CSEServerBrowser_Rules", L"Rules");
    AddFallback("CSEServerBrowser_Value", L"Value");
    AddFallback("CSEServerBrowser_DetailsFailed", L"Some server details could not be retrieved.");
}
}

// Only browser-owned fixed controls and the viewport-sized table live here.
// Scrolling moves their parent; it never creates, measures or visits model rows.
class BrowserOverflow final : public vgui2::EditablePanel {
public:
    BrowserOverflow(vgui2::Panel *parent, const char *name) : EditablePanel(parent, name) {
        SetTabPosition(1);
        content_ = new vgui2::EditablePanel(this, "Contents");
        content_->SetTabPosition(1);
        horizontal_ = new Bar(this, "HorizontalOverflow", false);
        vertical_ = new Bar(this, "VerticalOverflow", true);
        horizontal_->SetTabPosition(2);
        vertical_->SetTabPosition(3);
    }
    vgui2::Panel *Contents() const {
        return content_;
    }
    int ViewWidth() {
        return std::max(1, GetWide() - GetProportionalScaledValue(ScrollWidth));
    }
    int ViewHeight() {
        return std::max(1, GetTall() - GetProportionalScaledValue(ScrollWidth));
    }
    void SetContentSize(int width, int height) {
        content_->SetSize(std::max(width, ViewWidth()), std::max(height, ViewHeight()));
        PerformLayout();
    }
    void OnThink() override {
        Panel::OnThink();
        const auto focus = vgui2::input()->GetFocus();
        if (!focus) {
            lastFocus_ = 0;
            return;
        }
        auto *panel = vgui2::ipanel()->GetPanel(focus, vgui2::GetControlsModuleName());
        if (!panel || panel == content_ || !panel->HasParent(content_->GetVPanel()) || panel->IsPopup()) {
            lastFocus_ = 0;
            return;
        }
        int left = 0;
        int top = 0;
        panel->LocalToScreen(left, top);
        content_->ScreenToLocal(left, top);
        const std::array<int, 6> geometry{left, top, panel->GetWide(), panel->GetTall(), ViewWidth(), ViewHeight()};
        if (focus == lastFocus_ && geometry == lastFocusGeometry_) {
            return;
        }
        // Retained focus may move during resize/scheme layout. Measure the actual
        // descendant, including nested table scrolling, without visiting rows.
        // Unchanged geometry leaves deliberate user scrolling alone.
        lastFocus_ = focus;
        lastFocusGeometry_ = geometry;
        Reveal(*horizontal_, left, panel->GetWide(), ViewWidth());
        Reveal(*vertical_, top, panel->GetTall(), ViewHeight());
    }

protected:
    void PerformLayout() override {
        const int bar = GetProportionalScaledValue(ScrollWidth);
        const int width = ViewWidth();
        const int height = ViewHeight();
        horizontal_->SetBounds(0, height, width, bar);
        vertical_->SetBounds(width, 0, bar, height);
        horizontal_->SetRange(0, content_->GetWide());
        // An inactive retained slider uses a zero window; range == window would
        // divide by zero in its knob calculation even while the control is disabled.
        horizontal_->SetRangeWindow(content_->GetWide() > width ? width : 0);
        vertical_->SetRange(0, content_->GetTall());
        vertical_->SetRangeWindow(content_->GetTall() > height ? height : 0);
        horizontal_->SetEnabled(content_->GetWide() > width);
        vertical_->SetEnabled(content_->GetTall() > height);
        // Zero-window sliders do not initialize a drawable knob. Hide inactive
        // bars while retaining the same reserved viewport geometry.
        horizontal_->SetVisible(horizontal_->IsEnabled());
        vertical_->SetVisible(vertical_->IsEnabled());
        horizontal_->SetValue(std::min(horizontal_->GetValue(), std::max(0, content_->GetWide() - width)));
        vertical_->SetValue(std::min(vertical_->GetValue(), std::max(0, content_->GetTall() - height)));
        Position();
    }
    void OnMouseWheeled(int delta) override {
        if (vertical_->IsEnabled()) {
            vertical_->SetValue(vertical_->GetValue() - delta * GetProportionalScaledValue(ControlHeight));
        }
    }

private:
    class Bar final : public vgui2::ScrollBar {
    public:
        Bar(BrowserOverflow *parent, const char *name, bool vertical)
            : ScrollBar(parent, name, vertical), owner_(*parent) {}

    protected:
        void SendSliderMoveMessage(int) override {
            owner_.Position();
        }
        void OnKeyCodeTyped(vgui2::KeyCode code) override {
            if (!IsEnabled()) {
                return;
            }
            const int step = GetProportionalScaledValue(ControlHeight);
            switch (code) {
            case vgui2::KEY_LEFT:
            case vgui2::KEY_UP:
                SetValue(GetValue() - step);
                break;
            case vgui2::KEY_RIGHT:
            case vgui2::KEY_DOWN:
                SetValue(GetValue() + step);
                break;
            case vgui2::KEY_PAGEUP:
                SetValue(GetValue() - GetRangeWindow());
                break;
            case vgui2::KEY_PAGEDOWN:
                SetValue(GetValue() + GetRangeWindow());
                break;
            case vgui2::KEY_HOME:
                SetValue(0);
                break;
            case vgui2::KEY_END: {
                int minimum = 0;
                int maximum = 0;
                GetRange(minimum, maximum);
                SetValue(maximum);
                break;
            }
            default:
                ScrollBar::OnKeyCodeTyped(code);
                break;
            }
        }

    private:
        BrowserOverflow &owner_;
    };
    static void Reveal(Bar &bar, int start, int extent, int window) {
        if (!bar.IsEnabled()) {
            return;
        }
        if (start < bar.GetValue()) {
            bar.SetValue(start);
        } else if (start + extent > bar.GetValue() + window) {
            bar.SetValue(std::max(start, start + extent - window));
        }
    }
    void Position() {
        if (content_ && horizontal_ && vertical_) {
            content_->SetPos(-horizontal_->GetValue(), -vertical_->GetValue());
        }
    }
    vgui2::Panel *content_ = nullptr;
    Bar *horizontal_ = nullptr;
    Bar *vertical_ = nullptr;
    vgui2::VPANEL lastFocus_ = 0;
    std::array<int, 6> lastFocusGeometry_{};
};

BrowserSession::BrowserSession(ISteamMatchmakingServers *serverProvider, ISteamMatchmaking *favoriteProvider,
                               IFileSystem *filesystem, IConnectionEngine &engine, AppId_t app, const char *game,
                               std::string preferenceRoot, IFavoriteStore *injectedFavorites)
    : favorites(favoriteProvider), model({serverProvider, injectedFavorites ? injectedFavorites : &favorites}),
      storage(filesystem, std::move(preferenceRoot)), config(storage), connection(engine, model, app, game),
      servers(serverProvider) {
    config.Load();
    settings = config.Settings();
    // Every session initially browses the running game. Saved remaining filters are retained.
    settings.filters.app = app;
    settings.filters.gameDirectory = game;
    model.SetSort(settings.sort);
    presentation.RequestSort(settings.sort);
}

class BrowserViewport : public SlotList {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserViewport, SlotList, "ListPanel");

public:
    BrowserViewport(vgui2::Panel *parent, ServerBrowserDialog &owner) : SlotList(parent, "ServerList"), owner_(owner) {
        for (size_t index = 0; index < std::size(ColumnNames); ++index) {
            AddMeasuredColumn(static_cast<int>(index), ColumnNames[index], ColumnLabels[index],
                              index == static_cast<size_t>(Column::Name) ? NameColumnWidth : ColumnWidth);
            SetSortFunc(static_cast<int>(index), SlotOrder);
        }
    }

protected:
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
        constexpr int WheelRows = 3;
        owner_.MoveSelection(-delta * WheelRows);
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
            owner_.MoveSelection(-static_cast<int>(owner_.WindowSize()));
            break;
        case vgui2::KEY_PAGEDOWN:
            owner_.MoveSelection(static_cast<int>(owner_.WindowSize()));
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
        if (GetSelectedItemsCount()) {
            owner_.SelectVisible(static_cast<size_t>(GetItem(GetSelectedItem(0))->GetInt("slot")));
        }
    }
    ServerBrowserDialog &owner_;
};

class BrowserPasswordDialog final : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserPasswordDialog, vgui2::Frame, "Frame");

public:
    BrowserPasswordDialog(ServerBrowserDialog &parent, BrowserSession &session, Row row)
        : Frame(&parent, "DialogServerPassword", false, false), parent_(parent), session_(session),
          row_(std::move(row)) {
        SetDeleteSelfOnClose(true);
        entry_ = new vgui2::TextEntry(this, "PasswordEntry");
        LoadControlSettings("servers/DialogServerPassword.res", "PLATFORM");
        entry_->SetTextHidden(true);
        entry_->SetMaximumCharCount(static_cast<int>(ServerPasswordCapacity - 1));
        entry_->SetAllowNonAsciiCharacters(true);
        SetControlString("GameLabel", row_.name.c_str());
    }
    void Retire() {
        if (retiring_) {
            return;
        }
        retiring_ = true;
        Clear();
        Frame::OnClose();
    }
    void Clear() {
        // The retained control keeps capacity after SetText/SaveUndoState. Overwrite both bounded streams first.
        std::array<wchar_t, ServerPasswordCapacity> blank{};
        std::fill(blank.begin(), blank.end() - 1, L' ');
        entry_->SetText(blank.data());
        entry_->SaveUndoState();
        entry_->SetText(L"");
        entry_->SaveUndoState();
    }

protected:
    void OnCommand(const char *command) override {
        BrowserUse use(session_);
        if (session_.closing || !session_.active || retiring_) {
            Clear();
            return;
        }
        if (std::strcmp(command, "Connect") == 0) {
            std::array<char, ServerPasswordCapacity> buffer{};
            entry_->GetText(buffer.data(), static_cast<int>(buffer.size()));
            parent_.ConnectRow(row_, buffer.data());
            volatile char *erase = buffer.data();
            for (size_t index = 0; index < buffer.size(); ++index) {
                erase[index] = 0;
            }
            Clear();
            Close();
            return;
        }
        Clear();
        Frame::OnCommand(command);
    }
    void OnClose() override {
        Clear();
        Frame::OnClose();
    }

private:
    ServerBrowserDialog &parent_;
    BrowserSession &session_;
    Row row_;
    vgui2::TextEntry *entry_;
    bool retiring_ = false;
};

class BrowserDetailsDialog final : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserDetailsDialog, vgui2::Frame, "Frame");

public:
    BrowserDetailsDialog(ServerBrowserDialog &parent, BrowserSession &session, const Row &row)
        : Frame(&parent, "DialogGameInfo", false, false), session_(session), details_(session.servers) {
        SetHDProportional(true);
        constexpr int DetailsWidth = 600;
        constexpr int DetailsHeight = 440;
        SetSize(DetailsWidth, DetailsHeight);
        SetTitle("#ServerBrowser_GameInfoTitle", true);
        SetDeleteSelfOnClose(false);
        body_ = new BrowserOverflow(this, "DetailsOverflow");
        info_ = new vgui2::Label(body_->Contents(), "ServerInfo", row.name.c_str());
        endpoint_ = new vgui2::Label(body_->Contents(), "Endpoint", ConnectionAddress(row.key).c_str());
        table_ = new BrowserOverflow(body_->Contents(), "DetailsTableOverflow");
        list_ = new SlotList(table_->Contents(), "DetailsList");
        list_->AddMeasuredColumn(0, "kind", "#ServerBrowser_GameInfoTitle", ColumnWidth);
        list_->AddMeasuredColumn(1, "name", "#ServerBrowser_Players", NameColumnWidth);
        list_->AddMeasuredColumn(2, "value", "#CSEServerBrowser_Value", NameColumnWidth);
        for (int index = 0; index < 3; ++index) {
            list_->SetSortFunc(index, SlotOrder);
            list_->SetColumnSortable(index, false);
        }
        scroll_ = new LogicalScroll(body_->Contents(), [this](size_t position) { first_ = position; });
        status_ = new vgui2::Label(body_->Contents(), "Status", "#ServerBrowser_RefreshingServerList");
        details_.Start(row.key);
    }
    void Pump() {
        if (closing_) {
            if (BeginClose()) {
                MarkForDeletion();
            }
            return;
        }
        for (auto &update : details_.TakeUpdates()) {
            rows_.emplace(next_++, std::move(update));
        }
        const size_t window = list_->Capacity();
        ResizeSlots(*list_, slots_, window);
        first_ = std::min(first_, rows_.size() > window ? rows_.size() - window : 0);
        auto cursor = rows_.lower_bound(first_);
        for (size_t slot = 0; slot < slots_.size(); ++slot) {
            const bool visible = cursor != rows_.end();
            list_->SetItemVisible(slots_[slot], visible);
            if (!visible) {
                continue;
            }
            auto *item = list_->GetItem(slots_[slot]);
            const auto &row = cursor++->second;
            item->SetString("kind",
                            row.kind == DetailKind::Player ? "#ServerBrowser_Players" : "#CSEServerBrowser_Rules");
            item->SetString("name", row.name.c_str());
            const std::string value =
                row.kind == DetailKind::Player
                    ? std::to_string(row.score) + " / " +
                          std::to_string(static_cast<int>(std::min(row.seconds, static_cast<float>(INT_MAX / 2)))) +
                          " s"
                    : row.value;
            item->SetString("value", value.c_str());
            list_->ApplyItemChanges(slots_[slot]);
        }
        list_->SortList();
        scroll_->Synchronize(rows_.size(), window, first_);
        const auto status = details_.Status();
        const char *text = status.playersFailed || status.rulesFailed       ? "#CSEServerBrowser_DetailsFailed"
                           : status.playersComplete && status.rulesComplete ? ""
                                                                            : "#ServerBrowser_RefreshingServerList";
        if (shownStatus_ != text) {
            shownStatus_ = text;
            status_->SetText(text);
            InvalidateLayout(false, false);
        }
    }
    bool BeginClose() {
        BrowserUse use(session_);
        closing_ = true;
        SetVisible(false);
        if (!queryClosed_) {
            // The query queue and copied display rows share one detail reclamation budget.
            queryClosed_ = details_.BeginClose();
            return false;
        }
        size_t work = 0;
        while (!rows_.empty() && work < DetailFrameBudget) {
            rows_.erase(rows_.begin());
            ++work;
        }
        return rows_.empty();
    }

protected:
    void OnClose() override {
        closing_ = true;
        SetVisible(false);
    }
    void PerformLayout() override {
        ClampFrame(*this);
        Frame::PerformLayout();
        int left = 0;
        int top = 0;
        int width = 0;
        int height = 0;
        GetClientArea(left, top, width, height);
        const int gap = Padding(*this);
        body_->SetBounds(left + gap, top + gap, std::max(1, width - 2 * gap), std::max(1, height - 2 * gap));
        const auto info = LabelExtent(*info_);
        const auto endpoint = LabelExtent(*endpoint_);
        const auto status = LabelExtent(*status_);
        width = std::max({body_->ViewWidth(), info.width, endpoint.width, status.width});
        info_->SetBounds(0, 0, width, info.height + gap);
        endpoint_->SetBounds(0, info.height + gap, width, endpoint.height + gap);
        const int tableTop = info.height + endpoint.height + 2 * gap;
        constexpr int MinimumTableRows = 6;
        const int tableHeight =
            std::max(MinimumTableRows * (status.height + gap), body_->ViewHeight() - tableTop - status.height - gap);
        const int bar = GetProportionalScaledValue(ScrollWidth);
        table_->SetBounds(0, tableTop, std::max(1, width - bar), tableHeight);
        const int listWidth = std::max(table_->ViewWidth(), list_->MinimumWidth());
        list_->SetBounds(0, 0, listWidth, table_->ViewHeight());
        list_->InvalidateLayout(true, false);
        table_->SetContentSize(listWidth, table_->ViewHeight());
        scroll_->SetBounds(width - bar, tableTop, bar, tableHeight);
        status_->SetBounds(0, tableTop + tableHeight, width, status.height + gap);
        body_->SetContentSize(width, tableTop + tableHeight + status.height + gap);
    }

private:
    BrowserSession &session_;
    ServerBrowserDetails details_;
    BrowserOverflow *body_;
    BrowserOverflow *table_;
    vgui2::Label *info_;
    vgui2::Label *endpoint_;
    std::string shownStatus_;
    SlotList *list_;
    LogicalScroll *scroll_;
    vgui2::Label *status_;
    std::map<size_t, DetailUpdate> rows_;
    std::vector<int> slots_;
    size_t first_ = 0;
    size_t next_ = 0;
    bool closing_ = false;
    bool queryClosed_ = false;
};

ServerBrowserDialog::ServerBrowserDialog(BrowserSession &session)
    : Frame(nullptr, "CServerBrowserDialog", true, false), session_(session) {
    SetHDProportional(true);
    RegisterFallbacks();
    vgui2::localize()->AddFile(vgui2::filesystem(), "servers/serverbrowser_%language%.txt");
    SetTitle("#ServerBrowser_Servers", true);
    SetDeleteSelfOnClose(false);
    SetSizeable(true);
    body_ = new BrowserOverflow(this, "BrowserOverflow");
    auto *contents = body_->Contents();
    tabs_ = new BrowserSheet(this);
    status_ = new vgui2::Label(this, "StatusLabel", "");
    actionStatusLabel_ = new vgui2::Label(this, "ActionStatusLabel", "");
    LoadControlSettings("servers/DialogServerBrowser.res", "PLATFORM");
    tabs_->SetParent(contents);
    status_->SetParent(contents);
    actionStatusLabel_->SetParent(contents);
    for (auto *control : {static_cast<vgui2::Panel *>(tabs_), static_cast<vgui2::Panel *>(status_),
                          static_cast<vgui2::Panel *>(actionStatusLabel_)}) {
        control->SetAutoResize(vgui2::Panel::AUTORESIZE_NO);
        control->SetPinCorner(vgui2::Panel::PIN_TOPLEFT);
    }
    // Browser preferences have always stored physical pixels, never proportional units.
    SetSize(session.settings.width, session.settings.height);
    ClampFrame(*this);
    for (const char *label : TabLabels) {
        tabs_->AddPage(new vgui2::PropertyPage(tabs_, label, false), label);
    }
    tabs_->SetActivePage(tabs_->GetPage(static_cast<int>(session.settings.kind)));
    table_ = new BrowserOverflow(tabs_->GetActivePage(), "TableOverflow");
    list_ = new BrowserViewport(table_->Contents(), *this);
    scroll_ = new LogicalScroll(tabs_->GetActivePage(), [this](size_t position) { Seek(position); });
    auto addFilter = [this](const char *name, const char *label, vgui2::Panel *control) {
        filterControls_.push_back(new vgui2::Label(body_->Contents(), name, label));
        filterControls_.push_back(control);
    };
    game_ = new vgui2::ComboBox(body_->Contents(), "GameFilter", 2, false);
    game_->AddItem("Counter-Strike", nullptr);
    game_->AddItem("Half-Life", nullptr);
    game_->ActivateItem(session.settings.filters.app == HalfLifeApp ? 1 : 0);
    addFilter("GameLabel", "#ServerBrowser_Game", game_);
    name_ = new vgui2::TextEntry(contents, "NameFilter");
    name_->SetMaximumCharCount(255);
    name_->SetText(session.settings.filters.name.c_str());
    addFilter("NameLabel", "#CSEServerBrowser_Name", name_);
    map_ = new vgui2::TextEntry(contents, "MapFilter");
    map_->SetMaximumCharCount(255);
    map_->SetText(session.settings.filters.map.c_str());
    addFilter("MapLabel", "#ServerBrowser_Map", map_);
    ping_ = new vgui2::TextEntry(contents, "PingFilter");
    ping_->SetMaximumCharCount(5);
    ping_->SetAllowNumericInputOnly(true);
    ping_->SetText(std::to_string(session.settings.filters.maxPing).c_str());
    addFilter("PingLabel", "#ServerBrowser_Latency", ping_);
    auto requirement = [this](const char *name, Requirement selected) {
        auto *control = new vgui2::ComboBox(body_->Contents(), name, 3, false);
        control->AddItem("#ServerBrowser_All", nullptr);
        control->AddItem("#CSEServerBrowser_Yes", nullptr);
        control->AddItem("#CSEServerBrowser_No", nullptr);
        control->ActivateItem(static_cast<int>(selected));
        return control;
    };
    password_ = requirement("PasswordFilter", session.settings.filters.password);
    addFilter("PasswordLabel", "#ServerBrowser_PasswordLabel", password_);
    secure_ = requirement("SecureFilter", session.settings.filters.secure);
    addFilter("SecureLabel", "#CSEServerBrowser_Secure", secure_);
    region_ = new vgui2::ComboBox(body_->Contents(), "RegionFilter", 9, false);
    const char *regions[]{"#ServerBrowser_World",        "#ServerBrowser_US_East",    "#ServerBrowser_US_West",
                          "#ServerBrowser_SouthAmerica", "#ServerBrowser_Europe",     "#ServerBrowser_Asia",
                          "#ServerBrowser_Australia",    "#ServerBrowser_MiddleEast", "#ServerBrowser_Africa"};
    for (const char *region : regions) {
        region_->AddItem(region, nullptr);
    }
    region_->ActivateItem(session.settings.filters.region + 1);
    addFilter("RegionLabel", "#ServerBrowser_Location", region_);
    nonempty_ = new vgui2::CheckButton(contents, "NonemptyFilter", "#ServerBrowser_HasUsersPlaying");
    nonempty_->SetSelected(session.settings.filters.nonempty);
    filterControls_.push_back(nonempty_);
    notFull_ = new vgui2::CheckButton(contents, "NotFullFilter", "#ServerBrowser_ServerNotFull");
    notFull_->SetSelected(session.settings.filters.notFull);
    filterControls_.push_back(notFull_);
    auto addAction = [this](const char *command, const char *label) {
        auto *button = new vgui2::Button(body_->Contents(), command, label);
        button->SetCommand(command);
        button->AddActionSignalTarget(this);
        actions_.push_back(button);
        return button;
    };
    addAction("Apply", "#CSEServerBrowser_Apply");
    addAction("Refresh", "#ServerBrowser_RefreshAll");
    addAction("Stop", "#ServerBrowser_StopRefreshingList");
    addAction("Quick", "#ServerBrowser_RefreshQuick");
    addAction("QuickSelected", "#ServerBrowser_Refresh");
    connect_ = addAction("Connect", "#ServerBrowser_Connect");
    addAction("Details", "#ServerBrowser_GameInfoTitle");
    addAction("Favorite", "#ServerBrowser_AddToFavorites");
    addAction("RemoveFavorite", "#ServerBrowser_RemoveServerFromFavorites");
    addAction("AddAddress", "#ServerBrowser_AddServerByIP");
    address_ = new vgui2::TextEntry(contents, "FavoriteAddress");
    constexpr int AddressTextLimit = 64;
    address_->SetMaximumCharCount(AddressTextLimit);
    address_->SetText("127.0.0.1:27015");
    for (auto &tab : tabStates_) {
        tab.filters.app = session.settings.filters.app;
        tab.filters.gameDirectory = session.settings.filters.gameDirectory;
    }
    int tabPosition = 1;
    for (size_t index = 1; index < filterControls_.size() - 2; index += 2) {
        filterControls_[index]->SetTabPosition(tabPosition++);
    }
    nonempty_->SetTabPosition(tabPosition++);
    notFull_->SetTabPosition(tabPosition++);
    tabs_->SetTabPosition(tabPosition++);
    for (auto *button : actions_) {
        button->SetTabPosition(tabPosition++);
    }
    address_->SetTabPosition(tabPosition);
    list_->SetTabPosition(1);
    auto &savedTab = tabStates_[static_cast<size_t>(session.settings.kind)];
    savedTab.filters = session.settings.filters;
    savedTab.sort = session.settings.sort;
    SetVisible(false);
}

ServerBrowserDialog::~ServerBrowserDialog() = default;

void ServerBrowserDialog::PerformLayout() {
    ClampFrame(*this);
    Frame::PerformLayout();
    int left = 0;
    int top = 0;
    int clientWidth = 0;
    int clientHeight = 0;
    GetClientArea(left, top, clientWidth, clientHeight);
    const int gap = Padding(*this);
    body_->SetBounds(left + gap, top + gap, std::max(1, clientWidth - 2 * gap), std::max(1, clientHeight - 2 * gap));
    constexpr size_t FilterPairs = 7;
    std::array<Extent, FilterPairs> groups{};
    int width = std::max(body_->ViewWidth(), static_cast<BrowserSheet *>(tabs_)->MinimumWidth());
    for (size_t index = 0; index < FilterPairs; ++index) {
        auto *label = static_cast<vgui2::Label *>(filterControls_[2 * index]);
        auto *control = filterControls_[2 * index + 1];
        auto size = LabelExtent(*label);
        const int controlHeight = size.height + 2 * gap;
        constexpr int EntryWidth = 150;
        size.width = std::max(size.width, GetProportionalScaledValue(EntryWidth));
        if (auto *combo = dynamic_cast<vgui2::ComboBox *>(control)) {
            auto *menu = combo->GetMenu();
            for (int item = 0; item < menu->GetItemCount(); ++item) {
                auto *choice = menu->GetMenuItem(menu->GetMenuID(item));
                size.width = std::max(size.width, LabelExtent(*choice).width + controlHeight + 2 * gap);
            }
        }
        groups[index] = {size.width + 2 * gap, size.height + controlHeight + gap};
        width = std::max(width, groups[index].width);
    }
    for (auto *check : {nonempty_, notFull_}) {
        width = std::max(width, LabelExtent(*check).width + 2 * gap);
    }
    for (auto *button : actions_) {
        width = std::max(width, LabelExtent(*button).width + 2 * gap);
    }
    for (auto *label : {status_, actionStatusLabel_}) {
        width = std::max(width, LabelExtent(*label).width + 2 * gap);
    }
    int x = 0;
    int y = 0;
    int rowHeight = 0;
    auto next = [&](int itemWidth, int itemHeight) {
        if (x && x + itemWidth > width) {
            x = 0;
            y += rowHeight + gap;
            rowHeight = 0;
        }
        rowHeight = std::max(rowHeight, itemHeight);
    };
    for (size_t index = 0; index < FilterPairs; ++index) {
        const auto size = groups[index];
        next(size.width, size.height);
        auto *label = static_cast<vgui2::Label *>(filterControls_[2 * index]);
        const int labelHeight = LabelExtent(*label).height;
        label->SetBounds(x, y, size.width, labelHeight);
        filterControls_[2 * index + 1]->SetBounds(x, y + labelHeight + gap, size.width,
                                                  size.height - labelHeight - gap);
        x += size.width + gap;
    }
    for (auto *check : {nonempty_, notFull_}) {
        const auto size = LabelExtent(*check);
        next(size.width + 2 * gap, size.height + gap);
        check->SetBounds(x, y, size.width + 2 * gap, size.height + gap);
        x += size.width + 3 * gap;
    }
    y += rowHeight + gap;
    const int tabTop = y;
    // Reserve the action flow and both independent message lines before assigning
    // remaining height to the table. Small workspaces scroll this fixed content.
    x = 0;
    y = 0;
    rowHeight = 0;
    for (auto *button : actions_) {
        const auto size = LabelExtent(*button);
        next(size.width + 2 * gap, size.height + 2 * gap);
        button->SetBounds(x, y, size.width + 2 * gap, size.height + 2 * gap);
        x += size.width + 3 * gap;
    }
    constexpr int AddressWidth = 240;
    const int addressHeight = LabelExtent(*connect_).height + 2 * gap;
    const int addressWidth = std::min(width, GetProportionalScaledValue(AddressWidth));
    next(addressWidth, addressHeight);
    address_->SetBounds(x, y, addressWidth, addressHeight);
    const int actionHeight = y + rowHeight + gap;
    const int statusHeight = LabelExtent(*status_).height + gap;
    const int actionStatusHeight = LabelExtent(*actionStatusLabel_).height + gap;
    const int tabHeight = static_cast<BrowserSheet *>(tabs_)->TabHeight(width);
    constexpr int MinimumTableRows = 6;
    const int minimumTable = (LabelExtent(*status_).height + gap) * MinimumTableRows;
    const int tableHeight = std::max(minimumTable, body_->ViewHeight() - tabTop - tabHeight - actionHeight -
                                                       statusHeight - actionStatusHeight - gap);
    tabs_->SetBounds(0, tabTop, width, tabHeight + tableHeight);
    tabs_->InvalidateLayout(true, false);
    const int actionTop = tabTop + tabs_->GetTall() + gap;
    for (auto *button : actions_) {
        int buttonX = 0;
        int buttonY = 0;
        button->GetPos(buttonX, buttonY);
        button->SetPos(buttonX, actionTop + buttonY);
    }
    int addressX = 0;
    int addressY = 0;
    address_->GetPos(addressX, addressY);
    address_->SetPos(addressX, actionTop + addressY);
    status_->SetBounds(0, actionTop + actionHeight, width, statusHeight);
    actionStatusLabel_->SetBounds(0, actionTop + actionHeight + statusHeight, width, actionStatusHeight);
    body_->SetContentSize(width, actionTop + actionHeight + statusHeight + actionStatusHeight);
    if (auto *page = tabs_->GetActivePage()) {
        const int scrollWidth = GetProportionalScaledValue(ScrollWidth);
        table_->SetBounds(0, 0, std::max(1, page->GetWide() - scrollWidth), page->GetTall());
        const int listWidth = std::max(table_->ViewWidth(), list_->MinimumWidth());
        list_->SetBounds(0, 0, listWidth, table_->ViewHeight());
        list_->InvalidateLayout(true, false);
        table_->SetContentSize(listWidth, table_->ViewHeight());
        scroll_->SetBounds(page->GetWide() - scrollWidth, 0, scrollWidth, page->GetTall());
    }
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
    RetireChildren();
}

void ServerBrowserDialog::OnMessage(const KeyValues *message, vgui2::VPANEL sender) {
    if (std::strcmp(message->GetName(), "BrowserInactiveDispatch") == 0) {
        inactiveDispatchObserved_ = true;
        return;
    }
    Frame::OnMessage(message, sender);
}

void ServerBrowserDialog::OnClose() {
    Deactivate();
}

void ServerBrowserDialog::RestoreFilters() {
    const auto &filters = session_.settings.filters;
    game_->ActivateItem(filters.app == HalfLifeApp ? 1 : 0);
    name_->SetText(filters.name.c_str());
    map_->SetText(filters.map.c_str());
    ping_->SetText(std::to_string(filters.maxPing).c_str());
    password_->ActivateItem(static_cast<int>(filters.password));
    secure_->ActivateItem(static_cast<int>(filters.secure));
    region_->ActivateItem(filters.region + 1);
    region_->SetEnabled(session_.settings.kind == ListKind::Internet || session_.settings.kind == ListKind::Spectate);
    nonempty_->SetSelected(filters.nonempty);
    notFull_->SetSelected(filters.notFull);
}

bool ServerBrowserDialog::ChangeTab(int tab) {
    if (tab < 0 || static_cast<size_t>(tab) >= tabStates_.size()) {
        return false;
    }
    if (activeTab_ >= 0) {
        auto &outgoing = tabStates_[static_cast<size_t>(activeTab_)];
        outgoing.filters = session_.settings.filters;
        outgoing.sort = session_.settings.sort;
        // A bounded reset clears the live selection before its saved RowKey can be restored.
        if (!restoreSelection_) {
            outgoing.selection = session_.model.Selection();
        }
    }
    const auto kind = static_cast<ListKind>(tab);
    auto &destination = tabStates_[static_cast<size_t>(tab)];
    if (kind != ListKind::Internet && kind != ListKind::Spectate) {
        destination.filters.region = Filters{}.region;
    }
    if (session_.servers) {
        session_.model.Stop();
    }
    const bool accepted = session_.model.Refresh(kind, destination.filters);
    const auto state = session_.model.GetStatus();
    // A provider start failure has already installed this query identity; an early refusal has not.
    if (!accepted && state.kind != kind) {
        if (activeTab_ >= 0) {
            tabs_->SetActivePage(tabs_->GetPage(activeTab_));
        }
        RestoreFilters();
        return false;
    }
    transitioning_ = activeTab_ >= 0 && activeTab_ != tab;
    activeTab_ = tab;
    session_.settings.kind = kind;
    session_.settings.filters = destination.filters;
    session_.settings.sort = destination.sort;
    session_.model.SetSort(destination.sort);
    session_.presentation.RequestSort(destination.sort);
    restoreSelection_ = destination.selection.has_value();
    RestoreFilters();
    table_->SetParent(tabs_->GetActivePage());
    scroll_->SetParent(tabs_->GetActivePage());
    InvalidateLayout(false, false);
    return true;
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
    session_.model.Pump(BrowserFrameBudget);
    session_.presentation.Pump(BrowserFrameBudget);
    if (session_.presentation.CanAcceptUpdates()) {
        for (const auto &update : session_.model.TakeUpdates(BrowserFrameBudget)) {
            session_.presentation.Apply(update);
        }
    }
    const auto state = session_.model.GetStatus();
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
    }
    if (restoreSelection_) {
        const auto selection = tabStates_[static_cast<size_t>(activeTab_)].selection;
        if (selection && session_.model.Find(*selection)) {
            session_.model.Select(selection);
            restoreSelection_ = false;
        }
    }
    UpdateRows();
    if (detailsDialog_.Get()) {
        detailsDialog_->Pump();
    }
    const char *text = "";
    if (!state.error.empty()) {
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
        text = "#ServerBrowser_NoInternetGames";
    }
    if (shownQuery_ != text) {
        shownQuery_ = text;
        status_->SetText(text);
        InvalidateLayout(false, false);
    }
    const char *action = actionStatus_.empty() ? state.actionError.c_str() : actionStatus_.c_str();
    if (!*action && !session_.config.Status().error.empty()) {
        action = "#CSEServerBrowser_Unsaved";
    }
    if (shownAction_ != action) {
        shownAction_ = action;
        actionStatusLabel_->SetText(action);
        InvalidateLayout(false, false);
    }
}

void ServerBrowserDialog::UpdateRows() {
    visible_ = transitioning_ ? std::vector<Row>{} : session_.presentation.ReadViewport(list_->Capacity());
    ResizeSlots(*list_, slots_, list_->Capacity());
    list_->ClearSelectedItems();
    const auto selection = session_.model.Selection();
    bool canConnect = false;
    for (size_t slot = 0; slot < slots_.size(); ++slot) {
        list_->SetItemVisible(slots_[slot], slot < visible_.size());
        if (slot >= visible_.size()) {
            continue;
        }
        const auto &row = visible_[slot];
        auto *item = list_->GetItem(slots_[slot]);
        item->SetString("name", row.name.c_str());
        item->SetString("game", row.gameDescription.c_str());
        item->SetString("map", row.map.c_str());
        item->SetString("address", ConnectionAddress(row.key).c_str());
        item->SetString("password", row.responded ? (row.password ? "#CSEServerBrowser_Yes" : "") : "?");
        item->SetString("secure", row.responded ? (row.secure ? "#CSEServerBrowser_Yes" : "") : "?");
        item->SetString("players", row.responded
                                       ? (std::to_string(row.players) + "/" + std::to_string(row.maxPlayers)).c_str()
                                       : "?");
        item->SetString("ping", row.responded ? std::to_string(row.ping).c_str() : "?");
        list_->ApplyItemChanges(slots_[slot]);
        if (selection && row.key == *selection) {
            list_->SetSingleSelectedItem(slots_[slot]);
            canConnect = row.responded && !row.failed && session_.connection.CanConnect(row);
            if (!session_.connection.CanConnect(row)) {
                actionStatus_ = "#CSEServerBrowser_CurrentGame";
            }
        }
    }
    list_->SortList();
    connect_->SetEnabled(canConnect);
    static_cast<LogicalScroll *>(scroll_)->Synchronize(session_.presentation.VisibleCount(), list_->Capacity(),
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
    sort.ascending = sort.column == static_cast<Column>(column) ? !sort.ascending : true;
    sort.column = static_cast<Column>(column);
    session_.model.SetSort(sort);
    session_.presentation.RequestSort(sort);
    SaveSettings();
}
void ServerBrowserDialog::Seek(size_t position) {
    BrowserUse use(session_);
    if (!session_.closing && session_.active) {
        session_.presentation.SeekForUserAction(position);
        UpdateRows();
    }
}
void ServerBrowserDialog::SelectVisible(size_t slot) {
    BrowserUse use(session_);
    if (!session_.closing && session_.active && slot < visible_.size()) {
        restoreSelection_ = false;
        session_.model.Select(visible_[slot].key);
        actionStatus_.clear();
    }
}
void ServerBrowserDialog::MoveSelection(int delta, bool endpoint) {
    BrowserUse use(session_);
    const size_t count = session_.presentation.VisibleCount();
    if (session_.closing || !session_.active || !count) {
        return;
    }
    restoreSelection_ = false;
    size_t rank = session_.presentation.FirstVisibleRank();
    const auto selected = session_.model.Selection();
    if (selected) {
        if (const auto selectedRank = session_.presentation.RankForUserAction(*selected)) {
            rank = *selectedRank;
        }
    }
    const auto target = endpoint
                            ? (delta < 0 ? 0 : static_cast<long long>(count - 1))
                            : std::clamp(static_cast<long long>(rank) + delta, 0LL, static_cast<long long>(count - 1));
    session_.presentation.SeekForUserAction(static_cast<size_t>(target));
    const auto selectedRow = session_.presentation.ReadViewport(1);
    if (!selectedRow.empty()) {
        session_.model.Select(selectedRow.front().key);
    }
    UpdateRows();
}
void ServerBrowserDialog::ConnectSelection() {
    BrowserUse use(session_);
    if (session_.closing || !session_.active) {
        return;
    }
    const auto selected = session_.model.Selection();
    const Row *row = selected ? session_.model.Find(*selected) : nullptr;
    if (!row) {
        return;
    }
    if (!session_.connection.CanConnect(*row)) {
        ShowActionStatus("#CSEServerBrowser_CurrentGame");
        return;
    }
    if (row->password) {
        if (passwordDialog_.Get()) {
            passwordDialog_->Retire();
            return;
        }
        passwordDialog_ = new BrowserPasswordDialog(*this, session_, *row);
        passwordDialog_->DoModal();
    } else {
        ConnectRow(*row, "");
    }
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
void ServerBrowserDialog::OpenDetails() {
    if (detailsDialog_.Get()) {
        detailsDialog_->Activate();
        return;
    }
    const auto selected = session_.model.Selection();
    const Row *row = selected ? session_.model.Find(*selected) : nullptr;
    if (row) {
        detailsDialog_ = new BrowserDetailsDialog(*this, session_, *row);
        detailsDialog_->Activate();
    }
}
void ServerBrowserDialog::ShowContextMenu() {
    BrowserUse use(session_);
    if (session_.closing || !session_.active) {
        return;
    }
    if (!contextMenu_.Get()) {
        contextMenu_ = new vgui2::Menu(this, "ServerContext");
        contextMenu_->AddMenuItem("Connect", "#ServerBrowser_Connect", "Connect", this, nullptr);
        contextMenu_->AddMenuItem("Details", "#ServerBrowser_GameInfoTitle", "Details", this, nullptr);
        contextMenu_->AddMenuItem("Favorite", "#ServerBrowser_AddToFavorites", "Favorite", this, nullptr);
        contextMenu_->AddMenuItem("Remove", "#ServerBrowser_RemoveServerFromFavorites", "RemoveFavorite", this,
                                  nullptr);
        contextMenu_->AddMenuItem("Refresh", "#ServerBrowser_Refresh", "QuickSelected", this, nullptr);
    }
    auto *menu = contextMenu_.Get();
    int x = 0;
    int y = 0;
    vgui2::input()->GetCursorPos(x, y);
    menu->SetPos(x, y);
    menu->SetVisible(true);
}
void ServerBrowserDialog::ApplyFilters() {
    restoreSelection_ = false;
    auto &filters = session_.settings.filters;
    filters.app = game_->GetActiveItem() == 1 ? HalfLifeApp : CounterStrikeApp;
    filters.gameDirectory = filters.app == HalfLifeApp ? "valve" : "cstrike";
    filters.name = Text(name_);
    filters.map = Text(map_);
    filters.maxPing = std::atoi(Text(ping_).c_str());
    filters.password = static_cast<Requirement>(password_->GetActiveItem());
    filters.secure = static_cast<Requirement>(secure_->GetActiveItem());
    filters.region = region_->GetActiveItem() - 1;
    filters.nonempty = nonempty_->IsSelected();
    filters.notFull = notFull_->IsSelected();
    session_.model.Refresh(session_.settings.kind, filters);
    SaveSettings();
}
void ServerBrowserDialog::OnCommand(const char *command) {
    BrowserUse use(session_);
    if (session_.closing || !session_.active) {
        return;
    }
    actionStatus_.clear();
    const auto selected = session_.model.Selection();
    const Row *row = selected ? session_.model.Find(*selected) : nullptr;
    if (std::strcmp(command, "Apply") == 0) {
        ApplyFilters();
    } else if (std::strcmp(command, "Refresh") == 0) {
        session_.model.Refresh(session_.settings.kind, session_.settings.filters);
    } else if (std::strcmp(command, "Stop") == 0) {
        session_.model.Stop();
    } else if (std::strcmp(command, "Quick") == 0) {
        session_.model.QuickRefresh();
    } else if (std::strcmp(command, "QuickSelected") == 0 && selected) {
        session_.model.QuickRefresh(*selected);
    } else if (std::strcmp(command, "Connect") == 0) {
        ConnectSelection();
    } else if (std::strcmp(command, "Details") == 0) {
        OpenDetails();
    } else if (std::strcmp(command, "Favorite") == 0 && row) {
        session_.model.AddFavorite(row->app, row->key);
    } else if (std::strcmp(command, "RemoveFavorite") == 0 && row) {
        session_.model.RemoveFavorite(row->app, row->key);
    } else if (std::strcmp(command, "AddAddress") == 0) {
        Address address;
        if (ParseAddress(Text(address_), address)) {
            session_.model.AddFavorite(session_.settings.filters.app, address);
        } else {
            ShowActionStatus("#ServerBrowser_AddServerError");
        }
    } else {
        SaveSettings();
        Frame::OnCommand(command);
    }
}
void ServerBrowserDialog::ShowActionStatus(const char *token) {
    actionStatus_ = token;
    shownAction_ = token;
    actionStatusLabel_->SetText(token);
    InvalidateLayout(false, false);
}
void ServerBrowserDialog::SaveSettings() {
    session_.settings.width = GetWide();
    session_.settings.height = GetTall();
    session_.config.SetSettings(session_.settings);
    session_.config.Save();
}
bool ServerBrowserDialog::RetireChildren() {
    if (contextMenu_.Get()) {
        contextMenu_->SetVisible(false);
        contextMenu_->MarkForDeletion();
    }
    if (passwordDialog_.Get()) {
        passwordDialog_->Retire();
    }
    if (detailsDialog_.Get()) {
        if (!detailsDialog_->BeginClose()) {
            return false;
        }
        detailsDialog_->MarkForDeletion();
    }
    return !contextMenu_.Get() && !passwordDialog_.Get() && !detailsDialog_.Get();
}

bool ServerBrowserDialog::BeginClose() {
    session_.active = false;
    SetVisible(false);
    return RetireChildren();
}

}
