#pragma once

#include "ServerBrowserConfig.h"
#include "ServerBrowserMasterserverQuery.h"
#include "ServerBrowserConnection.h"
#include "ServerBrowserDetails.h"
#include "ServerBrowserPresentation.h"
#include "Frame.h"
#include "PHandle.h"
#include <map>
#include <array>

class ISteamApps;

namespace vgui2 {
class PropertySheet;
class PropertyPage;
class ListPanel;
class ScrollBar;
class Label;
class ComboBox;
class TextEntry;
class CheckButton;
class Button;
class ToggleButton;
class Menu;
}
namespace cse::server_browser {
constexpr size_t BrowserFrameBudget = 128;
struct BrowserSession {
    MasterserverQueryProvider masterserverProvider;
    SteamFavoriteStore favorites;
    ServerBrowserModel model;
    ServerBrowserPresentation presentation;
    NativeConfigStorage storage;
    ServerBrowserConfig config;
    ServerBrowserConnection connection;
    ISteamMatchmakingServers *servers;
    ISteamApps *apps = nullptr;
    BrowserSettings settings;
    size_t useDepth = 0;
    bool closing = false;
    bool active = false;
    bool favoritesDirty = false;
    bool AddFavorite(AppId_t app, Address address);
    BrowserSession(ISteamMatchmakingServers *serverProvider, ISteamMatchmaking *favoriteProvider,
                   IFileSystem *filesystem, IConnectionEngine &engine, AppId_t app, const char *game,
                   std::string preferenceRoot = {}, IFavoriteStore *injectedFavorites = nullptr);
};
class BrowserUse final {
public:
    explicit BrowserUse(BrowserSession &session) : session_(session) {
        ++session_.useDepth;
    }
    ~BrowserUse() {
        --session_.useDepth;
    }

private:
    BrowserSession &session_;
};
class BrowserViewport;
class BrowserPage;
class BrowserPasswordDialog;
class BrowserDetailsDialog;
class BrowserAddServerDialog;
class BrowserSettingsDialog;
class ServerBrowserDialog final : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(ServerBrowserDialog, vgui2::Frame, "Frame");

public:
    explicit ServerBrowserDialog(BrowserSession &session);
    ~ServerBrowserDialog() override;
    void Pump();
    void RefreshLanguagePresentation();
    void OnMessage(const KeyValues *message, vgui2::VPANEL sender) override;
    void Activate() override;
    void Deactivate();
    bool BeginClose();
    void SaveSettings();
    void ChooseSort(int column);
    void Seek(size_t position);
    void SelectVisible(size_t slot);
    void SelectVisibleItems(const std::vector<size_t> &slots);
    void ConnectSelection();
    void ConnectRow(const Row &row, std::string_view password);
    void OwnMessage(vgui2::Frame *message);
    void ShowContextMenu();
    void MoveSelection(int delta, bool endpoint = false, bool page = false);
    void ScrollViewport(int delta);
    size_t WindowSize() const;

protected:
    void OnCommand(const char *command) override;
    void OnClose() override;
    void PerformLayout() override;
    bool GetDefaultScreenPosition(int &x, int &y, int &width, int &height) override;

private:
    void ApplyFilters();
    void ApplyMasterservers(const MasterserverSettings& settings);
    void ReconcileSelection();
    void BindPage();
    void ReloadPage();
    void UpdateActions();
    void UpdateFilterSummary();
    bool ChangeTab(int tab);
    void RestoreFilters();
    bool RetireChildren();
    void UpdateRows();
    BrowserDetailsDialog *OpenDetails();
    void ShowActionStatus(const char *token);
    BrowserSession &session_;
    BrowserPage *page_ = nullptr;
    vgui2::PropertySheet *tabs_ = nullptr;
    BrowserViewport *list_ = nullptr;
    vgui2::ScrollBar *scroll_ = nullptr;
    vgui2::Label *status_ = nullptr;
    vgui2::Label *filterSummary_ = nullptr;
    vgui2::ToggleButton *filterToggle_ = nullptr;
    vgui2::Button *settingsButton_ = nullptr;
    vgui2::Button *refresh_ = nullptr;
    vgui2::Button *quick_ = nullptr;
    vgui2::Button *addServer_ = nullptr;
    vgui2::Button *addCurrent_ = nullptr;
    vgui2::ComboBox *game_ = nullptr;
    vgui2::CheckButton *password_ = nullptr;
    vgui2::CheckButton *validAccount_ = nullptr;
    vgui2::ComboBox *secure_ = nullptr;

    vgui2::TextEntry *map_ = nullptr;
    vgui2::ComboBox *ping_ = nullptr;
    vgui2::CheckButton *nonempty_ = nullptr;
    vgui2::CheckButton *notFull_ = nullptr;
    vgui2::Button *connect_ = nullptr;

    std::vector<Row> visible_;
    std::vector<int> slots_;
    vgui2::DHANDLE<vgui2::Menu> contextMenu_;
    std::vector<vgui2::DHANDLE<BrowserDetailsDialog>> detailsDialogs_;
    vgui2::DHANDLE<BrowserAddServerDialog> addServerDialog_;
    vgui2::DHANDLE<BrowserSettingsDialog> settingsDialog_;
    std::vector<RowKey> selectedKeys_;
    std::optional<RowKey> keyboardCursor_;
    std::optional<RowKey> rangeAnchor_;
    std::vector<vgui2::DHANDLE<vgui2::Frame>> messageDialogs_;
    struct TabState {
        Filters filters;
        Sort sort{Column::Address, true};
        std::optional<RowKey> selection;
        bool filtersExpanded = false;
    };
    static constexpr size_t TabCount = static_cast<size_t>(ListKind::Spectate) + 1;
    std::array<TabState, TabCount> tabStates_;
    int activeTab_ = -1;
    bool deactivating_ = false;
    bool inactiveDispatchObserved_ = false;
    bool reopenRequested_ = false;
    bool refreshOnOpen_ = false;
    bool transitioning_ = false;
    bool restoreSelection_ = false;
    bool restoringFilters_ = false;
    int tabsTopAdjustment_ = 0;
    std::string actionStatus_;
    std::string shownStatus_;
};
}
