#pragma once

#include "ServerBrowserConfig.h"
#include "ServerBrowserConnection.h"
#include "ServerBrowserDetails.h"
#include "ServerBrowserPresentation.h"
#include "Frame.h"
#include "PHandle.h"
#include <map>
#include <array>

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
class Menu;
}
namespace cse::server_browser {
constexpr size_t BrowserFrameBudget = 128;
struct BrowserSession {
    SteamFavoriteStore favorites;
    ServerBrowserModel model;
    ServerBrowserPresentation presentation;
    NativeConfigStorage storage;
    ServerBrowserConfig config;
    ServerBrowserConnection connection;
    ISteamMatchmakingServers *servers;
    BrowserSettings settings;
    size_t useDepth = 0;
    bool closing = false;
    bool active = false;
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
class BrowserOverflow;
class BrowserPasswordDialog;
class BrowserDetailsDialog;
class ServerBrowserDialog final : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(ServerBrowserDialog, vgui2::Frame, "Frame");

public:
    explicit ServerBrowserDialog(BrowserSession &session);
    ~ServerBrowserDialog() override;
    void Pump();
    void OnMessage(const KeyValues *message, vgui2::VPANEL sender) override;
    void Activate() override;
    void Deactivate();
    bool BeginClose();
    void SaveSettings();
    void ChooseSort(int column);
    void Seek(size_t position);
    void SelectVisible(size_t slot);
    void ConnectSelection();
    void ConnectRow(const Row &row, std::string_view password);
    void ShowContextMenu();
    void MoveSelection(int delta, bool endpoint = false);
    size_t WindowSize() const;

protected:
    void OnCommand(const char *command) override;
    void OnClose() override;
    void PerformLayout() override;

private:
    void ApplyFilters();
    bool ChangeTab(int tab);
    void RestoreFilters();
    bool RetireChildren();
    void UpdateRows();
    void OpenDetails();
    void ShowActionStatus(const char *token);
    BrowserSession &session_;
    BrowserOverflow *body_ = nullptr;
    BrowserOverflow *table_ = nullptr;
    vgui2::PropertySheet *tabs_ = nullptr;
    BrowserViewport *list_ = nullptr;
    vgui2::ScrollBar *scroll_ = nullptr;
    vgui2::Label *status_ = nullptr;
    vgui2::Label *actionStatusLabel_ = nullptr;
    vgui2::ComboBox *game_ = nullptr;
    vgui2::ComboBox *password_ = nullptr;
    vgui2::ComboBox *secure_ = nullptr;
    vgui2::ComboBox *region_ = nullptr;
    vgui2::TextEntry *name_ = nullptr;
    vgui2::TextEntry *map_ = nullptr;
    vgui2::TextEntry *ping_ = nullptr;
    vgui2::TextEntry *address_ = nullptr;
    vgui2::CheckButton *nonempty_ = nullptr;
    vgui2::CheckButton *notFull_ = nullptr;
    vgui2::Button *connect_ = nullptr;
    std::vector<vgui2::Panel *> filterControls_;
    std::vector<vgui2::Button *> actions_;
    std::vector<Row> visible_;
    std::vector<int> slots_;
    vgui2::DHANDLE<vgui2::Menu> contextMenu_;
    vgui2::DHANDLE<BrowserPasswordDialog> passwordDialog_;
    vgui2::DHANDLE<BrowserDetailsDialog> detailsDialog_;
    struct TabState {
        Filters filters;
        Sort sort;
        std::optional<RowKey> selection;
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
    std::string actionStatus_;
    std::string shownQuery_;
    std::string shownAction_;
};
}
