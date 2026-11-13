#include "model_fixture.inc"
#include "ServerBrowserDialog.h"
#include "check.h"
#include "detail_provider_fixture.h"
#include "controls.h"
#include "../../../GoldSrc-SDK/engine/controls.h"
#include "ListPanel.h"
#include "PropertySheet.h"
#include "PropertyPage.h"
#include "TextEntry.h"
#include "Button.h"
#include "Label.h"
#include "ComboBox.h"
#include "CheckButton.h"
#include "ToggleButton.h"
#include "Menu.h"
#include "ScrollBar.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "filesystem.h"
#include "vgui/KeyValues.h"
#include "geometry_fixture.h"
#include "system_fixture.h"
#include "layout_fixture.h"
#include "input_retirement_fixture.h"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>

// The component injects the typed provider below; it does not initialize the production Steam service.
extern "C" void CseSteamModuleInitialize() {}

#include "../../../GoldSrc-SDK/public/native_gameui_retirement.h"
#include "panel_owner_fixture.h"
#include "baseui_owner_fixture.h"

namespace {
constexpr size_t ServerCount = 10000;
constexpr int MaxFrames = 2000;
class Engine final : public IConnectionEngine {
public:
    std::string password = "before";
    std::vector<std::string> commands;
    const char *Password() override {
        return password.c_str();
    }
    void SetPassword(const char *value) override {
        password = value;
    }
    void Connect(const char *value) override {
        commands.emplace_back(value);
    }
};
void Send(vgui2::Panel &panel, const char *command) {
    vgui2::ivgui()->PostMessage(panel.GetVPanel(), new KeyValues("Command", "command", command), 0, 0);
    vgui2::ivgui()->RunFrame();
}
void Settle(ServerBrowserDialog &dialog, BrowserSession &session) {
    for (int frame = 0; frame < MaxFrames; ++frame) {
        dialog.Pump();
        vgui2::ivgui()->RunFrame();
        vgui2::surface()->SolveTraverse(dialog.GetVPanel(), false);
        vgui2::surface()->PaintTraverse(dialog.GetVPanel());
        const auto status = session.model.GetStatus();
        if (!status.viewUpdating && !status.pending && !session.presentation.IsUpdating()) {
            dialog.Pump();
            return;
        }
    }
    throw std::runtime_error("actual VGUI presentation failed to settle within bounded work attempts");
}
void StartList(ServerBrowserDialog &dialog, BrowserSession &session, DetailProvider &provider) {
    dialog.Pump();
    provider.lists.Complete();
    Settle(dialog, session);
}
vgui2::ListPanel *List(ServerBrowserDialog &dialog) {
    auto *tabs = static_cast<vgui2::PropertySheet *>(dialog.FindChildByName("GameTabs", false));
    auto *result = dynamic_cast<vgui2::ListPanel *>(tabs->GetActivePage()->FindChildByName("gamelist", false));
    Require(result != nullptr, "actual ListPanel missing");
    return result;
}
size_t AllocatedSlots(vgui2::ListPanel &list) {
    size_t count = 0;
    for (int item = list.FirstItem(); item != list.InvalidItemID(); item = list.NextItem(item)) {
        ++count;
    }
    return count;
}
}

int main(int argc, char **argv) {
    if (argc != 6 && argc != 7) {
        return 2;
    }
    return RunCase([&] {
        void *guiImage = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void *filesystemImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        const char *loadError = dlerror();
        Require(guiImage && filesystemImage, loadError ? loadError : "load actual VGUI/filesystem images");
        auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(guiImage, "CreateInterface"));
        auto filesystemFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(filesystemImage, "CreateInterface"));
        auto *filesystem = static_cast<IFileSystem *>(filesystemFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(guiImage,
            static_cast<vgui2::ISystem *>(guiFactory("VGUI_System009", nullptr)));
        filesystem->Mount();
        filesystem->AddSearchPath(argv[3], "PLATFORM");
        if (argc == 7) {
            filesystem->AddSearchPath(argv[6], "GAME");
        }
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface *>(guiFactory("VGUI_Surface026", nullptr)));
        const std::string layoutCase = argv[5];
        const bool focusLayout = layoutCase == "focus-resize" || layoutCase == "focus-scheme" ||
                                 layoutCase == "focus-tab" || layoutCase == "focus-header";
        const bool positionCase = layoutCase.rfind("position-", 0) == 0;
        const bool hdLayout = layoutCase == "hd-layout" || layoutCase == "hd-saved" || focusLayout ||
                              layoutCase == "tab-order" || positionCase;
        const bool playerRefresh = layoutCase == "original-game-info" ||
            layoutCase == "original-game-info-sync-empty" || layoutCase == "original-game-info-generation";
        const bool inputRetirement = std::string(argv[5]) == "input-retirement";
        geometry_fixture::surface = hdLayout || inputRetirement || playerRefresh ? &geometry : nullptr;
        geometry.forceWindowFocus = hdLayout;
        if (layoutCase == "focus-tab") {
            // Construct with the installed scheme at a real tall workspace before
            // shrinking; Label deliberately retains its already assigned font.
            geometry.Resize(3840, 2160);
        }
        CreateInterfaceFn factories[]{system_fixture::Factory, geometry_fixture::Factory, guiFactory, filesystemFactory};
        Require(vgui2::VGuiControls_Init("SERVER_BROWSER_TEST", factories, 4), "initialize actual VGUI controls");
        Require(vgui2::system() == system_fixture::current, "controls bypassed the private registry fixture");
        auto *gui = vgui2::ivgui();
        gui->SetSleep(false);
        gui->Start();
        auto *hdParent = hdLayout || playerRefresh ? geometry_fixture::CreateParent(geometry) : nullptr;
        Require(vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", nullptr) != 0,
                "load original retained scheme");
        if (inputRetirement) {
            input_retirement_fixture::Run(*gui, geometry, guiFactory);
            gui->Shutdown();
            vgui2::system()->Shutdown();
            vgui2::scheme()->Shutdown(true);
            vgui2::surface()->Shutdown();
            vgui2::VGuiControls_Shutdown();
            filesystem->Unmount();
            privateSystem.Restore();
            dlclose(guiImage);
            dlclose(filesystemImage);
            return;
        }
        wchar_t preserved[] = L"Preserved localized current-game reason";
        vgui2::localize()->AddString("CSEServerBrowser_CurrentGame", preserved, "fixture");
        if (std::string(argv[5]) == "baseui-owner") {
            baseui_owner_fixture::Run(gui);
        }
        if (std::string(argv[5]) == "existing-owners") {
            panel_owner_fixture::Run(gui);
        }
        DetailProvider provider;
        Favorites favorites;
        Engine engine;
        const std::string name = argv[5];
        constexpr size_t KeyboardFixtureRows = 64;
        const bool largeFixture = name == "bounded" || name == "navigation-sort" || name == "navigation-update" ||
            name == "rapid-tabs" || name == "tab-selection-supersession" || name == "hd-layout";
        const size_t fixtureRows = name == "original-keyboard-range" ? KeyboardFixtureRows :
            (largeFixture ? ServerCount : 3);
        for (size_t index = 0; index < fixtureRows; ++index) {
            auto row = Server(0x7f000001U + static_cast<uint32_t>(index), ("Server " + std::to_string(index)).c_str());
            row.m_nPing = static_cast<int>(index % 200);
            row.m_bPassword = name == "password" || name == "inactive-close";
            provider.lists.nextRows.push_back(row);
        }
        std::filesystem::create_directories(argv[4]);
        std::string sessionPreferences = argv[4];
        if (name == "unsaved") {
            sessionPreferences = (std::filesystem::path(argv[4]) / "blocked-directory").string();
            std::ofstream blocked(sessionPreferences);
            blocked << "fixture-owned ordinary file blocks creation of its child directory";
        }
        if (name == "hd-saved") {
            NativeConfigStorage storage(filesystem, sessionPreferences);
            ServerBrowserConfig config(storage);
            Require(config.Load(), "fixture could not initialize owned browser preferences");
            BrowserSettings settings;
            settings.savedSize = true;
            settings.width = 16384;
            settings.height = 16384;
            config.SetSettings(settings);
            Require(config.Save(), "fixture could not write owned extreme physical-size preference");
        }
        if (positionCase) {
            std::filesystem::create_directories(std::filesystem::path(sessionPreferences) / "server-browser");
            std::ofstream settings(std::filesystem::path(sessionPreferences) / "server-browser/settings.cfg");
            settings << "originalui=1\nsavedsize=1\nwidth=640\nheight=384\n";
            if (name == "position-saved") {
                settings << "savedposition=1\nx=120\ny=140\n";
            } else if (name == "position-negative") {
                settings << "savedposition=1\nx=-120\ny=-140\n";
            } else if (name == "position-offscreen") {
                settings << "savedposition=1\nx=2147483647\ny=2147483647\n";
            }
        }
        auto ownedSession = std::make_unique<BrowserSession>(&provider, nullptr, filesystem, engine, 10, "cstrike",
                                                             sessionPreferences, &favorites);
        auto &session = *ownedSession;
        auto *dialog = new ServerBrowserDialog(session);
        vgui2::DHANDLE<ServerBrowserDialog> handle;
        handle = dialog;
        if (hdParent) {
            dialog->SetParent(hdParent);
        }
        dialog->Activate();
        StartList(*dialog, session, provider);
        size_t minimumDetailCloseTurns = 0;
        if (positionCase) {
            int x = 0;
            int y = 0;
            dialog->GetPos(x, y);
            if (name == "position-saved") {
                Require(x == 120 && y == 140, "saved browser position was ignored");
            } else if (name == "position-negative") {
                Require(x == 0 && y == 0, "negative offscreen position was not clamped");
            } else if (name == "position-offscreen") {
                Require(x == 1600 - dialog->GetWide() && y == 1200 - dialog->GetTall(),
                        "positive offscreen position was not clamped");
            } else {
                Require(x == 80 && y == 480, "old config changed default browser positioning");
            }
            dialog->SetPos(160, 180);
            dialog->SaveSettings();
            Require(session.config.Load(), "saved position could not be reloaded");
            session.settings = session.config.Settings();
            auto *restored = new ServerBrowserDialog(session);
            restored->GetPos(x, y);
            Require(x == 160 && y == 180, "moved browser position did not survive reconstruction");
            delete restored;
        } else if (focusLayout || name == "tab-order" || name == "hd-layout") {
            auto *tabs = static_cast<vgui2::PropertySheet *>(layout_fixture::Child(*dialog, "GameTabs"));
            auto *initialPage = tabs->GetActivePage();
            vgui2::Panel *retainedFocus = nullptr;
            if (name == "focus-header") {
                retainedFocus = layout_fixture::Child(*List(*dialog), "Name");
            } else if (name == "focus-resize" || name == "focus-scheme") {
                retainedFocus = layout_fixture::Child(*initialPage, "RefreshButton");
            }
            if (retainedFocus) {
                retainedFocus->RequestFocus(0);
                Settle(*dialog, session);
                layout_fixture::FocusVisible(*retainedFocus);
            }
            constexpr std::array<std::array<int, 2>, 4> sizes{{{640, 384}, {1280, 720}, {1600, 1200}, {640, 384}}};
            for (const auto &size : sizes) {
                geometry.Resize(size[0], size[1]);
                dialog->SetSize(size[0], size[1]);
                dialog->InvalidateLayout(true, true);
                Settle(*dialog, session);
                layout_fixture::Check(*dialog, size[0], size[1]);
                if (retainedFocus) {
                    layout_fixture::FocusVisible(*retainedFocus);
                }
                auto *toggle = static_cast<vgui2::ToggleButton *>(layout_fixture::Child(*initialPage, "Filter"));
                toggle->DoClick();
                gui->RunFrame();
                Settle(*dialog, session);
                Require(toggle->IsSelected(), "Filter did not expand");
                layout_fixture::Check(*dialog, size[0], size[1]);
                if (retainedFocus) {
                    layout_fixture::FocusVisible(*retainedFocus);
                }
                toggle->DoClick();
                gui->RunFrame();
                Settle(*dialog, session);
                layout_fixture::Check(*dialog, size[0], size[1]);
                if (retainedFocus) {
                    layout_fixture::FocusVisible(*retainedFocus);
                }
            }
            if (name == "focus-scheme") {
                auto *toggle = static_cast<vgui2::ToggleButton *>(layout_fixture::Child(*initialPage, "Filter"));
                toggle->DoClick();
                gui->RunFrame();
                Settle(*dialog, session);
                auto *map = layout_fixture::Child(*initialPage, "MapFilter");
                map->RequestFocus(0);
                Settle(*dialog, session);
                layout_fixture::FocusVisible(*map);
                const auto originalScheme = dialog->GetScheme();
                constexpr const char *SchemeFilename = "fix1-focus-scheme.res";
                const auto schemePath = std::filesystem::path(argv[4]) / SchemeFilename;
                std::filesystem::copy_file(std::filesystem::path(argv[3]) / "resource/TrackerScheme.res", schemePath);
                filesystem->AddSearchPath(argv[4], "SKIN");
                const auto alternateScheme = vgui2::scheme()->LoadSchemeFromFile(SchemeFilename, "Fix1FocusScheme");
                Require(alternateScheme != 0 && alternateScheme != originalScheme,
                        "focus-scheme did not load a distinct actual scheme handle");
                dialog->SetScheme(alternateScheme);
                dialog->InvalidateLayout(true, true);
                Settle(*dialog, session);
                Require(dialog->GetScheme() == alternateScheme && map->GetScheme() == alternateScheme,
                        "alternate scheme was not inherited by the focused original control");
                layout_fixture::FocusVisible(*map);
                layout_fixture::Check(*dialog, 640, 384);
            }
            const bool keyboardTabs = name == "focus-tab" || name == "tab-order";
            if (keyboardTabs) {
                tabs->RequestFocus(1);
                Settle(*dialog, session);
                layout_fixture::FocusVisible(*tabs->GetActiveTab());
            }
            for (size_t index = 0; index < BrowserTabKinds.size(); ++index) {
                if (keyboardTabs) {
                    if (index) {
                        layout_fixture::Key(*tabs->GetActiveTab(), vgui2::KEY_RIGHT);
                    }
                } else {
                    tabs->SetActivePage(tabs->GetPage(static_cast<int>(index)));
                }
                Settle(*dialog, session);
                provider.lists.Complete();
                Settle(*dialog, session);
                Require(tabs->GetActivePageNum() == static_cast<int>(index) &&
                            session.settings.kind == BrowserTabKinds[index],
                        "tab traversal changed visual or stored tab identity");
                if (keyboardTabs) {
                    layout_fixture::FocusVisible(*tabs->GetActiveTab());
                }
                layout_fixture::Check(*dialog, 640, 384);
                auto *page = tabs->GetActivePage();
                const bool internet = BrowserTabKinds[index] == ListKind::Internet ||
                    BrowserTabKinds[index] == ListKind::Spectate;
                const bool favoritesPage = BrowserTabKinds[index] == ListKind::Favorites;
                Require(layout_fixture::Child(*page, "RefreshQuickButton")->IsVisible() == internet,
                        "quick-refresh capability differs from original page");
                Require(layout_fixture::Child(*page, "AddServerButton")->IsVisible() == favoritesPage,
                        "Add Server escaped Favorites");
            }
            if (keyboardTabs) {
                for (int index = static_cast<int>(BrowserTabKinds.size()) - 2; index >= 0; --index) {
                    layout_fixture::Key(*tabs->GetActiveTab(), vgui2::KEY_LEFT);
                    Settle(*dialog, session);
                    provider.lists.Complete();
                    Settle(*dialog, session);
                    Require(tabs->GetActivePageNum() == index &&
                                session.settings.kind == BrowserTabKinds[static_cast<size_t>(index)],
                            "Left key did not select the preceding original page");
                    layout_fixture::FocusVisible(*tabs->GetActiveTab());
                }
                auto *page = tabs->GetActivePage();
                auto *refresh = layout_fixture::Child(*page, "RefreshButton");
                layout_fixture::Key(*tabs->GetActiveTab(), vgui2::KEY_TAB);
                Settle(*dialog, session);
                layout_fixture::FocusVisible(*refresh);
                layout_fixture::Key(*refresh, vgui2::KEY_TAB);
                Settle(*dialog, session);
                layout_fixture::FocusVisible(*layout_fixture::Child(*page, "Filter"));
                // A Tab message dispatched through the ordinary page enters its first enabled resource control.
                layout_fixture::Key(*page, vgui2::KEY_TAB);
                Settle(*dialog, session);
                layout_fixture::FocusVisible(*refresh);
            }
            auto *reopenFocus = layout_fixture::Child(*tabs->GetActivePage(), "RefreshButton");
            reopenFocus->RequestFocus(0);
            Settle(*dialog, session);
            layout_fixture::FocusVisible(*reopenFocus);
            dialog->Deactivate();
            dialog->Activate();
            for (int turn = 0; turn < MaxFrames && !dialog->IsVisible(); ++turn) {
                dialog->Pump();
                gui->RunFrame();
            }
            Require(dialog->IsVisible(), "reopen did not finish bounded retirement");
            StartList(*dialog, session, provider);
            layout_fixture::FocusVisible(*reopenFocus);
            layout_fixture::Check(*dialog, 640, 384);
        } else if (name == "hd-saved") {
            Require(dialog->GetWide() == 1600 && dialog->GetTall() == 1200,
                    "saved physical dimensions were not clamped to the real workspace");
            layout_fixture::Check(*dialog, 1600, 1200);
            dialog->SetSize(1400, 1000);
            dialog->InvalidateLayout(true, false);
            Settle(*dialog, session);
            dialog->SaveSettings();
            Require(session.config.Load() && session.config.Settings().width == 1400 &&
                        session.config.Settings().height == 1000,
                    "saved physical dimensions were normalized or repeatedly scaled");
        } else if (name == "exact-window") {
            const size_t window = dialog->WindowSize();
            provider.lists.nextRows.clear();
            for (size_t index = 0; index < window; ++index) {
                provider.lists.nextRows.push_back(Server(0x7f000001U + static_cast<uint32_t>(index)));
            }
            Send(*dialog, "GetNewList");
            StartList(*dialog, session, provider);
            Require(session.presentation.VisibleCount() == dialog->WindowSize() &&
                        List(*dialog)->GetItemCount() == static_cast<int>(window),
                    "exact-window control did not retain every result");
            dialog->SelectVisible(0);
            Send(*dialog, "Details");
            auto *details = layout_fixture::Child(*dialog, "DialogGameInfo");
            for (int turn = 0; turn < 8; ++turn) {
                dialog->Pump();
                gui->RunFrame();
                vgui2::surface()->SolveTraverse(details->GetVPanel(), false);
            }
            auto *detailsList = static_cast<vgui2::ListPanel *>(layout_fixture::Child(*details, "DetailsList"));
            const size_t detailWindow = AllocatedSlots(*detailsList);
            for (size_t index = 0; index < detailWindow; ++index) {
                provider.players->AddPlayerToList("Player", static_cast<int>(index), 1.0f);
            }
            provider.players->PlayersRefreshComplete();
            provider.rules->RulesRefreshComplete();
            provider.players = nullptr;
            provider.rules = nullptr;
            for (int turn = 0; turn < 8; ++turn) {
                dialog->Pump();
                gui->RunFrame();
                vgui2::surface()->SolveTraverse(details->GetVPanel(), false);
                vgui2::surface()->PaintTraverse(details->GetVPanel());
            }
            Require(detailsList->GetItemCount() == static_cast<int>(detailWindow),
                    "exact-window details lost legitimate results");
        } else if (name == "bounded") {
            auto *list = List(*dialog);
            Require(list->GetTall() >= 100 && dialog->WindowSize() > 1,
                    ("real viewport layout never became usable: frame=" + std::to_string(dialog->GetTall()) +
                     " list=" + std::to_string(list->GetTall()) + " slots=" + std::to_string(dialog->WindowSize()))
                        .c_str());
            Require(session.presentation.VisibleCount() == ServerCount, "presentation silently dropped valid rows");
            for (int row = 0; row < list->GetItemCount(); ++row) {
                Require(list->GetItem(list->GetItemIDFromRow(row))->GetInt("slot") == row,
                        "actual displayed server order differs from canonical presentation slots");
            }
            Require(AllocatedSlots(*list) <= dialog->WindowSize(), "ListPanel allocated beyond viewport geometry");
            dialog->MoveSelection(1, true);
            Require(session.model.Selection().has_value(), "End key navigation lost selection");
            const auto last = session.model.Selection();
            Require(session.presentation.FirstVisibleRank() + list->GetItemCount() == ServerCount,
                    "logical scrollbar cannot reach final row");
            auto *sortMessage = new KeyValues("SetSortColumn", "column", 7);
            gui->PostMessage(list->GetVPanel(), sortMessage, 0, 0);
            Settle(*dialog, session);
            Require(session.model.Selection() == last, "header sort replaced RowKey selection");
            Require(AllocatedSlots(*list) <= dialog->WindowSize(), "sorting expanded ListPanel to full data set");
            dialog->SetSize(1000, 700);
            gui->RunFrame();
            dialog->Pump();
            Require(AllocatedSlots(*list) <= dialog->WindowSize(), "resize left a stale oversized slot pool");
        } else if (name == "input-actions") {
            auto *list = List(*dialog);
            auto key = [&](vgui2::KeyCode code) {
                gui->PostMessage(list->GetVPanel(), new KeyValues("KeyCodeTyped", "code", code), 0, 0);
                gui->RunFrame();
                dialog->Pump();
            };
            key(vgui2::KEY_END);
            const auto last = session.model.Selection();
            Require(last && last->ip == 0x7f000003U, "actual End message did not select final RowKey");
            key(vgui2::KEY_HOME);
            Require(session.model.Selection() && session.model.Selection()->ip == 0x7f000001U,
                    "actual Home message did not select first RowKey");
            key(vgui2::KEY_DOWN);
            Require(session.model.Selection()->ip == 0x7f000002U, "actual Down message lost RowKey identity");
            key(vgui2::KEY_ENTER);
            Require(engine.commands.size() == 1 && engine.commands.back() == "connect 127.0.0.2:27015\n",
                    "actual Enter message connected the wrong row");
            dialog->Activate();
            for (int turn = 0; !dialog->IsVisible() && turn < MaxFrames; ++turn) {
                dialog->Pump();
                gui->RunFrame();
            }
            Require(dialog->IsVisible(), "issued connection did not permit bounded reopen");
            gui->PostMessage(list->GetVPanel(), new KeyValues("MouseDoublePressed", "code", vgui2::MOUSE_LEFT), 0, 0);
            gui->RunFrame();
            Require(engine.commands.size() == 2 && engine.commands.back() == "connect 127.0.0.2:27015\n",
                    "actual double-click message lost selected RowKey");
        } else if (name == "inactive-close") {
            dialog->SelectVisible(0);
            Send(*dialog, "Details");
            constexpr size_t PendingDetailRows = 10000;
            for (size_t index = 0; index < PendingDetailRows; ++index) {
                provider.rules->RulesResponded("pending", "value");
            }
            Send(*dialog, "Connect");
            auto *prompt = dialog->FindChildByName("DialogServerPassword", true);
            auto *entry = dynamic_cast<vgui2::TextEntry *>(prompt->FindChildByName("PasswordEntry", false));
            entry->SetText("unsent");
            entry->SaveUndoState();
            dialog->Close();
            Require(entry->GetTextLength() == 0, "window close retained unsent password text");
            Require(vgui2::input()->GetAppModalSurface() != prompt->GetVPanel(),
                    "window close retained hidden password modality");
            entry->Undo();
            Require(entry->GetTextLength() == 0, "window close retained unsent password undo storage");
            gui->PostMessage(prompt->GetVPanel(), new KeyValues("Command", "command", "Connect"), 0, 0);
            gui->PostMessage(dialog->GetVPanel(), new KeyValues("Command", "command", "Favorite"), 0, 0);
            dialog->Activate();
            Require(!dialog->IsVisible(), "reopen bypassed pending detail retirement");
            size_t turns = 0;
            while (!dialog->IsVisible() && turns < MaxFrames) {
                dialog->Pump();
                gui->RunFrame();
                ++turns;
            }
            Require(dialog->IsVisible() && turns >= PendingDetailRows / DetailFrameBudget,
                    "hidden detail retirement was unbounded or reopening never completed");
            Require(provider.canceled == 2 && provider.lists.canceled == 1 && provider.lists.released == 1,
                    "ordinary window close did not cancel owned requests exactly once");
            Require(engine.commands.empty() && favorites.records.empty(),
                    "inactive queued action acquired new ownership");
            Require(session.presentation.VisibleCount() == 3, "ordinary close discarded copied rows");
            // Even without remaining provider/child storage, queued actions must cross an inactive dispatch.
            dialog->Deactivate();
            gui->PostMessage(dialog->GetVPanel(), new KeyValues("Command", "command", "Favorite"), 0, 0);
            dialog->Activate();
            dialog->Pump();
            Require(!dialog->IsVisible(), "empty hidden retirement bypassed queued-action dispatch");
            gui->RunFrame();
            dialog->Pump();
            Require(dialog->IsVisible() && favorites.records.empty(),
                    "inactive queued action ran during immediate reopen");
        } else if (name == "rapid-tabs" || name == "tab-selection-supersession") {
            auto *tabs = dynamic_cast<vgui2::PropertySheet *>(dialog->FindChildByName("GameTabs", true));
            dialog->Seek(ServerCount / 2);
            dialog->SelectVisible(1);
            const auto original = session.model.Selection();
            Require(original.has_value(), "large-list original selection was not established");
            auto switchOnce = [&](ListKind kind) {
                tabs->SetActivePage(tabs->GetPage(static_cast<int>(std::find(BrowserTabKinds.begin(), BrowserTabKinds.end(), kind) - BrowserTabKinds.begin())));
                dialog->Pump();
                gui->RunFrame();
                Require(session.settings.kind == kind, "rapid tab intention was not admitted");
            };
            auto completeInternet = [&] {
                Settle(*dialog, session);
                Require(provider.lists.lastKind == ListKind::Internet && provider.lists.requests.size() == 1,
                        "coalesced Internet request did not start after bounded retirement");
                provider.lists.Complete();
                Settle(*dialog, session);
            };
            std::optional<RowKey> expected = original;
            if (name == "tab-selection-supersession") {
                auto &rows = provider.lists.nextRows;
                const auto originalRow = std::find_if(rows.begin(), rows.end(), [&](const gameserveritem_t &row) {
                    return row.m_NetAdr.GetIP() == original->ip;
                });
                Require(originalRow != rows.end(), "supersession fixture could not locate original provider row");
                const auto omitted = *originalRow;
                rows.erase(originalRow);
                switchOnce(ListKind::Favorites);
                switchOnce(ListKind::Internet);
                completeInternet();
                Require(!session.model.Selection() && !session.model.Find(*original),
                        "missing original key did not leave restoration pending");
                dialog->SelectVisible(0);
                expected = session.model.Selection();
                Require(expected && !(expected == original), "explicit replacement selection was not established");
                rows.push_back(omitted);
                switchOnce(ListKind::Favorites);
                Settle(*dialog, session);
                switchOnce(ListKind::Internet);
                completeInternet();
                Require(session.model.Selection() == expected,
                        "pending saved selection overrode an explicit user replacement");
            } else {
                switchOnce(ListKind::Favorites);
                switchOnce(ListKind::Internet);
                Require(session.model.GetStatus().viewUpdating && !session.model.Selection() &&
                            !session.model.Find(*original),
                        "large-list return did not enter the pending-restoration window");
                switchOnce(ListKind::History);
                switchOnce(ListKind::Internet);
                completeInternet();
                Require(session.model.Selection() == original,
                        "rapid tab intentions overwrote the pending saved RowKey with live null");
            }
            gui->PostMessage(List(*dialog)->GetVPanel(), new KeyValues("KeyCodeTyped", "code", vgui2::KEY_ENTER), 0, 0);
            gui->RunFrame();
            Require(engine.commands.size() == 1 &&
                        engine.commands.back() == "connect " + ConnectionAddress(*expected) + "\n",
                    "restored tab selection connected an unexpected numeric endpoint");
        } else if (name == "region-tabs") {
            auto *tabs = static_cast<vgui2::PropertySheet *>(dialog->FindChildByName("GameTabs", false));
            auto *map = static_cast<vgui2::TextEntry *>(tabs->GetActivePage()->FindChildByName("MapFilter", false));
            map->SetText("DE_");
            gui->RunFrame();
            Settle(*dialog, session);
            for (size_t index = 1; index < BrowserTabKinds.size(); ++index) {
                tabs->SetActivePage(tabs->GetPage(static_cast<int>(index)));
                Settle(*dialog, session);
                provider.lists.Complete();
                Settle(*dialog, session);
                Require(session.settings.kind == BrowserTabKinds[index], "stable tab mapping changed");
                Require(session.settings.filters.map.empty(), "page inherited another page's map filter");
            }
            tabs->SetActivePage(tabs->GetPage(0));
            Settle(*dialog, session);
            Require(session.settings.filters.map == "DE_", "returning to Internet lost per-page filters");
        } else if (name == "status-fields") {
            auto *status = static_cast<vgui2::Label *>(dialog->FindChildByName("StatusLabel", false));
            Require(status && !dialog->FindChildByName("ActionStatusLabel", true), "original status parentage changed");
            const auto displayed = [&] {
                std::array<char, 512> text{};
                status->GetText(text.data(), static_cast<int>(text.size()));
                return std::string(text.data());
            };
            const auto transitionQuery = [&](const std::string &expected) {
                Require(session.model.QuickRefresh(), "status regression did not enter a new refresh");
                dialog->Pump();
                Require(session.model.GetStatus().phase == Phase::Refreshing,
                        "status regression did not observe refreshing state");
                Require(displayed() == expected, "query start erased an unchanged failure");
                provider.lists.Complete();
                Settle(*dialog, session);
                Require(session.model.GetStatus().phase == Phase::Complete,
                        "status regression did not observe completed state");
                Require(displayed() == expected, "query completion erased an unchanged failure");
            };
            favorites.failing = true;
            dialog->SelectVisible(0);
            Send(*dialog, "Favorite");
            dialog->Pump();
            const auto actionFailure = session.model.GetStatus().actionError;
            Require(!actionFailure.empty() && displayed() == actionFailure,
                    "action failure is not surfaced in original status label");
            transitionQuery(actionFailure);
            favorites.failing = false;
            Send(*dialog, "Favorite");
            dialog->Pump();
            Require(session.model.GetStatus().actionError.empty() && displayed().empty(),
                    "successful action recovery did not restore the completed query's empty status");

            dialog->SaveSettings();
            const auto settingsPath = std::filesystem::path(session.storage.Directory()) / "settings.cfg";
            const auto preservedPath = settingsPath.parent_path() / "status-regression-preserved.cfg";
            std::filesystem::rename(settingsPath, preservedPath);
            std::filesystem::create_directory(settingsPath);
            dialog->SaveSettings();
            dialog->Pump();
            Require(!session.config.Status().error.empty(), "status regression did not produce config failure");
            const auto configFailure = displayed();
            std::array<char, 512> expectedConfigFailure{};
            vgui2::localize()->ConvertUnicodeToANSI(vgui2::localize()->Find("CSEServerBrowser_Unsaved"),
                expectedConfigFailure.data(), static_cast<int>(expectedConfigFailure.size()));
            Require(configFailure == expectedConfigFailure.data(), "config failure was not displayed");
            transitionQuery(configFailure);
            std::filesystem::remove(settingsPath);
            std::filesystem::rename(preservedPath, settingsPath);
            dialog->SaveSettings();
            dialog->Pump();
            Require(session.config.Status().error.empty() && displayed().empty(),
                    "successful config recovery did not restore the completed query's empty status");
        } else if (name == "navigation-sort" || name == "navigation-update") {
            dialog->SelectVisible(1);
            const auto selected = session.model.Selection();
            Require(selected.has_value(), "navigation fixture selection absent");
            if (name == "navigation-sort") {
                dialog->ChooseSort(4);
            } else {
                auto &request = provider.lists.requests.at(provider.lists.current);
                for (size_t index = 0; index < request.rows.size(); ++index) {
                    if (request.rows[index].m_NetAdr.GetIP() == selected->ip) {
                        request.rows[index].SetName("zzzz moved selection");
                        request.callback->ServerResponded(provider.lists.current, static_cast<int>(index));
                        break;
                    }
                }
            }
            Settle(*dialog, session);
            auto *list = List(*dialog);
            Require(list->GetSelectedItemsCount() == 0, "fixture did not move the selected non-anchor key offviewport");
            if (name == "navigation-update") {
                dialog->ChooseSort(7);
                dialog->Pump();
                Require(session.presentation.IsUpdating(), "pending-sort navigation fixture settled prematurely");
            }
            auto keys = session.model.OrderedKeys();
            const auto activeSort = session.presentation.ActiveSort();
            std::sort(keys.begin(), keys.end(), [&](const RowKey &left, const RowKey &right) {
                return RowPrecedes(*session.model.Find(left), *session.model.Find(right), activeSort);
            });
            auto found = std::find(keys.begin(), keys.end(), *selected);
            Require(found != keys.end(), "selected row left full filtered model unexpectedly");
            size_t rank = static_cast<size_t>(found - keys.begin());
            auto navigate = [&](vgui2::KeyCode code, long long delta) {
                rank = static_cast<size_t>(
                    (std::clamp)(static_cast<long long>(rank) + delta, 0LL, static_cast<long long>(keys.size() - 1)));
                gui->PostMessage(list->GetVPanel(), new KeyValues("KeyCodeTyped", "code", code), 0, 0);
                gui->RunFrame();
                dialog->Pump();
                Require(session.model.Selection() == keys[rank],
                        "keyboard navigation used viewport anchor instead of selected RowKey");
            };
            navigate(vgui2::KEY_UP, -1);
            navigate(vgui2::KEY_DOWN, 1);
            navigate(vgui2::KEY_PAGEUP, -static_cast<long long>(dialog->WindowSize()));
            navigate(vgui2::KEY_PAGEDOWN, static_cast<long long>(dialog->WindowSize()));
            gui->PostMessage(list->GetVPanel(), new KeyValues("KeyCodeTyped", "code", vgui2::KEY_ENTER), 0, 0);
            gui->RunFrame();
            Require(engine.commands.size() == 1 &&
                        engine.commands.back() == "connect " + ConnectionAddress(keys[rank]) + "\n",
                    "post-navigation Enter connected an unexpected endpoint");
        } else if (name == "tabs") {
            auto *tabs = dynamic_cast<vgui2::PropertySheet *>(dialog->FindChildByName("GameTabs", true));
            Require(tabs && tabs->GetNumPages() == 6, "retained tabs missing");
            for (int index = 0; index < tabs->GetNumPages(); ++index) {
                tabs->SetActivePage(tabs->GetPage(index));
                dialog->Pump();
                Require(provider.lists.lastKind == BrowserTabKinds[static_cast<size_t>(index)], "tab dispatched wrong typed request");
                provider.lists.Complete();
                Settle(*dialog, session);
            }
        } else if (name == "original-keyboard-range") {
            auto *list = List(*dialog);
            auto *input = static_cast<vgui2::IInputInternal *>(guiFactory(VGUI_INPUTINTERNAL_INTERFACE_VERSION, nullptr));
            Require(input != nullptr, "actual input interface missing");
            const auto keys = session.model.OrderedKeys();
            const size_t anchor = dialog->WindowSize() - 1;
            Require(anchor + 2 < keys.size(), "keyboard fixture does not span viewport boundary");
            dialog->SelectVisible(anchor);
            // The default context polls physical SDL modifiers, which would replace
            // injected Shift immediately. A real private VGUI context owns fixture input.
            struct InputContextScope {
                vgui2::IVGui *gui;
                vgui2::HContext context;
                ~InputContextScope() {
                    constexpr vgui2::HContext DefaultVguiContext = static_cast<vgui2::HContext>(~0);
                    gui->ActivateContext(DefaultVguiContext);
                    gui->DestroyContext(context);
                }
            } controlledInput{gui, gui->CreateContext()};
            gui->AssociatePanelWithContext(controlledInput.context, dialog->GetVPanel());
            gui->ActivateContext(controlledInput.context);
            list->RequestFocus(0);
            input->InternalKeyCodePressed(vgui2::KEY_LSHIFT);
            Require(input->IsKeyDown(vgui2::KEY_LSHIFT), "controlled VGUI context did not retain injected Shift");
            std::cerr << "TRACE shift after press=" << input->IsKeyDown(vgui2::KEY_LSHIFT) << " anchor=" << anchor << "\n";
            const auto key = [&](vgui2::KeyCode code) {
                gui->PostMessage(list->GetVPanel(), new KeyValues("KeyCodeTyped", "code", code), 0, 0);
                gui->RunFrame();
                dialog->Pump();
                if (code != vgui2::KEY_ENTER) {
                    Require(input->IsKeyDown(vgui2::KEY_LSHIFT),
                            "native event changed the controlled modifier before navigation completed");
                }
            };
            key(vgui2::KEY_DOWN);
            std::cerr << "TRACE after down1 shift=" << input->IsKeyDown(vgui2::KEY_LSHIFT)
                << " rank=" << session.presentation.FirstVisibleRank() << " selected="
                << ConnectionAddress(*session.model.Selection()) << "\n";
            key(vgui2::KEY_DOWN);
            std::cerr << "TRACE after down2 shift=" << input->IsKeyDown(vgui2::KEY_LSHIFT)
                << " rank=" << session.presentation.FirstVisibleRank() << " selected="
                << ConnectionAddress(*session.model.Selection()) << "\n";
            Require(session.presentation.FirstVisibleRank() > anchor,
                    "actual Shift navigation did not cross the viewport boundary");
            Send(*dialog, "AddToFavorites");
            std::cerr << "TRACE range favorites=" << favorites.records.size() << "\n";
            for (const auto &record : favorites.records) {
                std::cerr << "TRACE favorite=" << ConnectionAddress(record.address) << "\n";
            }
            Require(favorites.records.size() == 3 && favorites.records[0].address == keys[anchor] &&
                favorites.records[1].address == keys[anchor + 1] && favorites.records[2].address == keys[anchor + 2],
                "actual repeated Shift+Down did not select anchor through third row");
            favorites.records.clear();
            key(vgui2::KEY_UP);
            Send(*dialog, "AddToFavorites");
            Require(favorites.records.size() == 2 && favorites.records[0].address == keys[anchor] &&
                favorites.records[1].address == keys[anchor + 1], "actual Shift+Up did not shrink the range");
            Require(session.model.Selection() == std::optional<RowKey>{keys[anchor]},
                    "actual range extension changed first-selected identity");
            input->InternalKeyCodeReleased(vgui2::KEY_LSHIFT);
            key(vgui2::KEY_ENTER);
            Require(engine.commands.empty() && provider.pings == 1, "Enter skipped fresh anchored metadata");
            auto item = Server(keys[anchor].ip);
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            Require(engine.commands == std::vector<std::string>{"connect " + ConnectionAddress(keys[anchor]) + "\n"},
                    "actual Enter connected keyboard cursor instead of first-selected anchor");
        } else if (name == "original-info-stop" || name == "original-connect-stop") {
            Send(*dialog, "GetNewList");
            Settle(*dialog, session);
            const auto request = provider.lists.current;
            provider.lists.requests.at(request).callback->ServerResponded(request, 0);
            Settle(*dialog, session);
            Require(session.model.GetStatus().phase == Phase::Refreshing, "source list is not actively refreshing");
            dialog->SelectVisible(0);
            const auto captured = *session.model.Selection();
            const int canceled = provider.lists.canceled;
            provider.lists.cancelCallbacks = true;
            Send(*dialog, name == "original-info-stop" ? "ViewGameInfo" : "ConnectToServer");
            dialog->Pump();
            gui->RunFrame();
            Require(session.model.GetStatus().phase == Phase::Stopped && provider.lists.canceled == canceled + 1,
                    "opening original GameInfo did not cancel/stop the live source query");
            auto *info = layout_fixture::Child(*dialog, "DialogGameInfo");
            auto *address = static_cast<vgui2::TextEntry *>(layout_fixture::Child(*info, "ServerIPText"));
            char text[64]{};
            address->GetText(text, sizeof(text));
            Require(ConnectionAddress(captured) == text, "source cancellation changed captured GameInfo endpoint");
            Require(provider.pings == 1 && provider.pingPorts.front() == captured.queryPort && engine.commands.empty(),
                    "source Stop lost independent child ping or bypassed metadata");
            auto item = Server(captured.ip);
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            if (name == "original-connect-stop") {
                Require(engine.commands == std::vector<std::string>{"connect " + ConnectionAddress(captured) + "\n"},
                        "Join lost the snapshot after stopping its source list");
            } else {
                Require(engine.commands.empty() && provider.players,
                        "ViewGameInfo should query players without connecting");
            }
        } else if (name == "original-add-server") {
            Send(*dialog, "AddServerByName");
            auto *add = layout_fixture::Child(*dialog, "DialogAddServer");
            Require(add->GetWide() == 572 && add->GetTall() == 205, "Add Server original collapsed extent changed");
            auto *entry = static_cast<vgui2::TextEntry *>(layout_fixture::Child(*add, "ServerNameText"));
            Require(!layout_fixture::Child(*add, "GameTabs")->IsVisible(), "Add Server test page initially visible");
            vgui2::DHANDLE<vgui2::Panel> firstAdd;
            firstAdd = add;
            entry->SetText("127.0.0.7");
            Send(*add, "OK");
            for (int frame = 0; frame < 8; ++frame) {
                dialog->Pump();
                gui->RunFrame();
            }
            Require(favorites.records.size() == 1 && favorites.records[0].address == Address{0x7f000007U, 27015, 27015},
                    "direct Add Server did not use entered address/default port");
            Require(provider.pings == 0, "direct add incorrectly requires ping response");
            Send(*dialog, "AddServerByName");
            add = layout_fixture::Child(*dialog, "DialogAddServer");
            entry = static_cast<vgui2::TextEntry *>(layout_fixture::Child(*add, "ServerNameText"));
            // Finish native popup creation before injecting a ready-window command.
            // SurfaceSDL dispatches queued VGUI messages between creation/resize events.
            constexpr int PopupCreationFrames = 8;
            for (int frame = 0; frame < PopupCreationFrames; ++frame) {
                gui->RunFrame();
                dialog->Pump();
            }
            Require(!firstAdd.Get() && add->IsVisible() && add->GetTall() == 205 && provider.pings == 0,
                    "reopened AddServer did not settle at its original collapsed state");
            std::cerr << "TRACE reopen oldAlive=" << (firstAdd.Get() != nullptr)
                << " same=" << (firstAdd.Get() == add) << " tall=" << add->GetTall()
                << " visible=" << add->IsVisible() << " active=" << session.active
                << " closing=" << session.closing << " depth=" << session.useDepth << "\n";
            entry->SetText("127.0.0.8");
            Send(*add, "TestServers");
            dialog->Pump();
            std::cerr << "TRACE test tall=" << add->GetTall() << " pings=" << provider.pings
                << " visible=" << add->IsVisible() << " active=" << session.active
                << " closing=" << session.closing << " depth=" << session.useDepth << "\n";
            Require(add->GetTall() == 471 && provider.pings == 14, "original expanded test/14 ports missing");
            const std::vector<uint16_t> expected{27015, 26900, 27016, 26901, 27017, 26902, 27018,
                26903, 27019, 26904, 27020, 26905, 4242, 27215};
            Require(provider.pingPorts == expected, "Add Server changed original common-port order");
            entry->SetText("127.0.0.9:27020");
            Send(*add, "TestServers");
            dialog->Pump();
            Require(provider.canceled == 14 && provider.pings == 15, "retest failed to retire old generation");
            auto *servers = static_cast<vgui2::ListPanel *>(layout_fixture::Child(*add, "Servers"));
            Require(servers->GetItemCount() == 0, "old canceled ping contaminated new test rows");
            auto item = Server(0x7f000009U, "returned endpoint");
            item.m_NetAdr.Init(0x7f000009U, 27020, 27021);
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            Require(servers->GetItemCount() == 1, "valid tested server response absent");
            Send(*add, "addselected");
            Require(favorites.records.size() == 2 &&
                favorites.records.back().address == Address{0x7f000009U, 27021, 27020},
                "addselected lost distinct connection/query ports");
            Require(add->IsVisible() && servers->GetItemCount() == 0, "selected add must remove row and stay open");
            Send(*add, "Close");
        } else if (name == "original-password") {
            dialog->SelectVisible(0);
            const auto target = *session.model.Selection();
            Send(*dialog, "Connect");
            dialog->Pump();
            Require(provider.pings == 1 && engine.commands.empty(), "Connect dispatched without fresh metadata");
            auto item = Server(target.ip);
            item.m_bPassword = true;
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            auto *info = layout_fixture::Child(*dialog, "DialogGameInfo");
            Require(info->FindChildByName("PlayerList", false) && !info->FindChildByName("DetailsOverflow", false),
                    "GameInfo uses custom presentation instead of original resource controls");
            auto *prompt = layout_fixture::Child(*info, "DialogServerPassword");
            auto *entry = static_cast<vgui2::TextEntry *>(layout_fixture::Child(*prompt, "PasswordEntry"));
            entry->SetText("discarded");
            Send(*prompt, "Close");
            dialog->Pump();
            gui->RunFrame();
            Require(engine.commands.empty() && engine.password == "before", "cancelled password connected/mutated cvar");
            Send(*info, "Connect");
            dialog->Pump();
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            prompt = layout_fixture::Child(*info, "DialogServerPassword");
            entry = static_cast<vgui2::TextEntry *>(layout_fixture::Child(*prompt, "PasswordEntry"));
            entry->SetText("secret");
            dialog->SelectVisible(1);
            Send(*prompt, "Connect");
            const int before = provider.pings;
            dialog->Pump();
            Require(provider.pings == before + 1 && engine.commands.empty(), "password result skipped fresh ping");
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            Require(engine.commands == std::vector<std::string>{"connect " + ConnectionAddress(target) + "\n"},
                    "password submission connected a changed selection");
            Require(engine.password == "secret", "password ownership did not reach safe engine setter");
        } else if (playerRefresh) {
            dialog->SelectVisible(0);
            const auto target = *session.model.Selection();
            Send(*dialog, "ViewGameInfo");
            dialog->Pump();
            auto *info = layout_fixture::Child(*dialog, "DialogGameInfo");
            auto item = Server(target.ip);
            item.m_nPlayers = 20;
            item.m_nBotPlayers = 20;
            item.m_nMaxPlayers = 20;
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            gui->RunFrame();
            Require(provider.players && !provider.rules, "GameInfo did not use original PlayerDetails-only flow");
            auto *players = static_cast<vgui2::ListPanel *>(layout_fixture::Child(*info, "PlayerList"));
            constexpr size_t ReplacementCount = DetailFrameBudget * 3 + 1;
            const auto pumpPlayers = [&] {
                for (size_t frame = 0; frame < ReplacementCount; ++frame) {
                    dialog->Pump();
                    gui->RunFrame();
                }
            };
            const auto populate = [&](const char *prefix) {
                for (size_t index = 0; index < ReplacementCount; ++index) {
                    const auto playerName = std::string(prefix) + std::to_string(index);
                    provider.players->AddPlayerToList(playerName.c_str(), static_cast<int>(index), 60.0f);
                }
            };
            populate("old-");
            provider.players->PlayersRefreshComplete();
            pumpPlayers();
            Require(players->GetItemCount() == static_cast<int>(ReplacementCount),
                "initial player generation did not span frame budgets");
            auto *oldPlayers = provider.players;
            Send(*info, "Refresh");
            dialog->Pump();
            Require(provider.players == oldPlayers, "metadata refresh retired independent players early");
            Require(players->GetItemCount() == static_cast<int>(ReplacementCount),
                "merely starting metadata cleared independent visible players");
            if (name == "original-game-info-sync-empty") {
                provider.duringStart = [&] { provider.players->PlayersRefreshComplete(); };
            }
            auto empty = item;
            empty.m_nPlayers = 0;
            empty.m_nBotPlayers = 0;
            provider.pingCallbacks.rbegin()->second->ServerResponded(empty);
            dialog->Pump();
            Require(players->GetItemCount() > 0 &&
                players->GetItemCount() >= static_cast<int>(ReplacementCount - DetailFrameBudget),
                "replacement cleared more than one display frame budget");
            if (name != "original-game-info-sync-empty") {
                provider.players->PlayersRefreshComplete();
            }
            pumpPlayers();
            Require(players->GetItemCount() == 0, "successful empty player generation left stale visible rows");
            geometry.paintedText.clear();
            geometry.observeText = true;
            vgui2::surface()->SolveTraverse(info->GetVPanel(), false);
            vgui2::surface()->PaintTraverse(info->GetVPanel());
            geometry.observeText = false;
            const auto *noPlayers = vgui2::localize()->Find("ServerBrowser_ServerHasNoPlayers");
            Require(noPlayers && geometry.paintedText.find(noPlayers) != std::wstring::npos,
                "empty actual PlayerList did not paint original no-players text");
            provider.duringStart = {};
            Send(*info, "Refresh");
            dialog->Pump();
            provider.pingCallbacks.rbegin()->second->ServerResponded(item);
            dialog->Pump();
            populate("new-");
            provider.players->PlayersRefreshComplete();
            dialog->Pump();
            Require(players->GetItemCount() <= static_cast<int>(DetailFrameBudget),
                "replacement applied more than one player frame budget");
            pumpPlayers();
            Require(players->GetItemCount() == static_cast<int>(ReplacementCount),
                "nonempty replacement lost rows across frame budgets");
            for (int row = 0; row < players->GetItemCount(); ++row) {
                Require(std::string(players->GetItem(players->GetItemIDFromRow(row))->GetString("PlayerName"))
                    .find("new-") == 0, "replacement revived an old player row");
            }
            if (name == "original-game-info-generation") {
                Send(*info, "Refresh");
                dialog->Pump();
                provider.pingCallbacks.rbegin()->second->ServerResponded(item);
                dialog->Pump();
                populate("superseded-");
                dialog->Pump();
                // Leave this generation incomplete with queued rows during bounded clearing.
                auto *independentPlayers = provider.players;
                const int cancellations = provider.canceled;
                Send(*info, "Refresh");
                dialog->Pump();
                Require(provider.players == independentPlayers && provider.canceled == cancellations,
                    "starting metadata retired an active independent player generation");
                provider.pingCallbacks.rbegin()->second->ServerResponded(empty);
                dialog->Pump();
                provider.players->PlayersRefreshComplete();
                pumpPlayers();
                Require(players->GetItemCount() == 0, "superseded pending rows revived after empty successor");
                Send(*info, "Refresh");
                dialog->Pump();
                provider.pingCallbacks.rbegin()->second->ServerResponded(item);
                dialog->Pump();
                populate("closed-");
                dialog->Pump();
                vgui2::DHANDLE<vgui2::Panel> closingInfo;
                closingInfo = info;
                Send(*info, "Close");
                pumpPlayers();
                Require(!closingInfo.Get(), "closed player generation retained its actual dialog");
                Require(engine.commands.empty(), "closing queued player generation unexpectedly connected");
            } else {
                Send(*info, "Connect");
                dialog->Pump();
                provider.pingCallbacks.rbegin()->second->ServerResponded(item);
                dialog->Pump();
                gui->RunFrame();
                Require(engine.commands.empty(), "bot-only full server incorrectly treated human count as free slot");
                auto *retry = static_cast<vgui2::ToggleButton *>(layout_fixture::Child(*info, "AutoRetry"));
                Require(retry->IsVisible(), "full server did not expose original AutoRetry control");
                const int normalHeight = info->GetTall();
                retry->DoClick();
                gui->RunFrame();
                Require(info->GetTall() == normalHeight + 60 &&
                    layout_fixture::Child(*info, "AutoRetryAlert")->IsVisible(),
                    "retry resource did not add original 60-pixel options area");
                Send(*info, "Connect");
                dialog->Pump();
                Send(*info, "Close");
                item.m_nPlayers = 0;
                provider.pingCallbacks.rbegin()->second->ServerResponded(item);
                dialog->Pump();
                Require(engine.commands.empty(), "closed GameInfo connected after late metadata callback");
            }
        } else if (name == "original-multiselect") {
            dialog->SelectVisibleItems({0, 1});
            const auto first = *session.model.Selection();
            dialog->ChooseSort(4);
            Settle(*dialog, session);
            Send(*dialog, "AddToFavorites");
            Require(favorites.records.size() == 2, "batch favorite ignored stable multiselection");
            provider.deferListRefresh = true;
            const size_t before = provider.refreshedIndexes.size();
            Send(*dialog, "RefreshServer");
            Require(provider.refreshedIndexes.size() == before + 2,
                    "selected refresh did not visit both identities before callbacks");
            const auto request = provider.lists.current;
            auto &controlled = provider.lists.requests.at(request);
            for (size_t index = 0; index < favorites.records.size(); ++index) {
                const int refreshed = provider.refreshedIndexes[before + index];
                const auto &address = controlled.rows.at(static_cast<size_t>(refreshed)).m_NetAdr;
                Require(address.GetIP() == favorites.records[index].address.ip &&
                    address.GetConnectionPort() == favorites.records[index].address.connectionPort,
                    "selected refresh retargeted a sorted identity");
            }
            controlled.callback->ServerResponded(request, provider.refreshedIndexes[before + 1]);
            Require(session.model.GetStatus().phase == Phase::Refreshing,
                    "one selected response prematurely completed both queries");
            controlled.callback->ServerResponded(request, provider.refreshedIndexes[before]);
            dialog->Pump();
            Require(session.model.GetStatus().phase == Phase::Complete, "selected refresh failed to complete");
            Require(session.model.Selection() && *session.model.Selection() == first,
                    "sort changed first-selected connect identity");
            for (const auto &favorite : favorites.records) {
                session.model.RecordAcceptedConnection(10, favorite.address, 123);
            }
            Send(*dialog, "RemoveFromHistory");
            for (const auto &favorite : favorites.records) {
                Require(favorite.flags == k_unFavoriteFlagFavorite, "History batch removed Favorite flags");
            }
            StartList(*dialog, session, provider);
            Send(*dialog, "AddToFavorites");
            Require(favorites.records.size() == 2, "stale generation selection targeted recycled viewport slots");
        } else if (name == "original-add-current") {
            unsigned char bytes[]{127, 0, 0, 9};
            uint32_t raw = 0;
            std::memcpy(&raw, bytes, sizeof(raw));
            session.connection.Accepted("cstrike", raw, 27018, 123);
            dialog->SelectVisible(0);
            Send(*dialog, "AddCurrentServer");
            Require(favorites.records.size() == 1 && favorites.records[0].address == Address{0x7f000009U, 27018, 27018},
                    "Add Current used selected row or normalized host IP twice");
            Require(favorites.records[0].flags == (k_unFavoriteFlagFavorite | k_unFavoriteFlagHistory),
                    "Add Current lost independent History flag");
            Require(!dialog->FindChildByName("DialogAddServer", true), "Add Current incorrectly opened child dialog");
            session.connection.Disconnected();
            Send(*dialog, "AddCurrentServer");
            Require(favorites.records.size() == 1, "disconnected Add Current retained stale endpoint");
        } else if (name == "actions") {
            dialog->SelectVisible(0);
            Send(*dialog, "Favorite");
            Require(favorites.records.size() == 1, "favorite control failed to call actual model owner");
            Send(*dialog, "RemoveFavorite");
            Require((favorites.records[0].flags & k_unFavoriteFlagFavorite) == 0,
                    "remove control lost favorite semantics");
            dialog->ShowContextMenu();
            auto *menu = dynamic_cast<vgui2::Menu *>(dialog->FindChildByName("ServerContext", false));
            Require(menu && menu->GetItemCount() == 4, "actual retained row context actions missing");
            menu->ActivateItemByRow(3);
            gui->RunFrame();
            Require((favorites.records.back().flags & k_unFavoriteFlagFavorite) != 0,
                    "actual context-menu favorite action did not reach selected RowKey");
            Send(*dialog, "QuickSelected");
            Require(provider.lists.lastIndex >= 0, "selected quick refresh did not reach provider");
            const int previousRefreshes = provider.lists.refreshed;
            Send(*dialog, "refresh");
            Require(provider.lists.refreshed == previousRefreshes + 1, "full quick refresh missed live request");
            provider.lists.Complete();
            Settle(*dialog, session);
            Send(*dialog, "GetNewList");
            Settle(*dialog, session);
            Send(*dialog, "stoprefresh");
            Require(session.model.GetStatus().phase == Phase::Stopped, "Stop control left request refreshing");
            Send(*dialog, "GetNewList");
            Settle(*dialog, session);
            provider.lists.Complete();
            Settle(*dialog, session);
            dialog->SelectVisible(0);
            const auto favoriteTimestamp = favorites.records[0].lastPlayed;
            Send(*dialog, "Connect");
            Require(engine.commands.size() == 1 && engine.commands.front().find("connect 127.") == 0,
                    "real connect action did not use numeric endpoint");
            Require(favorites.records[0].lastPlayed == favoriteTimestamp &&
                        (favorites.records[0].flags & k_unFavoriteFlagHistory) == 0,
                    "connect control wrote accepted history prematurely");
        } else if (name == "filters") {
            auto *tabs = static_cast<vgui2::PropertySheet *>(dialog->FindChildByName("GameTabs", false));
            auto *page = tabs->GetActivePage();
            auto *map = static_cast<vgui2::TextEntry *>(layout_fixture::Child(*page, "MapFilter"));
            auto *ping = static_cast<vgui2::ComboBox *>(layout_fixture::Child(*page, "PingFilter"));
            auto *passwordFilter = static_cast<vgui2::CheckButton *>(layout_fixture::Child(*page, "NoPasswordFilterCheck"));
            auto *account = static_cast<vgui2::CheckButton *>(layout_fixture::Child(*page, "ValidSteamAccountFilterCheck"));
            const auto request = provider.lists.current;
            map->SetText("dE_dU");
            ping->ActivateItem(1);
            passwordFilter->SetSelected(true);
            gui->RunFrame();
            Settle(*dialog, session);
            Require(session.presentation.VisibleCount() == 3, "mixed-case prefix filter excluded matching maps");
            Require(provider.lists.current == request, "secondary filter unexpectedly restarted discovery");
            account->SetSelected(true);
            gui->RunFrame();
            Settle(*dialog, session);
            Require(session.presentation.VisibleCount() == 0, "missing server account passed original account filter");
            account->SetSelected(false);
            map->SetText("dust");
            gui->RunFrame();
            Settle(*dialog, session);
            Require(session.presentation.VisibleCount() == 0, "map filter matched a substring instead of a prefix");
        } else if (name == "unsaved") {
            dialog->SaveSettings();
            Settle(*dialog, session);
            Require(!session.config.Status().error.empty(), "fixture did not produce a settings save failure");
            auto *label = dynamic_cast<vgui2::Label *>(dialog->FindChildByName("StatusLabel", true));
            Require(label != nullptr, "status label absent");
            std::array<wchar_t, 256> status{};
            label->GetText(status.data(), static_cast<int>(status.size()));
            Require(std::wstring(status.data()) == vgui2::localize()->Find("CSEServerBrowser_Unsaved"),
                    "failed save was not shown plainly to the user");
            dialog->SelectVisible(0);
            Send(*dialog, "Connect");
            Require(engine.commands.size() == 1, "failed settings save disabled otherwise usable browser");
        } else if (name == "favorite-error") {
            dialog->SelectVisible(0);
            Send(*dialog, "Favorite");
            favorites.failing = true;
            Send(*dialog, "RemoveFavorite");
            Require(!session.model.GetStatus().actionError.empty(), "favorite failure was not observable");
            auto invalidFilters = session.settings.filters;
            invalidFilters.region = 3;
            Require(!session.model.Refresh(ListKind::LAN, invalidFilters), "fixture did not produce query failure");
            const auto queryError = session.model.GetStatus().error;
            favorites.failing = false;
            Send(*dialog, "Favorite");
            Require(session.model.GetStatus().actionError.empty() && session.model.GetStatus().error == queryError,
                    "successful no-op favorite erased query failure or retained obsolete action failure");
        } else if (name == "password") {
            dialog->SelectVisible(0);
            Send(*dialog, "Connect");
            auto *prompt = dialog->FindChildByName("DialogServerPassword", true);
            Require(prompt && prompt->FindChildByName("CancelButton", false), "installed password resource missing");
            auto *entry = dynamic_cast<vgui2::TextEntry *>(prompt->FindChildByName("PasswordEntry", false));
            Require(entry != nullptr, "password control was not code-bound");
            auto *settings = new KeyValues("PasswordSettings");
            entry->GetSettings(settings);
            Require(settings->GetInt("textHidden") == 1 && settings->GetInt("maxchars") == 127,
                    "password entry is unmasked or unbounded");
            settings->deleteThis();
            entry->SetText("p\303\244ss");
            entry->SaveUndoState();
            auto *command = new KeyValues("Command", "command", "Connect");
            prompt->OnMessage(command, 0);
            command->deleteThis();
            Require(entry->GetTextLength() == 0, "connect left visible password storage populated");
            entry->Undo();
            Require(entry->GetTextLength() == 0, "password undo restored retired secret");
            Require(engine.password == "p\303\244ss" && engine.commands.size() == 1 &&
                        engine.commands.front() == "connect 127.0.0.1:27015\n",
                    "password entered command text or was lost");
            gui->RunFrame();
            session.connection.Disconnected();
            Require(engine.password == "before", "password owner did not restore previous cvar value");
            dialog->Activate();
            for (int turn = 0; !dialog->IsVisible() && turn < MaxFrames; ++turn) {
                dialog->Pump();
                gui->RunFrame();
            }
            Require(dialog->IsVisible(), "password retry did not wait for ordinary retirement");
            dialog->SelectVisible(1);
            Send(*dialog, "Connect");
            prompt = dialog->FindChildByName("DialogServerPassword", true);
            Require(prompt != nullptr, "explicit password retry did not create a fresh prompt");
            entry = dynamic_cast<vgui2::TextEntry *>(prompt->FindChildByName("PasswordEntry", false));
            entry->SetText("second");
            gui->PostMessage(prompt->GetVPanel(), new KeyValues("Command", "command", "Connect"), 0, 0);
            gui->PostMessage(prompt->GetVPanel(), new KeyValues("Command", "command", "Connect"), 0, 0);
            gui->RunFrame();
            Require(engine.commands.size() == 2 && engine.password == "second",
                    "terminal password dialog replayed a queued connect action");
            session.connection.Disconnected();
        } else if (name == "other-game") {
            auto *game = dynamic_cast<vgui2::ComboBox *>(dialog->FindChildByName("GameFilter", true));
            Require(game && !game->IsEnabled(), "active mod must constrain original game selector");
            game->ActivateItem(1);
            gui->RunFrame();
            Settle(*dialog, session);
            Require(session.settings.filters.app == 10 && provider.lists.lastApp == 10,
                    "disabled game selector changed active host game");
        } else if (name == "localization" || name == "existing-owners" || name == "baseui-owner") {
            Require(std::wstring(vgui2::localize()->Find("CSEServerBrowser_CurrentGame")) == preserved,
                    "fallback overwrote an existing localized token");
            Require(vgui2::localize()->Find("CSEServerBrowser_Updating") != nullptr, "missing fallback not registered");
        } else if (name == "details-live" || name == "details-pending") {
            dialog->SelectVisible(0);
            Send(*dialog, "Details");
            Require(provider.players && provider.rules && provider.port == 27016,
                    "actual details UI did not use typed query-port interfaces");
            constexpr size_t DetailRows = 10000;
            for (size_t index = 0; index < DetailRows; ++index) {
                const auto playerName = "Player " + std::to_string(index);
                const auto ruleName = "Rule " + std::to_string(index);
                provider.players->AddPlayerToList(playerName.c_str(), static_cast<int>(index), 60.0f);
                provider.rules->RulesResponded(ruleName.c_str(), "value;never a command");
            }
            provider.players->PlayersRefreshComplete();
            provider.rules->RulesRefreshComplete();
            provider.players = nullptr;
            provider.rules = nullptr;
            constexpr size_t ExtraPresentationTurns = 8;
            const size_t populationTurns = name == "details-pending"
                                               ? DetailRows / DetailFrameBudget
                                               : 2 * DetailRows / DetailFrameBudget + ExtraPresentationTurns;
            for (size_t frame = 0; frame < populationTurns; ++frame) {
                dialog->Pump();
                gui->RunFrame();
                vgui2::surface()->SolveTraverse(dialog->GetVPanel(), false);
            }
            minimumDetailCloseTurns = 2 * DetailRows / DetailFrameBudget;
            if (name == "details-live") {
                auto *details = dialog->FindChildByName("DialogGameInfo", true);
                auto *detailsList = dynamic_cast<vgui2::ListPanel *>(details->FindChildByName("DetailsList", true));
                auto *scrollbar = dynamic_cast<vgui2::ScrollBar *>(details->FindChildByName("LogicalScroll", true));
                Require(detailsList && scrollbar, "details window presentation controls missing");
                int minimum = 0;
                int maximum = 0;
                scrollbar->GetRange(minimum, maximum);
                Require(maximum == static_cast<int>(2 * DetailRows), "details silently capped typed results");
                Require(AllocatedSlots(*detailsList) <= static_cast<size_t>(detailsList->GetTall()),
                        "details materialized full results in ListPanel");
                scrollbar->SetValue(maximum);
                gui->RunFrame();
                dialog->Pump();
                const auto lastItem =
                    detailsList->GetItem(detailsList->GetItemIDFromRow(detailsList->GetItemCount() - 1));
                Require(
                    lastItem && std::string(lastItem->GetString("name")) == "Rule 9999",
                    ("details scrollbar did not reach final result: value=" + std::to_string(scrollbar->GetValue()) +
                     " rows=" + std::to_string(detailsList->GetItemCount()) +
                     " last=" + (lastItem ? lastItem->GetString("name") : "<null>"))
                        .c_str());
                Require(engine.commands.empty(), "rule display data entered the engine command channel");
            }
        } else if (name == "details-retirement") {
            dialog->SelectVisible(0);
            Send(*dialog, "Details");
            Require(dialog->FindChildByName("DialogGameInfo", true) != nullptr, "details action did not create frame");
        } else if (!focusLayout && name != "tab-order") {
            throw std::runtime_error("unknown actual VGUI case");
        }
        session.closing = true;
        session.connection.Disconnected();
        bool childrenClosed = false;
        size_t childCloseTurns = 0;
        for (int attempt = 0; attempt < MaxFrames && !childrenClosed; ++attempt) {
            ++childCloseTurns;
            childrenClosed = dialog->BeginClose();
            gui->RunFrame();
        }
        Require(childrenClosed, "child query/panel retirement did not settle");
        Require(childCloseTurns >= minimumDetailCloseTurns,
                "detail queue and presentation reclaimed more than aggregate 32 rows per lifecycle turn");
        bool modelClosed = false;
        for (int attempt = 0; attempt < MaxFrames && !modelClosed; ++attempt) {
            modelClosed = session.model.CloseStep(BrowserFrameBudget);
        }
        Require(modelClosed, "production model close did not retire all retained nodes");
        while (!session.presentation.BeginClose()) {
            session.presentation.Pump(BrowserFrameBudget);
        }
        std::vector<vgui2::DHANDLE<vgui2::Panel>> layoutChildren;
        for (const char *name : {"GameTabs"}) {
            auto *owner = layout_fixture::Child(*dialog, name);
            vgui2::DHANDLE<vgui2::Panel> child;
            child = owner;
            layoutChildren.push_back(child);
            for (int index = 0; index < owner->GetChildCount(); ++index) {
                child = owner->GetChild(index);
                layoutChildren.push_back(child);
            }
        }
        dialog->MarkForDeletion();
        Require(handle.Get() != nullptr, "queued real panel deletion was mistaken for immediate retirement");
        for (int attempt = 0; attempt < MaxFrames && handle.Get(); ++attempt) {
            gui->RunFrame();
        }
        Require(handle.Get() == nullptr, "VGUI serial handle remained valid after deletion dispatch");
        for (auto &child : layoutChildren) {
            Require(child.Get() == nullptr, "browser layout child survived the real parent deletion fence");
        }
        Require(std::wstring(vgui2::localize()->Find("CSEServerBrowser_CurrentGame")) == preserved,
                "dialog retirement invalidated copied localized text");
        ownedSession.reset();
        if (hdParent) {
            geometry.Root(0);
            delete geometry_fixture::embeddedRoot;
        }
        gui->Shutdown();
        vgui2::system()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        filesystem->Unmount();
        privateSystem.Restore();
        dlclose(guiImage);
        dlclose(filesystemImage);
    });
}
