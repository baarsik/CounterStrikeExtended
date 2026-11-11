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
    auto *result = dynamic_cast<vgui2::ListPanel *>(dialog.FindChildByName("ServerList", true));
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
    if (argc != 6) {
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
        filesystem->Mount();
        filesystem->AddSearchPath(argv[3], "PLATFORM");
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface *>(guiFactory("VGUI_Surface026", nullptr)));
        const std::string layoutCase = argv[5];
        const bool focusLayout = layoutCase == "focus-resize" || layoutCase == "focus-scheme" ||
                                 layoutCase == "focus-tab" || layoutCase == "focus-header";
        const bool hdLayout = layoutCase == "hd-layout" || layoutCase == "hd-saved" || focusLayout ||
                              layoutCase == "tab-order";
        const bool inputRetirement = std::string(argv[5]) == "input-retirement";
        geometry_fixture::surface = hdLayout || inputRetirement ? &geometry : nullptr;
        geometry.forceWindowFocus = hdLayout;
        if (layoutCase == "focus-tab") {
            // Construct with the installed scheme at a real tall workspace before
            // shrinking; Label deliberately retains its already assigned font.
            geometry.Resize(3840, 2160);
        }
        CreateInterfaceFn factories[]{geometry_fixture::Factory, guiFactory, filesystemFactory};
        Require(vgui2::VGuiControls_Init("SERVER_BROWSER_TEST", factories, 3), "initialize actual VGUI controls");
        auto *gui = vgui2::ivgui();
        gui->SetSleep(false);
        gui->Start();
        auto *hdParent = hdLayout ? geometry_fixture::CreateParent(geometry) : nullptr;
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
        for (size_t index = 0;
             index < ((name == "bounded" || name == "navigation-sort" || name == "navigation-update" ||
                       name == "rapid-tabs" || name == "tab-selection-supersession" || name == "hd-layout")
                          ? ServerCount
                          : 3);
             ++index) {
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
            settings.width = 16384;
            settings.height = 16384;
            config.SetSettings(settings);
            Require(config.Save(), "fixture could not write owned extreme physical-size preference");
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
        if (focusLayout || name == "tab-order") {
            auto *body = layout_fixture::Child(*dialog, "BrowserOverflow");
            auto *table = layout_fixture::Child(*dialog, "TableOverflow");
            auto *tabs = static_cast<vgui2::PropertySheet *>(layout_fixture::Child(*dialog, "GameTabs"));
            auto *vertical = static_cast<vgui2::ScrollBar *>(body->FindChildByName("VerticalOverflow", false));
            auto *horizontal = static_cast<vgui2::ScrollBar *>(body->FindChildByName("HorizontalOverflow", false));
            auto settleLayout = [&] {
                constexpr int LayoutTurns = 8;
                for (int turn = 0; turn < LayoutTurns; ++turn) {
                    Settle(*dialog, session);
                    body->OnThink();
                    table->OnThink();
                    vgui2::surface()->SolveTraverse(dialog->GetVPanel(), false);
                }
            };
            auto resize = [&](int width, int height, bool schemeChange) {
                geometry.Resize(width, height);
                hdParent->SetSize(width, height);
                if (schemeChange) {
                    // Switch to another owned handle for the identical installed
                    // scheme; keep the old real font/border resources alive.
                    const auto path = std::filesystem::path(argv[4]) / "focus-scheme.res";
                    std::filesystem::copy_file(std::filesystem::path(argv[3]) / "resource/TrackerScheme.res", path,
                                               std::filesystem::copy_options::overwrite_existing);
                    filesystem->AddSearchPath(argv[4], "SKIN");
                    const auto scheme = vgui2::scheme()->LoadSchemeFromFile("focus-scheme.res", "FocusScheme");
                    Require(scheme != 0, "retained-focus real scheme failed to load");
                    dialog->SetScheme(scheme);
                }
                dialog->SetSize(width, height);
                dialog->InvalidateLayout(true, schemeChange);
                settleLayout();
            };
            resize(name == "focus-tab" ? 3840 : 1600, name == "focus-tab" ? 2160 : 1200, false);
            const auto logicalTabs = layout_fixture::LogicalTabs(*tabs);
            if (name == "tab-order") {
                constexpr std::array<std::array<int, 2>, 2> sizes{{{1600, 1200}, {640, 480}}};
                for (const auto &size : sizes) {
                    resize(size[0], size[1], false);
                    const auto positions = layout_fixture::Bounds(logicalTabs);
                    for (int page : {4, 1, 5, 2, 0, 3}) {
                        tabs->SetActivePage(tabs->GetPage(page));
                        dialog->InvalidateLayout(true, false);
                        settleLayout();
                        Require(layout_fixture::Bounds(logicalTabs) == positions,
                                "selecting a page changed the logical tabs' positions after layout");
                    }
                    layout_fixture::LogicalOrder(logicalTabs, size[0] == 640);
                    tabs->SetActivePage(tabs->GetPage(0));
                    tabs->RequestFocus(1);
                    settleLayout();
                    Require(vgui2::input()->GetFocus() == logicalTabs[0]->GetVPanel(),
                            "forward traversal did not enter the actual first tab");
                    for (int page = 1; page < tabs->GetNumPages(); ++page) {
                        layout_fixture::Key(*tabs->GetActiveTab(), vgui2::KEY_RIGHT);
                        settleLayout();
                        Require(tabs->GetActivePageNum() == page,
                                "right arrow did not follow the visible logical tab order");
                        Require(layout_fixture::Bounds(logicalTabs) == positions,
                                "keyboard selection changed wrapped tab positions");
                    }
                    for (int page = tabs->GetNumPages() - 2; page >= 0; --page) {
                        layout_fixture::Key(*tabs->GetActiveTab(), vgui2::KEY_LEFT);
                        settleLayout();
                        Require(tabs->GetActivePageNum() == page && layout_fixture::Bounds(logicalTabs) == positions,
                                "left arrow changed the visible logical tab order or positions");
                    }
                }
            } else {
                auto *focus = layout_fixture::Child(*dialog, "AddAddress");
                focus->RequestFocus(0);
                settleLayout();
                Require(vgui2::input()->GetFocus() == focus->GetVPanel(), "actual action focus was not delivered");
                Require(!vertical->IsEnabled() && !horizontal->IsEnabled(),
                        "retained-focus control must start without outer overflow");
                Require(vertical->GetValue() == 0 && horizontal->GetValue() == 0,
                        "nonoverflow control moved its inactive scrollbars");
                layout_fixture::VisibleWithin(*focus, *body);
                resize(640, 480, name == "focus-scheme");
                if (name == "focus-tab" || name == "focus-header") {
                    focus = name == "focus-tab" ? logicalTabs[1] : layout_fixture::Child(*List(*dialog), "password");
                    if (name == "focus-header") {
                        auto *tableHorizontal =
                            static_cast<vgui2::ScrollBar *>(table->FindChildByName("HorizontalOverflow", false));
                        Require(tableHorizontal->IsEnabled(), "nested header fixture needs horizontal overflow");
                        tableHorizontal->SetValue(0);
                    }
                    focus->RequestFocus(0);
                    settleLayout();
                }
                Require(vgui2::input()->GetFocus() == focus->GetVPanel(),
                        "resize/scheme transition lost the actual focused descendant");
                layout_fixture::VisibleWithin(*focus, *body);
                if (name == "focus-header") {
                    layout_fixture::VisibleWithin(*focus, *table);
                    focus = layout_fixture::Child(*List(*dialog), "address");
                    focus->RequestFocus(0);
                    settleLayout();
                    Require(vgui2::input()->GetFocus() == focus->GetVPanel(),
                            "actual last header focus was not delivered");
                    layout_fixture::VisibleWithin(*focus, *body);
                    layout_fixture::VisibleWithin(*focus, *table);
                }
                // An ordinary user scroll must remain effective with unchanged
                // focus and geometry, rather than being undone every frame.
                vertical->SetValue(0);
                settleLayout();
                Require(vertical->GetValue() == 0, "unchanged focus repeatedly overrides manual scrolling");
                resize(name == "focus-tab" ? 3840 : 1600, name == "focus-tab" ? 2160 : 1200, false);
                Require(!vertical->IsEnabled() && !horizontal->IsEnabled() &&
                            vertical->GetValue() == 0 && horizontal->GetValue() == 0,
                        "expansion left an inactive outer scrollbar moved or enabled");
                layout_fixture::VisibleWithin(*focus, *body);
            }
        }
        if (name == "hd-layout") {
            auto *label = dynamic_cast<vgui2::Label *>(layout_fixture::Child(*dialog, "NameLabel"));
            Require(label->IsHDProportional(), "fixture failed actual HD parent inheritance");
            Require(vgui2::surface()->GetFontTall(label->GetFont()) >= 24,
                    "fixture did not load real HD scheme fonts through surface factory");
            Require(session.presentation.VisibleCount() == ServerCount, "HD layout dropped legitimate results");
            layout_fixture::Check(*dialog, 1600, 1200);
            constexpr std::array<std::array<int, 2>, 5> sizes{
                {{640, 480}, {1280, 720}, {1600, 1200}, {3840, 2160}, {640, 480}}};
            int schemeGeneration = 0;
            for (const auto &size : sizes) {
                geometry.Resize(size[0], size[1]);
                hdParent->SetSize(size[0], size[1]);
                // Keep the previous scheme's borders alive while live controls switch.
                // Every variant is an exact byte copy of the installed scheme, not a smaller test font.
                const auto schemeName = "layout-scheme-" + std::to_string(schemeGeneration++) + ".res";
                const auto schemePath = std::filesystem::path(argv[4]) / schemeName;
                std::filesystem::copy_file(std::filesystem::path(argv[3]) / "resource/TrackerScheme.res", schemePath,
                                           std::filesystem::copy_options::overwrite_existing);
                filesystem->AddSearchPath(argv[4], "SKIN");
                const auto changedScheme = vgui2::scheme()->LoadSchemeFromFile(schemeName.c_str(), schemeName.c_str());
                Require(changedScheme != 0, "real alternate scheme failed to load");
                dialog->SetScheme(changedScheme);
                dialog->SetSize(size[0], size[1]);
                dialog->InvalidateLayout(true, true);
                for (int turn = 0; turn < 8; ++turn) {
                    Settle(*dialog, session);
                }
                std::fprintf(stderr, "layout %dx%d\n", size[0], size[1]);
                layout_fixture::Check(*dialog, size[0], size[1]);
            }
            auto *table = layout_fixture::Child(*dialog, "TableOverflow");
            auto *horizontal = dynamic_cast<vgui2::ScrollBar *>(layout_fixture::Child(*table, "HorizontalOverflow"));
            Require(horizontal->IsEnabled(), "small workspace hides required horizontal table overflow");
            layout_fixture::Key(*horizontal, vgui2::KEY_END);
            Require(horizontal->GetValue() > 0, "keyboard cannot reach right-hand table columns");
            auto *body = layout_fixture::Child(*dialog, "BrowserOverflow");
            auto *vertical = dynamic_cast<vgui2::ScrollBar *>(layout_fixture::Child(*body, "VerticalOverflow"));
            // Find the outer bar directly: the table owns another fixed pair.
            vertical = dynamic_cast<vgui2::ScrollBar *>(body->FindChildByName("VerticalOverflow", false));
            Require(vertical && vertical->IsEnabled(), "small workspace hides required control overflow");
            layout_fixture::Key(*vertical, vgui2::KEY_END);
            Require(vertical->GetValue() > 0, "keyboard cannot reach bottom actions/messages");
            auto *first = layout_fixture::Child(*dialog, "NameFilter");
            first->RequestFocus(0);
            for (int turn = 0; turn < 8; ++turn) {
                gui->RunFrame();
                body->OnThink();
            }
            Require(vgui2::input()->GetFocus() == first->GetVPanel(),
                    "actual input did not receive queued filter focus");
            int firstX = 0;
            int firstY = 0;
            first->LocalToScreen(firstX, firstY);
            body->ScreenToLocal(firstX, firstY);
            Require(firstY >= 0 && firstY + first->GetTall() <= body->GetTall(),
                    "focused filter remains outside scroll viewport");
            layout_fixture::Key(*first, vgui2::KEY_TAB);
            for (int turn = 0; turn < 8; ++turn) {
                gui->RunFrame();
                body->OnThink();
            }
            Require(vgui2::input()->GetFocus() == layout_fixture::Child(*dialog, "MapFilter")->GetVPanel(),
                    "actual Tab navigation cannot cross the new fixed-control hierarchy");
            wchar_t longLabel[] = L"Bardzo długa zlokalizowana etykieta przycisku i ustawień serwera";
            vgui2::localize()->AddString("CSEServerBrowser_LayoutFixture", longLabel, "fixture");
            for (const char *control : {"NameLabel", "RemoveFavorite", "Apply"}) {
                static_cast<vgui2::Label *>(layout_fixture::Child(*dialog, control))
                    ->SetText("#CSEServerBrowser_LayoutFixture");
            }
            auto *tabs = static_cast<vgui2::PropertySheet *>(layout_fixture::Child(*dialog, "GameTabs"));
            for (int index = 0; index < tabs->GetChildCount(); ++index) {
                if (auto *tab = dynamic_cast<vgui2::Label *>(tabs->GetChild(index))) {
                    if (std::string(tab->GetName()) == "tab") {
                        tab->SetText("#CSEServerBrowser_LayoutFixture");
                    }
                }
            }
            dialog->InvalidateLayout(true, true);
            Settle(*dialog, session);
            layout_fixture::Check(*dialog, 640, 480);
            dialog->SetSize(16384, 16384);
            dialog->InvalidateLayout(true, false);
            Settle(*dialog, session);
            layout_fixture::Check(*dialog, 640, 480);
            dialog->SaveSettings();
            const int savedWidth = dialog->GetWide();
            const int savedHeight = dialog->GetTall();
            session.config.Load();
            Require(session.config.Settings().width == savedWidth && session.config.Settings().height == savedHeight,
                    "saved browser dimensions changed physical-pixel units");
            dialog->Deactivate();
            dialog->Activate();
            for (int turn = 0; turn < MaxFrames && !dialog->IsVisible(); ++turn) {
                dialog->Pump();
                gui->RunFrame();
            }
            Require(dialog->IsVisible(), "layout reopen did not finish bounded inactive drain");
            StartList(*dialog, session, provider);
            Require(dialog->GetWide() == savedWidth && dialog->GetTall() == savedHeight,
                    "reopening repeatedly rescales saved physical dimensions");
            // Label retains an assigned font across SetScheme. Construct details
            // with a newly loaded real HD scheme first, then resize its live controls.
            geometry.Resize(1600, 1200);
            hdParent->SetSize(1600, 1200);
            const auto detailsSchemePath = std::filesystem::path(argv[4]) / "details-initial-scheme.res";
            std::filesystem::copy_file(std::filesystem::path(argv[3]) / "resource/TrackerScheme.res", detailsSchemePath,
                                       std::filesystem::copy_options::overwrite_existing);
            const auto detailsScheme =
                vgui2::scheme()->LoadSchemeFromFile("details-initial-scheme.res", "DetailsInitialHD");
            Require(detailsScheme != 0, "initial real details HD scheme failed to load");
            dialog->SetScheme(detailsScheme);
            dialog->SelectVisible(0);
            Send(*dialog, "Details");
            auto *details = layout_fixture::Child(*dialog, "DialogGameInfo");
            for (int turn = 0; turn < 8; ++turn) {
                dialog->Pump();
                gui->RunFrame();
                vgui2::surface()->SolveTraverse(details->GetVPanel(), false);
            }
            for (const char *control : {"ServerInfo", "Endpoint", "Status", "kind", "name", "value"}) {
                layout_fixture::Fits(*details, control);
            }
            static_cast<vgui2::Label *>(layout_fixture::Child(*details, "ServerInfo"))
                ->SetText("#CSEServerBrowser_LayoutFixture");
            for (const auto &size : sizes) {
                geometry.Resize(size[0], size[1]);
                hdParent->SetSize(size[0], size[1]);
                const auto schemeName = "details-scheme-" + std::to_string(schemeGeneration++) + ".res";
                const auto schemePath = std::filesystem::path(argv[4]) / schemeName;
                std::filesystem::copy_file(std::filesystem::path(argv[3]) / "resource/TrackerScheme.res", schemePath,
                                           std::filesystem::copy_options::overwrite_existing);
                const auto changedScheme = vgui2::scheme()->LoadSchemeFromFile(schemeName.c_str(), schemeName.c_str());
                Require(changedScheme != 0, "real details scheme failed to load");
                details->SetScheme(changedScheme);
                details->SetSize(size[0], size[1]);
                details->InvalidateLayout(true, true);
                for (int turn = 0; turn < 8; ++turn) {
                    dialog->Pump();
                    gui->RunFrame();
                    vgui2::surface()->SolveTraverse(details->GetVPanel(), false);
                    vgui2::surface()->PaintTraverse(details->GetVPanel());
                }
                Require(details->GetWide() <= size[0] && details->GetTall() <= size[1],
                        "details physical frame exceeds the actual workspace");
                for (const char *control : {"ServerInfo", "Endpoint", "Status", "kind", "name", "value"}) {
                    layout_fixture::Fits(*details, control);
                }
                if (size[1] >= 1200) {
                    auto *info = static_cast<vgui2::Label *>(layout_fixture::Child(*details, "ServerInfo"));
                    Require(vgui2::surface()->GetFontTall(info->GetFont()) >= 24,
                            "details did not inherit tall actual scheme fonts");
                }
            }
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
            Send(*dialog, "Refresh");
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
            auto *sortMessage = new KeyValues("SetSortColumn", "column", static_cast<int>(Column::Ping));
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
                tabs->SetActivePage(tabs->GetPage(static_cast<int>(kind)));
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
            auto *tabs = dynamic_cast<vgui2::PropertySheet *>(dialog->FindChildByName("GameTabs", true));
            auto *region = dynamic_cast<vgui2::ComboBox *>(dialog->FindChildByName("RegionFilter", true));
            auto *nameFilter = dynamic_cast<vgui2::TextEntry *>(dialog->FindChildByName("NameFilter", true));
            nameFilter->SetText("Server 1");
            region->ActivateItem(4);
            Send(*dialog, "Apply");
            dialog->ChooseSort(static_cast<int>(Column::Ping));
            Settle(*dialog, session);
            provider.lists.Complete();
            Settle(*dialog, session);
            Require(provider.lists.lastFilters["region"] == "3", "non-world Internet fixture was not admitted");
            dialog->SelectVisible(0);
            const auto internetSelection = session.model.Selection();
            for (int index = 1; index < tabs->GetNumPages(); ++index) {
                tabs->SetActivePage(tabs->GetPage(index));
                Settle(*dialog, session);
                Require(provider.lists.lastKind == static_cast<ListKind>(index),
                        "new tab continued the old Internet request after region refusal");
                Require(session.settings.kind == static_cast<ListKind>(index),
                        "tab did not publish admitted query identity");
                const bool supportsRegion = index == static_cast<int>(ListKind::Spectate);
                Require(region->IsEnabled() == supportsRegion,
                        "unsupported destination retained enabled region filter");
                Require(session.settings.filters.name.empty(), "destination inherited another tab's name filter");
                Require(session.settings.sort.column == Column::Name, "destination inherited another tab's sort");
                provider.lists.Complete();
                Settle(*dialog, session);
                Require(session.presentation.VisibleCount() == 3, "new tab retained previous filtered membership");
            }
            tabs->SetActivePage(tabs->GetPage(0));
            Settle(*dialog, session);
            Require(region->GetActiveItem() == 4 && provider.lists.lastFilters["region"] == "3",
                    "returning to Internet lost its supported per-tab region state");
            provider.lists.Complete();
            Settle(*dialog, session);
            Require(session.settings.filters.name == "Server 1" && session.settings.sort.column == Column::Ping &&
                        session.presentation.VisibleCount() == 1 && session.model.Selection() == internetSelection,
                    "returning to Internet lost supported filter/sort/membership/selection state");
            provider.lists.failRequest = true;
            tabs->SetActivePage(tabs->GetPage(static_cast<int>(ListKind::Favorites)));
            Settle(*dialog, session);
            const auto refused = session.model.GetStatus();
            Require(refused.kind == ListKind::Favorites && refused.phase == Phase::Failed && !refused.error.empty() &&
                        session.settings.kind == ListKind::Favorites && session.presentation.VisibleCount() == 0 &&
                        List(*dialog)->GetItemCount() == 0,
                    "provider refusal displayed old Internet results under a new query identity");
            provider.lists.failRequest = false;
            Send(*dialog, "Refresh");
            Settle(*dialog, session);
            provider.lists.Complete();
            Settle(*dialog, session);
            Require(session.presentation.VisibleCount() == 3, "refused destination could not recover honestly");
        } else if (name == "status-fields") {
            auto *query = dynamic_cast<vgui2::Label *>(dialog->FindChildByName("StatusLabel", true));
            auto *action = dynamic_cast<vgui2::Label *>(dialog->FindChildByName("ActionStatusLabel", true));
            Require(query && action, "query/progress and action result do not have independent visible fields");
            auto labelText = [](vgui2::Label &label) {
                std::array<char, 512> text{};
                label.GetText(text.data(), static_cast<int>(text.size()));
                return std::string(text.data());
            };
            provider.lists.requests.at(provider.lists.current)
                .callback->RefreshComplete(provider.lists.current, eServerFailedToRespond);
            favorites.failing = true;
            dialog->SelectVisible(0);
            Send(*dialog, "Favorite");
            dialog->Pump();
            const auto queryError = session.model.GetStatus().error;
            Require(query->IsVisible() && action->IsVisible() && labelText(*query) == queryError &&
                        labelText(*action) == session.model.GetStatus().actionError && !labelText(*action).empty(),
                    "an action result hid simultaneous query failure");
            favorites.failing = false;
            Send(*dialog, "Favorite");
            dialog->Pump();
            Require(labelText(*query) == queryError && labelText(*action).empty(),
                    "successful action recovery cleared query truth or left its obsolete action visible");
            favorites.failing = true;
            Send(*dialog, "RemoveFavorite");
            Send(*dialog, "Refresh");
            dialog->Pump();
            Require(!labelText(*query).empty() && !labelText(*action).empty(),
                    "an unresolved action hid query progress");
        } else if (name == "navigation-sort" || name == "navigation-update") {
            dialog->SelectVisible(1);
            const auto selected = session.model.Selection();
            Require(selected.has_value(), "navigation fixture selection absent");
            if (name == "navigation-sort") {
                dialog->ChooseSort(static_cast<int>(Column::Name));
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
                dialog->ChooseSort(static_cast<int>(Column::Ping));
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
                Require(provider.lists.lastKind == static_cast<ListKind>(index), "tab dispatched wrong typed request");
                provider.lists.Complete();
                Settle(*dialog, session);
            }
        } else if (name == "actions") {
            dialog->SelectVisible(0);
            Send(*dialog, "Favorite");
            Require(favorites.records.size() == 1, "favorite control failed to call actual model owner");
            Send(*dialog, "RemoveFavorite");
            Require((favorites.records[0].flags & k_unFavoriteFlagFavorite) == 0,
                    "remove control lost favorite semantics");
            dialog->ShowContextMenu();
            auto *menu = dynamic_cast<vgui2::Menu *>(dialog->FindChildByName("ServerContext", false));
            Require(menu && menu->GetItemCount() == 5, "actual retained row context actions missing");
            menu->ActivateItemByRow(2);
            gui->RunFrame();
            Require((favorites.records.back().flags & k_unFavoriteFlagFavorite) != 0,
                    "actual context-menu favorite action did not reach selected RowKey");
            Send(*dialog, "QuickSelected");
            Require(provider.lists.lastIndex >= 0, "selected quick refresh did not reach provider");
            const int previousRefreshes = provider.lists.refreshed;
            Send(*dialog, "Quick");
            Require(provider.lists.refreshed == previousRefreshes + 1, "full quick refresh missed live request");
            provider.lists.Complete();
            Settle(*dialog, session);
            Send(*dialog, "Refresh");
            Settle(*dialog, session);
            Send(*dialog, "Stop");
            Require(session.model.GetStatus().phase == Phase::Stopped, "Stop control left request refreshing");
            Send(*dialog, "Refresh");
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
            auto *text = dynamic_cast<vgui2::TextEntry *>(dialog->FindChildByName("NameFilter", true));
            Require(text != nullptr, "name filter control absent");
            text->SetText("Server 1");
            Send(*dialog, "Apply");
            Settle(*dialog, session);
            provider.lists.Complete();
            Settle(*dialog, session);
            Require(session.presentation.VisibleCount() == 1, "actual filter control did not filter rows");
            text->SetText("");
            auto *map = dynamic_cast<vgui2::TextEntry *>(dialog->FindChildByName("MapFilter", true));
            auto *ping = dynamic_cast<vgui2::TextEntry *>(dialog->FindChildByName("PingFilter", true));
            auto *passwordFilter = dynamic_cast<vgui2::ComboBox *>(dialog->FindChildByName("PasswordFilter", true));
            auto *secureFilter = dynamic_cast<vgui2::ComboBox *>(dialog->FindChildByName("SecureFilter", true));
            auto *region = dynamic_cast<vgui2::ComboBox *>(dialog->FindChildByName("RegionFilter", true));
            auto *nonempty = dynamic_cast<vgui2::CheckButton *>(dialog->FindChildByName("NonemptyFilter", true));
            auto *notFull = dynamic_cast<vgui2::CheckButton *>(dialog->FindChildByName("NotFullFilter", true));
            Require(map && ping && passwordFilter && secureFilter && region && nonempty && notFull,
                    "retained filter controls missing");
            map->SetText("de_dust2");
            ping->SetText("50");
            passwordFilter->ActivateItem(1);
            secureFilter->ActivateItem(1);
            region->ActivateItem(4);
            nonempty->SetSelected(true);
            notFull->SetSelected(true);
            for (auto &row : provider.lists.nextRows) {
                row.m_bPassword = true;
                row.m_bSecure = true;
            }
            provider.lists.nextRows[0].m_nPlayers = 0;
            provider.lists.nextRows[2].m_nPlayers = provider.lists.nextRows[2].m_nMaxPlayers;
            provider.lists.nextRows[2].m_nPing = 100;
            Send(*dialog, "Apply");
            Settle(*dialog, session);
            provider.lists.Complete();
            Settle(*dialog, session);
            Require(session.presentation.VisibleCount() == 1 && provider.lists.lastFilters["region"] == "3",
                    "map/ping/password/security/occupancy/location controls lost canonical filtering");
            passwordFilter->ActivateItem(2);
            secureFilter->ActivateItem(2);
            Send(*dialog, "Apply");
            Settle(*dialog, session);
            provider.lists.Complete();
            Settle(*dialog, session);
            Require(session.presentation.VisibleCount() == 0, "negative security/password filter was ignored");
        } else if (name == "unsaved") {
            Send(*dialog, "Apply");
            Settle(*dialog, session);
            Require(!session.config.Status().error.empty(), "fixture did not produce a settings save failure");
            auto *label = dynamic_cast<vgui2::Label *>(dialog->FindChildByName("ActionStatusLabel", true));
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
            Require(game != nullptr, "retained game selector absent");
            game->ActivateItem(1);
            for (auto &row : provider.lists.nextRows) {
                row.m_nAppID = 70;
                std::strcpy(row.m_szGameDir, "valve");
            }
            Send(*dialog, "Apply");
            Settle(*dialog, session);
            provider.lists.Complete();
            Settle(*dialog, session);
            Require(provider.lists.lastApp == 70 &&
                        session.presentation.VisibleCount() == provider.lists.nextRows.size(),
                    "other retained game could not be browsed");
            dialog->SelectVisible(0);
            dialog->Pump();
            Require(!dialog->FindChildByName("Connect", true)->IsEnabled(), "other-game row enabled connect");
            Send(*dialog, "Connect");
            Require(engine.commands.empty(), "other-game row issued a connection command");
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
        for (const char *name : {"BrowserOverflow", "TableOverflow"}) {
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
        dlclose(guiImage);
        dlclose(filesystemImage);
    });
}
