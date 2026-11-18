#include "model_fixture.inc"
#include "controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "../../../src/GameUI/OptionsDialog.h"
#include "../../../src/GameUI/OptionsSubInterface.h"
#include "../../../src/GameUI/HudRadarDialog.h"
#include "../../../src/cstrike/cl_dll/nextclient/radar_cvars.h"
#include "cvardef.h"
#include "CheckButton.h"
#include "Slider.h"
#include "vgui/IInput.h"
#include <map>
#include <cmath>
#include "../../../src/common/interface_language.h"
#include "PropertyPage.h"
#include "PropertySheet.h"
#include "SectionedListPanel.h"
#include "ComboBox.h"
#include "Menu.h"
#include "MenuItem.h"
#include "Button.h"
#include "RadioButton.h"
#include "Label.h"
#include "TextImage.h"
#include "TextEntry.h"
#include "Tooltip.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "filesystem.h"
#include "check.h"
#include "system_fixture.h"
#include "../../../src/engine/localization_state.h"
#include "../../../src/public/localization_controls.h"
#include "../../../src/public/vgui_background_controls.h"
#include "../../../src/public/vgui_smoked_glass.h"
#include "../../../src/vgui2/controls/SmokedGlass.h"
#include "geometry_fixture.h"
#include "control_state_fixture.h"
#include "options_reset_fixture.h"
#include "keyboard_cell_fixture.h"
#include "smoked_metrics_fixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cwchar>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <strings.h>
#define stricmp strcasecmp

extern "C" void CseSteamModuleInitialize() {}

// The production strict reader is separately checked against real native bytes.
// This provider keeps component reads in the fixture's private registry and can
// model a record whose type/length information the ISystem facade cannot retain.
bool rejectSavedLanguage = false;
namespace cse {
namespace interface_language {
std::string ReadSavedPreference() {
    if (rejectSavedLanguage || !system_fixture::current) {
        return {};
    }
    char saved[PreferenceBufferBytes]{};
    if (!system_fixture::current->GetRegistryString(PreferenceKey, saved, sizeof(saved))) {
        return {};
    }
    const std::string language = Normalize(saved);
    return IsBundled(language) ? language : std::string{};
}
}
}


// Only unrelated pages/game/taskbar are doubles. The complete OptionsDialog
// methods are copied unchanged with a source receipt; page and shared controls
// compile from production files with their owning CMake flags.
int appliedOtherPages = 0;
int expectedOtherPages = 0;
int clientCommands = 0;
int RecordClientCommand(char*) {
    ++clientCommands;
    return 1;
}

namespace engine_fixture {
using namespace cse::localization;
float uiStyle = 0;
int materialDraws = 0;
bool radarAvailable = false;
std::map<std::string, float> radarValues;
float GetCvar(char* name) {
    if (std::strcmp(name, cse::ui::VGuiStyleCvar) == 0) {
        return uiStyle;
    }
    const auto found = radarValues.find(name);
    return found != radarValues.end() ? found->second : 0.0f;
}
cvar_t* GetCvarPointer(const char* name) {
    static cvar_t available{};
    return radarAvailable && radarValues.count(name) != 0 ? &available : nullptr;
}
void SetCvar(char* name, float value) {
    if (std::strcmp(name, cse::ui::VGuiStyleCvar) == 0) {
        uiStyle = value;
        return;
    }
    Require(radarAvailable && radarValues.count(name) != 0, "only registered radar settings are changed");
    radarValues[name] = value;
}
int CSE_ABI_CALL CurrentStyle() { return uiStyle > 0.0f ? 1 : 0; }
bool CSE_ABI_CALL Material(const cse::ui::BackdropRect*, const cse::ui::BackdropRect*,
    const cse::ui::BackdropStyle*, float) {
    ++materialDraws;
    return false; // Exercise the real rounded no-GL fallback on the native SDL surface.
}
cse::ui::StyleApi styleApi{sizeof(styleApi), cse::ui::StyleApiVersion, CurrentStyle, Material};

class Backend final : public StateBackend {
public:
    bool failPrepare = false;
    bool failPersist = false;
    int prepares = 0;
    int persists = 0;
    int commits = 0;
    int discards = 0;

    std::string Resolve(const std::string& preference) override {
        return preference.empty() ? "english" : preference;
    }
    bool Prepare(const std::string&, std::string& error) override {
        ++prepares;
        if (failPrepare) {
            error = "Fixture rejected prepared catalogue";
            return false;
        }
        return true;
    }
    bool Persist(const std::string& preference, std::string& error) override {
        ++persists;
        if (failPersist) {
            error = "Fixture rejected preference write";
            return false;
        }
        return system_fixture::current->SetRegistryString(
            cse::interface_language::PreferenceKey, preference.c_str());
    }
    void Commit(const std::string&, std::uint64_t) override {
        ++commits;
    }
    void Discard() override {
        ++discards;
    }
};

State state;
Backend backend;
bool expose = true;
bool rejectRequest = false;
int requests = 0;
std::uint64_t lastRequest = InvalidRequest;

void Initialize(const char* preference) {
    backend = Backend{};
    state.Initialize(preference[0] ? preference : "english", preference);
    requests = 0;
    lastRequest = InvalidRequest;
    rejectRequest = false;
    expose = true;
    system_fixture::current->SetRegistryString(cse::interface_language::PreferenceKey, preference);
}
const char* CSE_ABI_CALL Language() {
    return state.Language().c_str();
}
const char* CSE_ABI_CALL Preference() {
    return state.Preference().c_str();
}
std::uint64_t CSE_ABI_CALL Revision() {
    return state.Revision();
}
std::uint64_t CSE_ABI_CALL RequestLanguage(const char* preference, bool persist) {
    ++requests;
    Require(persist, "Options requests engine-owned persistence");
    lastRequest = rejectRequest ? InvalidRequest : state.Request(preference, persist);
    return lastRequest;
}
bool CSE_ABI_CALL Result(std::uint64_t request, RequestResult* result) {
    return state.Result(request, result);
}
const char* CSE_ABI_CALL Error() {
    return state.LastError();
}
bool CSE_ABI_CALL Catalogue(const client_textmessage_s*) {
    return false;
}
bool CSE_ABI_CALL Center(const char*, const char*) {
    return false;
}
EngineApi api{sizeof(EngineApi), InterfaceVersion, Language, Preference, Revision, RequestLanguage,
    Result, Error, Catalogue, Center};
void* Factory(const char* name, int*) {
    if (std::strcmp(name, cse::ui::StyleInterface) == 0) {
        return &styleApi;
    }
    return expose && std::strcmp(name, EngineInterface) == 0 ? &api : nullptr;
}
void Advance() {
    state.Process(backend);
}
std::string Saved() {
    char preference[cse::interface_language::PreferenceBufferBytes]{};
    Require(system_fixture::current->GetRegistryString(cse::interface_language::PreferenceKey,
        preference, sizeof(preference)), "fixture has a stable saved preference");
    return preference;
}
}

cl_enginefuncs_s gEngfuncs{};
cl_enginefuncs_s* engine = &gEngfuncs;

class EmptyPage : public vgui2::PropertyPage {
public:
    explicit EmptyPage(vgui2::Panel* parent) : PropertyPage(parent, "OtherPage", true) {}
    void OnApplyChanges() override { ++appliedOtherPages; }
};
using COptionsSubMultiplayer = EmptyPage;
using COptionsSubAudio = EmptyPage;
using COptionsSubVideo = EmptyPage;
using COptionsSubMouse = EmptyPage;
using COptionsSubKeyboard = EmptyPage;
using COptionsSubVoice = EmptyPage;
using COptionsSubAdvanced = EmptyPage;
using CMultiplayerAdvancedPage = EmptyPage;
struct TestMod {
    int kind = 0;
    bool IsMultiplayerOnly() { return kind == 0; }
    bool IsSinglePlayerOnly() { return kind == 2; }
    const char* GetGameDescription() { return kind == 1 ? "Half-Life" : "Fixture"; }
} testMod;
TestMod* ModInfo() { return &testMod; }
struct TestTaskbar {
    void AddTask(vgui2::VPANEL) {}
    void SetTitle(vgui2::VPANEL, wchar_t*) {}
} taskbar;
TestTaskbar* g_pTaskbar = &taskbar;
#include "options_fixture.inc"

class TestOptions : public COptionsDialog {
public:
    explicit TestOptions(vgui2::Panel* parent) : COptionsDialog(parent) {}
    using COptionsDialog::OnCommand;
    using COptionsDialog::OnClose;
};

namespace {
int ScreenWidth = 640;
int ScreenHeight = 480;
constexpr int SettleFrames = 6;
void Settle(vgui2::Panel& root) {
    for (int index = 0; index < SettleFrames; ++index) {
        vgui2::ivgui()->RunFrame();
        vgui2::surface()->SolveTraverse(root.GetVPanel(), true);
    }
}
void Select(vgui2::ComboBox& combo, const char* id) {
    for (int index = 0; index < combo.GetItemCount(); ++index) {
        const int item = combo.GetMenu()->GetMenuID(index);
        KeyValues* data = combo.GetItemUserData(item);
        if (data && std::strcmp(data->GetString("language"), id) == 0) {
            combo.ActivateItem(item);
            return;
        }
    }
    throw std::runtime_error("language missing from combo");
}
void Capture(vgui2::Panel& root, const std::filesystem::path& path) {
    vgui2::surface()->SolveTraverse(root.GetVPanel(), true);
    root.Repaint();
    vgui2::surface()->PaintTraverse(root.GetVPanel());
    std::ofstream state(path.string() + ".state");
    const auto dump = [&](auto&& visit, vgui2::Panel& panel, int depth) -> void {
        int x = 0;
        int y = 0;
        panel.GetPos(x, y);
        const char* name = panel.GetName();
        state << std::string(depth, ' ') << (name ? name : "<null>") << " " << x << " " << y
            << " " << panel.GetWide() << " " << panel.GetTall() << " " << panel.IsVisible();
        if (auto* label = dynamic_cast<vgui2::Label*>(&panel)) state << " font=" << label->GetFont();
        state << "\n";
        for (int i = 0; i < panel.GetChildCount(); ++i) {
            if (auto* child = panel.GetChild(i)) visit(visit, *child, depth + 1);
        }
    };
    dump(dump, root, 0);
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Renderer* renderer = nullptr;
    for (int index = 0; index < count; ++index) {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(windows[index], &width, &height);
        if (width == root.GetWide() && height == root.GetTall()) {
            renderer = SDL_GetRenderer(windows[index]);
        }
    }
    SDL_free(windows);
    Require(renderer != nullptr, "actual native renderer");
    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    Require(pixels && SDL_SaveBMP(pixels, path.c_str()), "capture Options controls");
    SDL_DestroySurface(pixels);
}
void Fit(vgui2::Panel& panel, std::ofstream& report) {
    if (!panel.IsVisible()) {
        return;
    }
    if (auto* label = dynamic_cast<vgui2::Label*>(&panel)) {
        wchar_t text[256]{};
        label->GetText(text, 256);
        // Empty image-only frame decorations do not contain translated text.
        if (text[0]) {
            int neededWidth = 0;
            int neededHeight = 0;
            label->GetContentSize(neededWidth, neededHeight);
            report << panel.GetName() << '\t' << panel.GetWide() << '\t' << panel.GetTall() << '\t'
                << neededWidth << '\t' << neededHeight << '\n';
            Require(neededWidth <= panel.GetWide() && neededHeight <= panel.GetTall(), "Options label overflow");
        }
    }
    for (int index = 0; index < panel.GetChildCount(); ++index) {
        Fit(*panel.GetChild(index), report);
    }
}
}

class ParentOwnedPresentation : public vgui2::Frame {
public:
    vgui2::Button* playback;
    vgui2::TextEntry* hidden;
    vgui2::TextImage icon{"*"};
    int iconIndex = -1;

    explicit ParentOwnedPresentation(vgui2::Panel* parent) : Frame(parent, "ThemeOwnership", true, false) {
        SetBounds(0, 0, 360, 240);
        SetTitle("Shared style ownership", true);
        playback = new vgui2::Button(this, "Playback", "Play");
        playback->SetBounds(20, 40, 200, 24);
        hidden = new vgui2::TextEntry(this, "NeverPainted");
        hidden->SetBounds(20, 80, 200, 24);
        hidden->SetText("retained input");
        hidden->SetVisible(false);
    }

    void ApplySchemeSettings(vgui2::IScheme* scheme) override {
        Frame::ApplySchemeSettings(scheme);
        iconIndex = playback->AddImage(&icon, 0);
        playback->SetBgColor(Color(7, 19, 31, 0));
    }

    bool HasPlaybackIcon() {
        return iconIndex >= 0 && playback->GetImageAtIndex(iconIndex) == &icon;
    }
};

class GlyphProbeCombo : public vgui2::ComboBox {
public:
    using ComboBox::ComboBox;
    int glyphs = 0;

    int DrawChar(wchar_t character, vgui2::HFont font, int index, int x, int y) override {
        ++glyphs;
        return ComboBox::DrawChar(character, font, index, x, y);
    }
};

void RunRadarSettings(vgui2::Panel* root, vgui2::HScheme scheme, const std::filesystem::path& output) {
    using namespace engine_fixture;
    {
        vgui2::Slider singleValue(root, "SingleValueSlider");
        singleValue.SetSize(128, 24);
        singleValue.SetRange(42, 42);
        singleValue.SetValue(42, false);
        int knobStart = 0;
        int knobEnd = 0;
        singleValue.GetNobPos(knobStart, knobEnd);
        Require(singleValue.GetValue() == 42 && knobStart == 0 && knobEnd > knobStart && knobEnd <= 128,
            "a collapsed slider range keeps a valid knob while radar controls initialize");
    }
    {
        auto* first = new vgui2::Panel(root, "FirstTooltipOwner");
        auto* second = new vgui2::Panel(root, "SecondTooltipOwner");
        auto* firstTooltip = first->GetTooltip();
        firstTooltip->SetText("First tooltip");
        firstTooltip->SetTooltipDelay(0);
        firstTooltip->ResetDelay();
        firstTooltip->ShowTooltip(first);
        vgui2::PHandle popupOwner;
        popupOwner.Set(first->FindChildByName("tooltip", false));
        Require(popupOwner.Get(), "the first tooltip creates a shared popup");
        auto* secondTooltip = second->GetTooltip();
        secondTooltip->SetText("Second tooltip");
        secondTooltip->SetTooltipDelay(0);
        secondTooltip->ResetDelay();
        secondTooltip->ShowTooltip(second);
        firstTooltip->HideTooltip();
        delete first;
        auto* popup = popupOwner.Get();
        Require(popup && popup->IsVisible() && popup->GetParent() == second,
            "hiding and deleting an inactive tooltip owner preserves the active owner's popup");
        delete second;
        Require(!popupOwner.Get(), "the final tooltip owner releases the shared popup");
    }
    for (const auto& definition : radar::kCvarDefinitions) {
        radarValues[definition.name] = static_cast<float>(std::atof(definition.default_value));
    }
    radarAvailable = false;
    auto* halfLifePage = new COptionsSubInterface(root);
    auto* halfLifeButton = dynamic_cast<vgui2::Button*>(halfLifePage->FindChildByName("RadarSettings", false));
    Require(halfLifeButton && !halfLifeButton->IsEnabled(), "Half-Life cannot configure the CS-only radar");
    delete halfLifePage;
    radarAvailable = true;
    auto* csPage = new COptionsSubInterface(root);
    auto* radarButton = dynamic_cast<vgui2::Button*>(csPage->FindChildByName("RadarSettings", false));
    Require(radarButton && radarButton->IsEnabled(), "radar configuration belongs to the Interface page");
    delete csPage;
    std::ofstream fit(output / "radar-fit.tsv");
    for (int style : {0, 1}) {
        uiStyle = static_cast<float>(style);
        radarValues["hud_radar_scale"] = 1.7f;
        auto* dialog = new CHudRadarDialog(root);
        dialog->SetScheme(scheme);
        dialog->DoModal();
        Settle(*root);
        Require(vgui2::input()->GetAppModalSurface() == dialog->GetVPanel(), "radar child is modal");
        auto* slider = dynamic_cast<vgui2::Slider*>(dialog->FindChildByName("hud_radar_scale", false));
        auto* names = dynamic_cast<vgui2::CheckButton*>(dialog->FindChildByName("hud_radar_names", false));
        auto* apply = dynamic_cast<vgui2::Button*>(dialog->FindChildByName("Apply", false));
        Require(slider && names && apply && !apply->IsEnabled(), "opening radar settings has no pending edits");
        slider->SetValue(200);
        names->SetSelected(true);
        Settle(*root);
        Require(apply->IsEnabled() && radarValues["hud_radar_scale"] == 1.7f
            && radarValues["hud_radar_names"] == 0.0f, "radar edits are staged until Apply or OK");
        Fit(*dialog, fit);
        Capture(*dialog, output / (style ? "radar-smoked.bmp" : "radar-original.bmp"));
        vgui2::PHandle owner;
        owner.Set(dialog);
        dialog->OnCommand("Close");
        Settle(*root);
        Require(!owner.Get() && radarValues["hud_radar_scale"] == 1.7f,
            "Cancel deletes radar child and leaves unapplied settings unchanged");

        dialog = new CHudRadarDialog(root);
        dialog->SetScheme(scheme);
        dialog->DoModal();
        Settle(*root);
        apply = dynamic_cast<vgui2::Button*>(dialog->FindChildByName("Apply", false));
        dialog->OnCommand("Defaults");
        Settle(*root);
        Require(apply->IsEnabled() && radarValues["hud_radar_scale"] == 1.7f,
            "restoring radar defaults stages the shared defaults");
        dialog->OnCommand("Apply");
        Settle(*root);
        Require(std::abs(radarValues["hud_radar_scale"] - 1.15f) < 0.001f && !apply->IsEnabled(),
            "Apply commits radar defaults and establishes a new baseline");
        names = dynamic_cast<vgui2::CheckButton*>(dialog->FindChildByName("hud_radar_names", false));
        names->SetSelected(true);
        Settle(*root);
        dialog->OnCommand("Close");
        Settle(*root);
        Require(radarValues["hud_radar_names"] == 0.0f
            && std::abs(radarValues["hud_radar_scale"] - 1.15f) < 0.001f,
            "Cancel after Apply preserves committed settings and discards later edits");

        dialog = new CHudRadarDialog(root);
        dialog->SetScheme(scheme);
        dialog->DoModal();
        Settle(*root);
        names = dynamic_cast<vgui2::CheckButton*>(dialog->FindChildByName("hud_radar_names", false));
        names->SetSelected(true);
        owner.Set(dialog);
        dialog->OnCommand("OK");
        Settle(*root);
        Require(!owner.Get() && radarValues["hud_radar_names"] == 1.0f, "OK commits and closes radar settings");
        radarValues["hud_radar_names"] = 0.0f;
    }

    constexpr float CustomMapScale = 1.2345f;
    radarValues["hud_radar_map_scale"] = CustomMapScale;
    auto* preciseDialog = new CHudRadarDialog(root);
    preciseDialog->SetScheme(scheme);
    preciseDialog->DoModal();
    Settle(*root);
    auto* showNames = dynamic_cast<vgui2::CheckButton*>(preciseDialog->FindChildByName("hud_radar_names", false));
    Require(showNames != nullptr, "radar name control exists");
    showNames->SetSelected(true);
    preciseDialog->OnCommand("OK");
    Settle(*root);
    Require(radarValues["hud_radar_map_scale"] == CustomMapScale,
        "changing player names preserves the configured precision of an untouched map scale");

    for (const int style : {0, 1}) {
        uiStyle = static_cast<float>(style);
        for (const bool hoverTooltip : {false, true}) {
            constexpr int ReopenCount = 3;
            for (int reopen = 0; reopen < ReopenCount; ++reopen) {
                auto* options = new TestOptions(root);
                options->SetScheme(scheme);
                options->Activate();
                auto* page = dynamic_cast<COptionsSubInterface*>(options->FindChildByName("Interface", true));
                Require(page, "reopened Options owns its Interface page");
                options->GetPropertySheet()->SetActivePage(page);
                Settle(*root);
                auto* button = dynamic_cast<vgui2::Button*>(page->FindChildByName("RadarSettings", false));
                Require(button && button->IsEnabled(), "reopened Interface enables the radar settings button");
                button->DoClick();
                Settle(*root);
                auto* radar = dynamic_cast<CHudRadarDialog*>(page->FindChildByName("HudRadarDialog", false));
                Require(radar && radar->IsVisible(), "Interface opens a live radar dialog on every visit");
                vgui2::PHandle tooltipPopup;
                if (hoverTooltip) {
                    auto* shift = radar->FindChildByName("hud_radar_map_shift", false);
                    Require(shift, "radar owns the map-shift tooltip control");
                    auto* tooltip = shift->GetTooltip();
                    tooltip->SetTooltipDelay(0);
                    tooltip->ResetDelay();
                    tooltip->ShowTooltip(shift);
                    auto* popup = shift->FindChildByName("tooltip", false);
                    Require(popup && popup->IsVisible(), "hover displays the real shared tooltip popup");
                    tooltipPopup.Set(popup);
                    if (reopen % 2 == 0) {
                        tooltip->HideTooltip();
                    }
                }
                vgui2::PHandle radarOwner;
                radarOwner.Set(radar);
                radar->OnCommand("Close");
                Settle(*root);
                Require(!radarOwner.Get(), "closing radar settings deletes the dialog before reopening");
                if (auto* popup = tooltipPopup.Get()) {
                    Require(!popup->IsVisible() && !popup->GetParent(),
                        "closing a tooltip owner hides and detaches the shared popup");
                }
                vgui2::PHandle optionsOwner;
                optionsOwner.Set(options);
                options->OnClose();
                Settle(*root);
                Require(!optionsOwner.Get(), "closing all settings deletes the Options dialog");
                Require(vgui2::input()->GetAppModalSurface() == 0, "closed radar settings leave no modal owner");
            }
        }
    }
    radarAvailable = false;
    uiStyle = 0.0f;
}

int main(int argc, char** argv) {
    Require(argc == 7, "usage: interface_options_tests vgui fs resources output language platform");
    if (const char* width = std::getenv("CSE_UI_TEST_WIDTH")) {
        ScreenWidth = std::max(640, std::atoi(width));
    }
    if (const char* height = std::getenv("CSE_UI_TEST_HEIGHT")) {
        ScreenHeight = std::max(480, std::atoi(height));
    }
    return RunCase([&] {
        const std::filesystem::path output = argv[4];
        std::filesystem::create_directories(output);
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void* fsImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(image && fsImage, "load owning VGUI and filesystem");
        auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(image, "CreateInterface"));
        auto fsFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(fsImage, "CreateInterface"));
        auto* fs = static_cast<IFileSystem*>(fsFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(image,
            static_cast<vgui2::ISystem*>(guiFactory("VGUI_System009", nullptr)));
        fs->Mount();
        fs->AddSearchPathNoWrite(argv[6], "PLATFORM");
        for (const char* folder : {"platform", "valve", "cstrike"}) {
            fs->AddSearchPathNoWrite((std::string(argv[3]) + "/" + folder).c_str(), "GAME");
        }
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface*>(guiFactory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = &geometry;
        // Window activation is an external input; keep keyboard checks independent of other macOS windows.
        geometry.forceWindowFocus = true;
        geometry.Resize(ScreenWidth, ScreenHeight);
        CreateInterfaceFn factories[] = {engine_fixture::Factory, system_fixture::Factory,
            geometry_fixture::Factory, guiFactory, fsFactory};
        Require(vgui2::VGuiControls_Init("INTERFACE_OPTIONS", factories, 5), "initialize controls");
        vgui2::ivgui()->Start();
        vgui2::ivgui()->SetSleep(false);
        auto* root = new vgui2::Panel(nullptr, "OptionsRoot");
        root->SetBounds(0, 0, ScreenWidth, ScreenHeight);
        geometry.Root(root->GetVPanel());
        vgui2::surface()->SetLanguage(argv[5]);
        const auto tracker = vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", "Tracker");
        Require(tracker != 0, "load actual Tracker scheme at640x480");
        root->SetScheme(tracker);
        for (const char* file : {"resource/valve_%language%.txt", "resource/gameui_%language%.txt",
                 "resource/platform_%language%.txt", "resource/vgui_%language%.txt", "resource/cse_%language%.txt"}) {
            Require(vgui2::localize()->AddFile(fs, file), "load bundled translation");
        }
        auto manager = reinterpret_cast<void* (*)()>(dlsym(image, "_Z11FontManagerv"));
        auto fontForChar = reinterpret_cast<void* (*)(void*, vgui2::HFont, wchar_t)>(
            dlsym(image, "_ZN12CFontManager14GetFontForCharEjw"));
        Require(manager && fontForChar, "actual native font coverage probes");
        gEngfuncs.pfnClientCmd = RecordClientCommand;
        gEngfuncs.pfnGetCvarFloat = engine_fixture::GetCvar;
        gEngfuncs.Cvar_SetValue = engine_fixture::SetCvar;
        gEngfuncs.pfnGetCvarPointer = engine_fixture::GetCvarPointer;
        RunRadarSettings(root, tracker, output);
        keyboard_cell_fixture::Run(root, tracker, engine_fixture::uiStyle);
        smoked_metrics_fixture::Run(root, tracker, geometry, engine_fixture::uiStyle);
        options_reset_fixture::Run(root);
        control_state_fixture::Run(root, tracker, geometry, engine_fixture::uiStyle);
        testMod.kind = 0;
        engine_fixture::Initialize("");
        engine_fixture::uiStyle = 1.0f;
        auto* coldDialog = new TestOptions(root);
        coldDialog->SetScheme(tracker);
        coldDialog->Run();
        Require(coldDialog->GetPropertySheet()->GetActivePage() != nullptr,
            "activation initializes settings before showing or requesting keyboard focus");
        Settle(*root);
        auto* coldSheet = coldDialog->GetPropertySheet();
        Require(coldSheet->GetActivePage() == coldSheet->GetPage(0) && coldSheet->GetPage(0)->IsVisible(),
            "cold Smoked settings show their first page before any tab selection");
        Require(!coldDialog->IsApplyButtonEnabled(), "opening settings does not create pending changes");
        const int coldWidth = coldDialog->GetWide();
        const int coldHeight = coldDialog->GetTall();
        for (int idle = 0; idle < 12; ++idle) {
            Settle(*root);
        }
        Require(coldDialog->GetWide() == coldWidth && coldDialog->GetTall() == coldHeight,
            "cold settings keep stable geometry during idle without user changes");
        coldDialog->EnableApplyButton(true);
        Settle(*root);
        Require(coldDialog->GetWide() == coldWidth && coldDialog->GetTall() == coldHeight,
            "changing Apply state does not resize a settled settings window");
        coldDialog->EnableApplyButton(false);
        Settle(*root);
        for (int index = 0; index < coldSheet->GetNumPages(); ++index) {
            coldSheet->SetActivePage(coldSheet->GetPage(index));
            Settle(*root);
            Require(coldDialog->GetWide() == coldWidth && coldDialog->GetTall() == coldHeight,
                "switching any settings tab keeps the same window dimensions");
        }
        coldSheet->SetActivePage(coldSheet->GetPage(0));
        Settle(*root);
        auto* firstTab = coldSheet->GetActiveTab();
        Require(firstTab != nullptr, "cold settings have a visible active tab");
        coldSheet->RequestFocus(1);
        Settle(*root);
        Require(firstTab->HasFocus(), "active tab receives real keyboard focus");
        geometry.paintedFilledRectColors.clear();
        geometry.observeFilledRects = true;
        firstTab->Paint();
        geometry.observeFilledRects = false;
        Require(geometry.paintedFilledRectColors.empty(),
            "keyboard focus does not add a rounded highlight to the flat active tab");
        coldSheet->RequestFocusPrev(0);
        coldSheet->RequestFocusNext(0);
        vgui2::ivgui()->PostMessage(coldSheet->GetVPanel(),
            new KeyValues("KeyCodeTyped", "code", vgui2::KEY_LSHIFT), 0, 0);
        Settle(*root);
        Require(coldSheet->GetActivePage() == coldSheet->GetPage(0),
            "Shift and focus traversal retain the initialized settings page");
        delete coldDialog;
        engine_fixture::uiStyle = 0.0f;
        std::ofstream report(output / "fit.tsv");
        for (int kind = 0; kind < 3; ++kind) {
            testMod.kind = kind;
            engine_fixture::Initialize(kind == 2 ? "polish" : "");
            // The bound engine is authoritative even if a legacy/native reader disagrees.
            rejectSavedLanguage = true;
            Require(cse::localization::ControlsEngine() == &engine_fixture::api, "live service resolved by factory");
            system_fixture::current->languageOverride.clear();
            auto* dialog = new TestOptions(root);
            dialog->SetScheme(tracker);
            dialog->SetPos(0, 0);
            dialog->SetVisible(true);
            auto* sheet = dialog->GetPropertySheet();
            int interfaceIndex = -1;
            for (int index = 0; index < sheet->GetNumPages(); ++index) {
                if (dynamic_cast<COptionsSubInterface*>(sheet->GetPage(index))) {
                    interfaceIndex = index;
                }
            }
            Require(interfaceIndex == (kind == 0 ? 3 : 2), "Interface must immediately follow Video");
            auto* page = dynamic_cast<COptionsSubInterface*>(sheet->GetPage(interfaceIndex));
            sheet->SetActivePage(page);
            Settle(*root);
            Require(dialog->GetWide() <= ScreenWidth && dialog->GetTall() <= ScreenHeight, "Options within screen");
            int tabs = 0;
            for (int index = 0; index < sheet->GetChildCount(); ++index) {
                auto* child = sheet->GetChild(index);
                if (std::strcmp(child->GetName(), "tab") == 0) {
                    ++tabs;
                    int x = 0;
                    int y = 0;
                    child->GetPos(x, y);
                    Require(x + child->GetWide() <= sheet->GetWide(), "all Options tabs visible");
                }
            }
            Require(tabs == sheet->GetNumPages(), "every tab inspected");
            Fit(*dialog, report);
            for (int childIndex = 0; childIndex < page->GetChildCount(); ++childIndex) {
                auto* child = page->GetChild(childIndex);
                if (child->IsVisible()) {
                    int x = 0;
                    int y = 0;
                    child->GetPos(x, y);
                    Require(x >= 0 && y >= 0 && x + child->GetWide() <= page->GetWide()
                        && y + child->GetTall() <= page->GetTall(), "Interface controls within page");
                }
            }
            auto* combo = dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("Language", false));
            Require(combo && !combo->IsEditable() && combo->GetItemCount() == 11, "automatic plus all ten languages");
            Require(std::strcmp(combo->GetActiveItemUserData()->GetString("language"),
                kind == 2 ? "polish" : "") == 0, "saved or invalid preference reflected in selection");
            expectedOtherPages = sheet->GetNumPages() - 1;
            auto rebuild = [&](const char* preference) {
                if (dialog) {
                    delete dialog;
                }
                engine_fixture::Initialize(preference);
                dialog = new TestOptions(root);
                dialog->SetScheme(tracker);
                dialog->SetVisible(true);
                sheet = dialog->GetPropertySheet();
                page = dynamic_cast<COptionsSubInterface*>(sheet->GetPage(interfaceIndex));
                sheet->SetActivePage(page);
                Settle(*root);
                combo = dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("Language", false));
                expectedOtherPages = sheet->GetNumPages() - 1;
                appliedOtherPages = 0;
            };
            const int unchangedWrites = system_fixture::current->stringWrites;
            appliedOtherPages = 0;
            dialog->OnCommand("Apply");
            Settle(*root);
            Require(engine_fixture::requests == 0 && system_fixture::current->stringWrites == unchangedWrites,
                "unchanged Apply neither requests nor writes a language");
            Require(appliedOtherPages == expectedOtherPages, "unchanged Apply applies each other page once");
            for (const auto& language : cse::interface_language::BundledLanguages) {
                Select(*combo, language.id);
                Settle(*root);
                int width = 0;
                int height = 0;
                wchar_t name[256]{};
                combo->GetText(name, 256);
                Require(std::wcscmp(name, language.nativeName) == 0, "complete selected native language name");
                wchar_t menuName[256]{};
                combo->GetMenu()->GetMenuItem(combo->GetActiveItem())->GetText(menuName, 256);
                Require(std::wcscmp(menuName, language.nativeName) == 0, "complete native language dropdown item");
                KeyValues* updatedData = combo->GetActiveItemUserData()->MakeCopy();
                Require(combo->UpdateItem(combo->GetActiveItem(), language.nativeName, updatedData),
                    "wide dropdown item update");
                updatedData->deleteThis();
                combo->ActivateItem(combo->GetActiveItem());
                combo->GetText(name, 256);
                Require(std::wcscmp(name, language.nativeName) == 0, "complete wide item update selection");
                const auto font = vgui2::scheme()->GetIScheme(combo->GetScheme())->GetFontEx(
                    "Default", combo->IsProportional(), combo->IsHDProportional());
                for (const wchar_t* character = name; *character; ++character) {
                    if (*character > L' ') {
                        Require(fontForChar(manager(), font, *character) != nullptr, "native language name glyph coverage");
                    }
                }
                vgui2::surface()->GetTextSize(font, name, width, height);
                Require(width <= combo->GetWide() - 24 && height <= combo->GetTall(), "native language name fits");
            }
            const int fitRequests = engine_fixture::requests;
            combo->OnKeyTyped(L'p');
            Settle(*root);
            Require(std::strcmp(combo->GetActiveItemUserData()->GetString("language"), "polish") == 0,
                "keyboard language selection");
            Require(engine_fixture::requests == fitRequests, "changing selection alone never requests language");
            dialog->ResetAllData();
            Settle(*root);

            rebuild("");
            Select(*combo, "russian");
            Settle(*root);
            Require(dialog->IsApplyButtonEnabled(), "selection enables Apply");
            const int pendingWrites = system_fixture::current->stringWrites;
            vgui2::PHandle pendingOwner;
            pendingOwner.Set(dialog);
            dynamic_cast<vgui2::Button*>(dialog->FindChildByName("ApplyButton", false))->DoClick();
            Settle(*root);
            Require(page->IsLanguagePending() && !dialog->IsEnabled(), "Apply owns a pending disabled dialog");
            Require(engine_fixture::requests == 1 && engine_fixture::Saved().empty()
                && system_fixture::current->stringWrites == pendingWrites, "UI does not persist before engine acceptance");
            Require(appliedOtherPages == expectedOtherPages, "accepted Apply applies other pages exactly once");
            dialog->OnCommand("Apply");
            dialog->OnCommand("OK");
            dialog->OnCommand("Cancel");
            dialog->OnClose();
            Settle(*root);
            Require(pendingOwner.Get() == dialog && page->IsLanguagePending(), "pending owner survives Apply OK Cancel close");
            Require(engine_fixture::requests == 1 && appliedOtherPages == expectedOtherPages,
                "duplicate pending commands neither request nor reapply pages");
            engine_fixture::Advance();
            Require(engine_fixture::Saved() == "russian" && engine_fixture::backend.commits == 1,
                "engine persists stable id and commits the language");
            Settle(*root);
            Require(dialog->IsVisible() && dialog->IsEnabled() && !page->IsLanguagePending()
                && !page->HasLanguageChange(), "completed Apply stays open with accepted original state");
            Require(appliedOtherPages == expectedOtherPages, "completion does not apply pages twice");
            Select(*combo, "polish");
            Settle(*root);
            dialog->ResetAllData();
            Settle(*root);
            Require(std::strcmp(combo->GetActiveItemUserData()->GetString("language"), "russian") == 0,
                "reset reads the engine accepted preference");

            // Failure through OK must never discard this owner. Each attempt may apply
            // unrelated pages once, but polling/repeated input may not apply them again.
            for (const char* failure : {"request", "prepare", "persist", "missing", "superseded"}) {
                rebuild("");
                Select(*combo, "russian");
                Settle(*root);
                engine_fixture::rejectRequest = std::strcmp(failure, "request") == 0;
                engine_fixture::backend.failPrepare = std::strcmp(failure, "prepare") == 0;
                engine_fixture::backend.failPersist = std::strcmp(failure, "persist") == 0;
                const int beforeFailureWrites = system_fixture::current->stringWrites;
                vgui2::PHandle owner;
                owner.Set(dialog);
                dialog->OnCommand("OK");
                Require(engine_fixture::requests == 1, "failure scenario made exactly one engine request");
                if (std::strcmp(failure, "superseded") == 0) {
                    engine_fixture::state.Request("german", false);
                } else if (std::strcmp(failure, "missing") == 0) {
                    // Exercise the real bounded State result history rather than
                    // fabricating a missing response in the factory callback.
                    constexpr int HistoryProbeLimit = 256;
                    cse::localization::RequestResult result{sizeof(result),
                        cse::localization::RequestState::Pending, 0};
                    int displaced = 0;
                    while (engine_fixture::state.Result(engine_fixture::lastRequest, &result)
                        && displaced < HistoryProbeLimit) {
                        engine_fixture::state.Request("german", false);
                        ++displaced;
                    }
                    Require(!engine_fixture::state.Result(engine_fixture::lastRequest, &result),
                        "production engine history expires the UI request status");
                } else {
                    engine_fixture::Advance();
                }
                Settle(*root);
                Require(owner.Get() == dialog && dialog->IsVisible() && dialog->IsEnabled()
                    && dialog->IsApplyButtonEnabled() && !page->IsLanguagePending(), "failed OK retains retryable owner");
                Require(page->FindChildByName("SaveFailed", false)->IsVisible(), "failed transaction displays error");
                Require(engine_fixture::state.Language() == "english" && engine_fixture::state.Preference().empty()
                    && engine_fixture::state.Revision() == 1 && engine_fixture::Saved().empty(),
                    "failure leaves active language preference revision and saved id unchanged");
                Require(system_fixture::current->stringWrites == beforeFailureWrites, "failed attempt does not write registry");
                Require(appliedOtherPages == (engine_fixture::rejectRequest ? 0 : expectedOtherPages),
                    "other pages apply once only after request acceptance");
                Require(std::strcmp(combo->GetActiveItemUserData()->GetString("language"), "russian") == 0,
                    "failure preserves selected retry language");
                Fit(*dialog, report);
                engine_fixture::rejectRequest = false;
                engine_fixture::backend.failPrepare = false;
                engine_fixture::backend.failPersist = false;
                            appliedOtherPages = 0;
                dialog->OnCommand("OK");
                Require(owner.Get() == dialog && page->IsLanguagePending(), "retry OK retains owner until completion");
                Settle(*root);
                Require(appliedOtherPages == expectedOtherPages && engine_fixture::requests == 2,
                    "retry requests once and applies other pages once");
                engine_fixture::Advance();
                Settle(*root);
                Require(owner.Get() == nullptr && engine_fixture::Saved() == "russian", "successful OK retry closes after commit");
                dialog = nullptr;
            }

            rebuild("polish");
            vgui2::PHandle unchangedOwner;
            unchangedOwner.Set(dialog);
            dialog->OnCommand("OK");
            Settle(*root);
            Require(unchangedOwner.Get() == nullptr && engine_fixture::requests == 0
                && appliedOtherPages == expectedOtherPages, "unchanged OK closes normally without requesting language");
            dialog = nullptr;

            rebuild("");
            Select(*combo, "english");
            Settle(*root);
            dialog->OnCommand("Apply");
            engine_fixture::Advance();
            Settle(*root);
            Require(engine_fixture::Saved() == "english" && engine_fixture::backend.prepares == 0
                && engine_fixture::backend.commits == 0 && engine_fixture::state.Revision() == 1,
                "same effective language still persists explicit preference without unnecessary reload");
            Select(*combo, "");
            Settle(*root);
            appliedOtherPages = 0;
            dialog->OnCommand("Apply");
            Require(page->IsLanguagePending(), "automatic selection uses the engine transaction");
            engine_fixture::Advance();
            Settle(*root);
            Require(engine_fixture::Saved().empty() && engine_fixture::state.Preference().empty()
                && !page->HasLanguageChange(), "automatic clears preference through engine persistence");
            Require(appliedOtherPages == expectedOtherPages, "automatic applies other pages exactly once");

            // Launch-override notice, all locale geometry, and captures remain the
            // same real controls/layout checks used by the original fixture.
            system_fixture::current->languageOverride = "german";
            dialog->ResetAllData();
            Settle(*root);
            Require(page->FindChildByName("LaunchOverride", false)->IsVisible(), "launch override notice");
            Fit(*dialog, report);
            if (kind == 0) {
                Capture(*dialog, output / "options.bmp");
                combo->OnKeyCodeTyped(vgui2::KEY_SPACE);
                Settle(*root);
                Fit(*combo->GetMenu(), report);
                int menuX = 0;
                int menuY = 0;
                combo->GetMenu()->GetPos(menuX, menuY);
                Require(menuY + combo->GetMenu()->GetTall() <= ScreenHeight, "dropdown within screen");
                Capture(*combo->GetMenu(), output / "languages.bmp");
                combo->HideMenu();
                auto* styles = dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("UIStyle", false));
                Require(styles && styles->GetItemCount() == 2, "Original and Smoked Tactical Glass stay available");
                const Color originalBackground = combo->GetBgColor();
                auto* languageLabel = dynamic_cast<vgui2::Label*>(page->FindChildByName("LanguageLabel", false));
                const vgui2::HFont originalLabelFont = languageLabel->GetFont();
                int originalComboX = 0;
                int originalComboY = 0;
                combo->GetPos(originalComboX, originalComboY);
                const std::string selectedLanguage = combo->GetActiveItemUserData()->GetString("language");
                styles->ActivateItem(styles->GetMenu()->GetMenuID(1));
                static_cast<vgui2::PropertyPage*>(page)->OnApplyChanges();
                Settle(*root);
                Capture(*dialog, output / "smoked-options.bmp");
                Require(engine_fixture::uiStyle == 1.0f && engine_fixture::materialDraws > 0,
                    "selected style reaches shared window background painting");
                Require(page->GetBgColor().a() == 0 && sheet->GetBgColor().a() == 0,
                    "structural settings containers expose the shared glass instead of covering it");
                std::fprintf(stderr, "Font evidence: Original %lu/%d; Smoked %lu/%d\n",
                    static_cast<unsigned long>(originalLabelFont), vgui2::surface()->GetFontTall(originalLabelFont),
                    static_cast<unsigned long>(languageLabel->GetFont()), vgui2::surface()->GetFontTall(languageLabel->GetFont()));
                Require(languageLabel->GetFont() != originalLabelFont &&
                    vgui2::surface()->GetFontTall(languageLabel->GetFont()) <= languageLabel->GetTall(),
                    "Smoked text uses scalable typography that fits its allocated row");
                int labelX = 0;
                int labelY = 0;
                int fieldX = 0;
                int fieldY = 0;
                languageLabel->GetPos(labelX, labelY);
                combo->GetPos(fieldX, fieldY);
                Require(fieldX >= labelX + languageLabel->GetWide() && fieldY == labelY,
                    "Smoked settings use aligned label and field columns");
                Fit(*dialog, report);
                sheet->SetActivePage(sheet->GetPage(0));
                Settle(*root);
                const int stableWidth = dialog->GetWide();
                const int stableHeight = dialog->GetTall();
                sheet->SetActivePage(page);
                Settle(*root);
                Require(dialog->GetWide() == stableWidth && dialog->GetTall() == stableHeight,
                    "Interface shares the same owner dimensions as all other settings pages");
                auto* originalRadio = dynamic_cast<vgui2::RadioButton*>(page->FindChildByName("OriginalStyle", false));
                auto* smokedRadio = dynamic_cast<vgui2::RadioButton*>(page->FindChildByName("SmokedStyle", false));
                Require(originalRadio && smokedRadio && originalRadio->GetRadioTabPosition() > 0 &&
                    originalRadio->GetRadioTabPosition() == smokedRadio->GetRadioTabPosition(),
                    "style radios retain their actual keyboard group");
                for (vgui2::RadioButton* radio : {originalRadio, smokedRadio}) {
                    wchar_t text[256]{};
                    radio->GetText(text, 256);
                    int textWide = 0;
                    int textTall = 0;
                    vgui2::surface()->GetTextSize(radio->GetFont(), text, textWide, textTall);
                    Require(text[0] && textWide > 0 && textWide +
                        vgui2::smoked::Pixels(cse::ui::RadioDiameter + cse::ui::RadioTextGap) <= radio->GetWide(),
                        "both style choices have complete visible captions on first opening");
                }
                smokedRadio->OnKeyCodeTyped(vgui2::KEY_LEFT);
                Settle(*root);
                Require(styles->GetActiveItemUserData()->GetInt("style") == cse::ui::OriginalVGuiStyle &&
                    dialog->IsApplyButtonEnabled(), "arrow-key radio choice updates the applied style owner");
                originalRadio->OnKeyCodeTyped(vgui2::KEY_RIGHT);
                Settle(*root);
                Require(styles->GetActiveItemUserData()->GetInt("style") == cse::ui::SmokedTacticalGlassVGuiStyle,
                    "keyboard radio navigation selects Smoked again");
                const Color styledText = combo->GetFgColor();
                Require(styledText.r() == cse::ui::Text.r && styledText.g() == cse::ui::Text.g &&
                    styledText.b() == cse::ui::Text.b, "shared input uses the smoked foreground palette");
                wchar_t styleName[128]{};
                styles->GetText(styleName, 128);
                Require(std::wcscmp(styleName, L"Smoked Tactical Glass") == 0, "new style name is localized");
                styles->ActivateItem(styles->GetMenu()->GetMenuID(0));
                static_cast<vgui2::PropertyPage*>(page)->OnApplyChanges();
                Settle(*root);
                const int drawsBeforeOriginal = engine_fixture::materialDraws;
                Capture(*dialog, output / "original-options-restored.bmp");
                Require(combo->GetBgColor() == originalBackground &&
                    engine_fixture::materialDraws == drawsBeforeOriginal,
                    "Original restores scheme colors and stops all material requests immediately");
                int restoredX = 0;
                int restoredY = 0;
                combo->GetPos(restoredX, restoredY);
                Require(languageLabel->GetFont() == originalLabelFont && restoredX == originalComboX &&
                    restoredY == originalComboY, "Original restores exact fonts and form geometry");
                Require(selectedLanguage == combo->GetActiveItemUserData()->GetString("language") &&
                    sheet->GetActivePage() == page, "style switching keeps open page and selection");
            }
            Select(*combo, "polish");
            Settle(*root);
            const int beforeCancelWrites = system_fixture::current->stringWrites;
            const int beforeCancelRequests = engine_fixture::requests;
            vgui2::PHandle cancelled;
            cancelled.Set(dialog);
            dynamic_cast<vgui2::Button*>(dialog->FindChildByName("CancelButton", false))->DoClick();
            Settle(*root);
            Require(cancelled.Get() == nullptr && system_fixture::current->stringWrites == beforeCancelWrites
                && engine_fixture::requests == beforeCancelRequests, "Cancel discards selection without request or persistence");
            dialog = nullptr;

            // Without an engine provider only the strict native reader is allowed;
            // a changed selection cannot fall back to page-owned setting writes.
            engine_fixture::expose = false;
            cse::localization::BindControls(factories, 5);
            system_fixture::current->SetRegistryString(cse::interface_language::PreferenceKey, "russian");
            rejectSavedLanguage = true;
            dialog = new TestOptions(root);
            dialog->SetScheme(tracker);
            dialog->SetVisible(true);
            sheet = dialog->GetPropertySheet();
            page = dynamic_cast<COptionsSubInterface*>(sheet->GetPage(interfaceIndex));
            sheet->SetActivePage(page);
            Settle(*root);
            combo = dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("Language", false));
            Require(combo->GetActiveItemUserData()->GetString("language")[0] == '\0', "fallback rejects malformed native record");
            Select(*combo, "polish");
            Settle(*root);
            const int noServiceWrites = system_fixture::current->stringWrites;
            const int noServiceRequests = engine_fixture::requests;
            dialog->OnCommand("Apply");
            Settle(*root);
            Require(dialog->IsVisible() && page->FindChildByName("SaveFailed", false)->IsVisible()
                && system_fixture::current->stringWrites == noServiceWrites && engine_fixture::requests == noServiceRequests,
                "missing service keeps dialog open without native fallback write");
            delete dialog;
            dialog = nullptr;
            engine_fixture::expose = true;
            cse::localization::BindControls(factories, 5);
            Require(clientCommands == 0, "no Apply OK failure Cancel or completion issues any client/restart command");
            Settle(*root);
        }
        // Shared Smoked chrome is independent of old resource metrics; Original restores those metrics exactly.
        class FixedResourceWindow : public vgui2::Frame {
        public:
            using Frame::Frame;
            using Frame::GetClientArea;
        };
        auto* fixedWindow = new FixedResourceWindow(root, "FixedResourceWindow", true, false);
        fixedWindow->SetScheme(tracker);
        fixedWindow->SetBounds(0, 0, 640, 390);
        fixedWindow->MakeReadyForUse();
        int originalClientX = 0;
        int originalClientY = 0;
        int originalClientWide = 0;
        int originalClientTall = 0;
        fixedWindow->GetClientArea(originalClientX, originalClientY, originalClientWide, originalClientTall);
        engine_fixture::uiStyle = 1.0f;
        fixedWindow->InvalidateLayout(true, true);
        int clientX = 0;
        int clientY = 0;
        int clientWide = 0;
        int clientTall = 0;
        fixedWindow->GetClientArea(clientX, clientY, clientWide, clientTall);
        Require(clientX == vgui2::smoked::Pixels(16) && clientY == vgui2::smoked::Pixels(48) &&
            clientWide == fixedWindow->GetWide() - clientX * 2 &&
            clientTall == fixedWindow->GetTall() - clientY - clientX,
            "fixed windows use the shared Smoked header and inset");
        engine_fixture::uiStyle = 0.0f;
        fixedWindow->InvalidateLayout(true, true);
        fixedWindow->GetClientArea(clientX, clientY, clientWide, clientTall);
        Require(clientX == originalClientX && clientY == originalClientY &&
            clientWide == originalClientWide && clientTall == originalClientTall,
            "Original restores the complete fixed-window client geometry");
        engine_fixture::uiStyle = 1.0f;
        fixedWindow->InvalidateLayout(true, true);
        auto* layoutMenu = new vgui2::Menu(root, "LayoutMenu");
        auto* cascade = new vgui2::Menu(root, "LayoutCascade");
        auto* parentRow = new vgui2::MenuItem(layoutMenu, "ParentRow", "Parent", cascade, false);
        layoutMenu->AddMenuItem(parentRow);
        auto* separator = new vgui2::MenuItem(layoutMenu, "BlankSeparator", "", nullptr, false);
        layoutMenu->AddMenuItem(separator);
        auto* childRow = new vgui2::MenuItem(cascade, "ChildRow", "Child", nullptr, false);
        cascade->AddMenuItem(childRow);
        layoutMenu->SetPos(40, 40);
        layoutMenu->SetVisible(true);
        layoutMenu->MakeReadyForUse();
        layoutMenu->InvalidateLayout(true, true);
        layoutMenu->PerformLayout();
        parentRow->OpenCascadeMenu();
        Require(separator->GetTall() <= parentRow->GetTall() / 2 && !separator->IsEnabled(),
            "empty menu rows become compact noninteractive separators");
        separator->ArmItem();
        Require(!separator->IsItemArmed(), "empty separators never acquire the selection marker");
        int parentX = 0;
        int parentY = 0;
        int childX = 0;
        int childY = 0;
        vgui2::ipanel()->GetAbsPos(parentRow->GetVPanel(), parentX, parentY);
        vgui2::ipanel()->GetAbsPos(childRow->GetVPanel(), childX, childY);
        std::fprintf(stderr, "Cascade: parent %d,%d width %d; child %d,%d\n",
            parentX, parentY, parentRow->GetWide(), childX, childY);
        Require(parentY == childY && childX >= parentX + parentRow->GetWide() + vgui2::smoked::Pixels(18),
            "cascades align their first row with the owning row and keep a consistent gap");
        delete layoutMenu;
        auto* displayCombo = new GlyphProbeCombo(fixedWindow, "DisplayCombo", 4, false);
        displayCombo->SetText("Spectator camera");
        displayCombo->MakeReadyForUse();
        displayCombo->SetBounds(0, 0, 240, 32);
        displayCombo->SetText("A&B && Company");
        displayCombo->PaintBackground();
        Require(displayCombo->glyphs == 0, "display dropdown backgrounds never draw a second editor text pass");
        wchar_t literal[32]{};
        geometry.paintedText.clear();
        geometry.observeText = true;
        displayCombo->Paint();
        geometry.observeText = false;
        Require(geometry.paintedText == L"A&B && Company", "closed dropdowns render literal ampersands without mnemonic loss");
        displayCombo->GetText(literal, 32);
        Require(std::wcscmp(literal, L"A&B && Company") == 0, "dropdown presentation preserves literal ampersands");
        displayCombo->AddItem("A long translated spectator camera option", nullptr);
        displayCombo->SetBounds(ScreenWidth - 140, ScreenHeight - 40, 120, 32);
        displayCombo->ShowMenu();
        displayCombo->GetMenu()->PerformLayout();
        int popupX = 0;
        int popupY = 0;
        displayCombo->GetMenu()->GetPos(popupX, popupY);
        Require(displayCombo->GetMenu()->GetWide() > displayCombo->GetWide() &&
            popupX >= 0 && popupX + displayCombo->GetMenu()->GetWide() <= ScreenWidth &&
            popupY >= 0 && popupY + displayCombo->GetMenu()->GetTall() <= ScreenHeight,
            "narrow dropdowns fit complete captions and stay inside the screen");
        displayCombo->HideMenu();
        displayCombo->SelectAllText(false);
        int selectionStart = 0;
        int selectionEnd = 0;
        Require(!displayCombo->GetSelectedRange(selectionStart, selectionEnd),
            "a display dropdown never exposes text-editor selection under glass");
        auto* editableCombo = new vgui2::ComboBox(fixedWindow, "EditableCombo", 4, true);
        editableCombo->SetText("editable value");
        editableCombo->SelectAllText(false);
        Require(editableCombo->GetSelectedRange(selectionStart, selectionEnd),
            "editable dropdowns retain actual text selection");
        engine_fixture::uiStyle = 0.0f;
        fixedWindow->InvalidateLayout(true, true);
        Require(displayCombo->GetSelectedRange(selectionStart, selectionEnd),
            "Original retains the legacy dropdown selection behavior");
        displayCombo->glyphs = 0;
        displayCombo->PaintBackground();
        Require(displayCombo->glyphs > 0, "Original dropdowns retain their editor drawing pass");
        delete fixedWindow;
        engine_fixture::uiStyle = 1.0f;
        auto* longMenu = new vgui2::Menu(root, "OverflowMenu");
        const int rows = ScreenHeight / vgui2::smoked::Pixels(36) + 5;
        for (int row = 0; row < rows; ++row) {
            auto* item = new vgui2::MenuItem(longMenu, "OverflowRow", "Visible command", nullptr, false);
            longMenu->AddMenuItem(item);
        }
        auto* ownerHidden = longMenu->GetMenuItem(longMenu->GetMenuID(1));
        ownerHidden->SetVisible(false);
        longMenu->SetNumberOfVisibleItems(99);
        longMenu->SetVisible(true);
        longMenu->MakeReadyForUse();
        longMenu->InvalidateLayout(true, true);
        longMenu->PerformLayout();
        Require(longMenu->GetTall() <= ScreenHeight, "overflow menu fits the screen");
        auto* finalRow = longMenu->GetMenuItem(longMenu->GetMenuID(rows - 1));
        Require(!finalRow->IsVisible(), "overflow rows start outside the scroll window");
        for (int row = 0; row <= rows; ++row) {
            longMenu->OnKeyCodeTyped(vgui2::KEY_DOWN);
            longMenu->PerformLayout();
        }
        int finalX = 0;
        int finalY = 0;
        finalRow->GetPos(finalX, finalY);
        Require(finalRow->IsVisible() && finalY >= 0 && finalY + finalRow->GetTall() <= longMenu->GetTall(),
            "keyboard navigation scrolls the final command into the menu");
        Require(!ownerHidden->IsVisible(), "scrolling respects owner-hidden menu items");
        engine_fixture::uiStyle = 0.0f;
        longMenu->InvalidateLayout(true, true);
        longMenu->PerformLayout();
        Require(finalRow->IsVisible() && !ownerHidden->IsVisible(),
            "Original restores requested row visibility after a glass scroll window");
        Require(cse::ui::ControlsMenuLimits().find(longMenu) == cse::ui::ControlsMenuLimits().end(),
            "Original releases the temporary screen-height row limit");
        delete longMenu;
        auto* compactCaption = new vgui2::Button(root, "CompactCaption", "");
        compactCaption->SetScheme(tracker);
        compactCaption->SetBounds(0, 0, 104, 24);
        compactCaption->SetText(L"По умолчанию");
        compactCaption->MakeReadyForUse();
        int originalCaptionWide = 0;
        int originalCaptionTall = 0;
        compactCaption->GetContentSize(originalCaptionWide, originalCaptionTall);
        engine_fixture::uiStyle = 1.0f;
        compactCaption->InvalidateLayout(true, true);
        int captionWide = 0;
        int captionTall = 0;
        compactCaption->GetContentSize(captionWide, captionTall);
        Require(captionWide <= std::max(compactCaption->GetWide(), originalCaptionWide),
            "small fixed buttons retain fitting captions without increasing existing overflow");
        delete compactCaption;
        engine_fixture::uiStyle = 0.0f;
        auto* visibleArtwork = new vgui2::Panel(root, "VisibleDecoration");
        auto* hiddenArtwork = new vgui2::Panel(root, "HiddenDecoration");
        visibleArtwork->SetVisible(true);
        hiddenArtwork->SetVisible(false);
        engine_fixture::uiStyle = 1.0f;
        vgui2::smoked::Decorations({visibleArtwork, hiddenArtwork});
        Require(!visibleArtwork->IsVisible() && !hiddenArtwork->IsVisible(), "Smoked hides only shell artwork");
        engine_fixture::uiStyle = 0.0f;
        vgui2::smoked::Decorations({visibleArtwork, hiddenArtwork});
        Require(visibleArtwork->IsVisible() && !hiddenArtwork->IsVisible(),
            "Original restores the owner's exact decoration visibility");
        delete visibleArtwork;
        delete hiddenArtwork;
        auto* ownership = new ParentOwnedPresentation(root);
        ownership->SetScheme(tracker);
        ownership->InvalidateLayout(true, true);
        ownership->Activate();
        ownership->hidden->MakeReadyForUse();
        const Color originalHidden = ownership->hidden->GetBgColor();
        const vgui2::HFont originalPlaybackFont = ownership->playback->GetFont();
        Settle(*root);
        Capture(*ownership, output / "parent-owned-original.bmp");
        Require(ownership->HasPlaybackIcon(), "parent installs its playback image after child scheme setup");
        std::map<vgui2::Panel*, std::array<int, 4>> originalChildBounds;
        for (int i = 0; i < ownership->GetChildCount(); ++i) {
            auto* child = ownership->GetChild(i);
            auto& bounds = originalChildBounds[child];
            child->GetBounds(bounds[0], bounds[1], bounds[2], bounds[3]);
        }
        engine_fixture::uiStyle = 1.0f;
        Capture(*ownership, output / "parent-owned-smoked.bmp");
        Require(ownership->HasPlaybackIcon() && ownership->playback->GetBgColor() == Color(7, 19, 31, 0),
            "style refresh preserves parent-owned playback icons and spectator-like child transparency");
        ownership->hidden->SetVisible(true);
        Capture(*ownership, output / "previously-hidden-smoked.bmp");
        Require(ownership->hidden->GetBgColor() == Color(cse::ui::Field.r, cse::ui::Field.g,
            cse::ui::Field.b, cse::ui::Field.a), "a never-painted control adopts the current style when shown");
        engine_fixture::uiStyle = 0.0f;
        Capture(*ownership, output / "parent-owned-restored.bmp");
        wchar_t input[128]{};
        ownership->hidden->GetText(input, 128);
        for (const auto& item : originalChildBounds) {
            std::array<int, 4> bounds{};
            item.first->GetBounds(bounds[0], bounds[1], bounds[2], bounds[3]);
            Require(bounds == item.second, "Original restores child geometry on the very first painted frame");
        }
        Require(ownership->playback->GetFont() == originalPlaybackFont,
            "Original restores a parent-owned button's exact font");
        Require(ownership->HasPlaybackIcon() && ownership->hidden->GetBgColor() == originalHidden &&
            std::wcscmp(input, L"retained input") == 0,
            "Original restores hidden input colors without clearing edits or playback images");
        delete ownership;
        auto* bindings = new vgui2::Frame(root, "PopulatedBindings", true, false);
        bindings->SetBounds(0, 0, 440, 240);
        bindings->SetScheme(tracker);
        auto* table = new vgui2::SectionedListPanel(bindings, "Bindings");
        table->SetBounds(16, 48, 408, 160);
        table->AddSection(0, "Keyboard bindings");
        table->AddColumnToSection(0, "action", "Action", 0, 180);
        table->AddColumnToSection(0, "key", "Key", 0, 100);
        table->AddColumnToSection(0, "alternate", "Alternate", 0, 100);
        KeyValues* binding = new KeyValues("Binding", "action", "Jump");
        binding->SetString("key", "Space");
        binding->SetString("alternate", "Mouse 3");
        const int bindingID = table->AddItem(0, binding);
        binding->deleteThis();
        bindings->InvalidateLayout(true, true);
        bindings->Activate();
        Settle(*root);
        Capture(*bindings, output / "bindings-original.bmp");
        engine_fixture::uiStyle = 1.0f;
        Capture(*bindings, output / "bindings-smoked.bmp");
        Settle(*root);
        engine_fixture::uiStyle = 0.0f;
        Capture(*bindings, output / "bindings-restored.bmp");
        Settle(*root);
        Require(table->GetItemCount() == 1 &&
            std::strcmp(table->GetItemData(bindingID)->GetString("action"), "Jump") == 0,
            "live style switches preserve populated multi-column binding rows");
        delete bindings;
        vgui2::surface()->SetEmbeddedPanel(0);
        delete root;
        vgui2::ivgui()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        fs->Unmount();
        privateSystem.Restore();
        std::puts("PASS: live Options transactions, ownership, failure/retry, fit and native names");
    });
}
