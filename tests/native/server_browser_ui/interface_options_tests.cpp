#include "model_fixture.inc"
#include "controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "../../../src/GameUI/OptionsDialog.h"
#include "../../../src/GameUI/OptionsSubInterface.h"
#include "../../../src/common/interface_language.h"
#include "PropertyPage.h"
#include "PropertySheet.h"
#include "ComboBox.h"
#include "Menu.h"
#include "MenuItem.h"
#include "Button.h"
#include "Label.h"
#include "TextImage.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "filesystem.h"
#include "check.h"
#include "system_fixture.h"
#include "geometry_fixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
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
int restartCommands = 0;
int RecordClientCommand(char* command) {
    Require(std::strcmp(command, "_restart\n") == 0, "only the normal restart command is issued");
    Require(appliedOtherPages == expectedOtherPages, "all Options pages apply before language restart");
    ++restartCommands;
    return 1;
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
constexpr int ScreenWidth = 640;
constexpr int ScreenHeight = 480;
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

int main(int argc, char** argv) {
    Require(argc == 7, "usage: interface_options_tests vgui fs resources output language platform");
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
        geometry.Resize(ScreenWidth, ScreenHeight);
        CreateInterfaceFn factories[] = {system_fixture::Factory, geometry_fixture::Factory, guiFactory, fsFactory};
        Require(vgui2::VGuiControls_Init("INTERFACE_OPTIONS", factories, 4), "initialize controls");
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
        std::ofstream report(output / "fit.tsv");
        for (int kind = 0; kind < 3; ++kind) {
            testMod.kind = kind;
            system_fixture::current->SetRegistryString(cse::interface_language::PreferenceKey,
                kind == 1 ? "unsupported" : kind == 2 ? "polish" : "");
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
            auto reopenAfterRestart = [&] {
                delete dialog;
                dialog = new TestOptions(root);
                dialog->SetScheme(tracker);
                dialog->SetVisible(true);
                sheet = dialog->GetPropertySheet();
                page = dynamic_cast<COptionsSubInterface*>(sheet->GetPage(interfaceIndex));
                sheet->SetActivePage(page);
                Settle(*root);
                combo = dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("Language", false));
            };
            expectedOtherPages = sheet->GetNumPages() - 1;
            system_fixture::current->SetRegistryString(cse::interface_language::PreferenceKey, "russian");
            rejectSavedLanguage = true;
            dialog->ResetAllData();
            Settle(*root);
            Require(combo->GetActiveItemUserData()->GetString("language")[0] == '\0',
                "page must reject a native record rejected by startup");
            Select(*combo, "russian");
            Settle(*root);
            const int beforeRepair = system_fixture::current->stringWrites;
            int beforeRestart = restartCommands;
            appliedOtherPages = 0;
            vgui2::PHandle restartOwner;
            restartOwner.Set(dialog);
            dialog->OnCommand("Apply");
            dialog->OnCommand("OK");
            dialog->OnClose();
            Settle(*root);
            Require(restartOwner.Get() != nullptr, "close cannot discard a pending forced restart owner");
            Require(system_fixture::current->stringWrites == beforeRepair + 1,
                "choosing a rejected stored id must rewrite a valid preference");
            Require(restartCommands == beforeRestart + 1, "changed language Apply forces one restart");
            rejectSavedLanguage = false;
            reopenAfterRestart();
            system_fixture::current->SetRegistryString(cse::interface_language::PreferenceKey,
                kind == 2 ? "polish" : "");
            dialog->ResetAllData();
            Settle(*root);
            const int writes = system_fixture::current->stringWrites;
            beforeRestart = restartCommands;
            dialog->OnCommand("Apply");
            Settle(*root);
            Require(system_fixture::current->stringWrites == writes, "unchanged Apply must not write");
            Require(restartCommands == beforeRestart, "unchanged Apply must not restart");
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
            combo->OnKeyTyped(L'p');
            Settle(*root);
            Require(std::strcmp(combo->GetActiveItemUserData()->GetString("language"), "polish") == 0,
                "keyboard language selection");
            Select(*combo, "russian");
            Settle(*root);
            Require(dialog->IsApplyButtonEnabled(), "selection enables Apply");
            beforeRestart = restartCommands;
            system_fixture::current->failStringWrites = true;
            dynamic_cast<vgui2::Button*>(dialog->FindChildByName("OKButton", false))->DoClick();
            Settle(*root);
            Require(dialog->IsVisible() && dialog->IsApplyButtonEnabled(), "failed save stays open for retry");
            Require(page->FindChildByName("SaveFailed", false)->IsVisible(), "save error visible");
            Require(restartCommands == beforeRestart, "failed save must not restart");
            Fit(*dialog, report);
            system_fixture::current->failStringWrites = false;
            appliedOtherPages = 0;
            dynamic_cast<vgui2::Button*>(dialog->FindChildByName("ApplyButton", false))->DoClick();
            Settle(*root);
            char saved[cse::interface_language::PreferenceBufferBytes]{};
            Require(system_fixture::current->GetRegistryString(cse::interface_language::PreferenceKey, saved, sizeof(saved))
                && std::strcmp(saved, "russian") == 0, "Apply persists stable id");
            Require(restartCommands == beforeRestart + 1, "successful save retry forces restart");
            reopenAfterRestart();
            Select(*combo, "polish");
            Settle(*root);
            dialog->ResetAllData();
            Settle(*root);
            Require(std::strcmp(combo->GetActiveItemUserData()->GetString("language"), "russian") == 0,
                "reset restores saved value");
            Select(*combo, "");
            beforeRestart = restartCommands;
            appliedOtherPages = 0;
            dialog->OnCommand("Apply");
            Settle(*root);
            Require(system_fixture::current->GetRegistryString(cse::interface_language::PreferenceKey, saved, sizeof(saved))
                && saved[0] == '\0', "automatic clears saved id");
            Require(restartCommands == beforeRestart + 1, "restoring automatic forces restart");
            reopenAfterRestart();
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
            }
            Select(*combo, "polish");
            const int beforeCancel = system_fixture::current->stringWrites;
            beforeRestart = restartCommands;
            if (kind == 2) {
                appliedOtherPages = 0;
                dialog->OnCommand("OK");
                Require(system_fixture::current->GetRegistryString(cse::interface_language::PreferenceKey, saved, sizeof(saved))
                    && std::strcmp(saved, "polish") == 0, "OK persists selection");
                Settle(*root);
                Require(restartCommands == beforeRestart + 1, "OK forces restart with launch override present");
                delete dialog;
            } else {
                dynamic_cast<vgui2::Button*>(dialog->FindChildByName("CancelButton", false))->DoClick();
                Settle(*root);
                Require(system_fixture::current->stringWrites == beforeCancel, "Cancel must not persist selection");
                Require(restartCommands == beforeRestart, "Cancel must not restart");
            }
            Settle(*root);
        }
        vgui2::surface()->SetEmbeddedPanel(0);
        delete root;
        vgui2::ivgui()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        fs->Unmount();
        privateSystem.Restore();
        std::puts("PASS: Options placement, fit and language preference behavior");
    });
}
