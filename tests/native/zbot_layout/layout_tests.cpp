#include "model_fixture.inc"
#include "controls.h"
#include "../server_browser_ui/check.h"
#include "../server_browser_ui/geometry_fixture.h"
#include "../server_browser_ui/system_fixture.h"
#include "Label.h"
#include "CheckButton.h"
#include "RadioButton.h"
#include "ComboBox.h"
#include "TextEntry.h"
#include "Divider.h"
#include "TextImage.h"
#include "vgui/IVGui.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "filesystem.h"
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>

extern "C" void CseSteamModuleInitialize() {}

namespace {
constexpr int ScreenWidth = 2560;
constexpr int ScreenHeight = 1440;
constexpr int PageWidth = 436;
constexpr int PageHeight = 365;
constexpr int HdScale = 2;
constexpr int SettleFrames = 6;

void Settle(vgui2::Panel& root) {
    for (int frame = 0; frame < SettleFrames; ++frame) {
        vgui2::ivgui()->RunFrame();
        vgui2::surface()->SolveTraverse(root.GetVPanel(), true);
    }
}

void Capture(vgui2::Panel& root, const std::filesystem::path& path) {
    root.Repaint();
    vgui2::surface()->PaintTraverse(root.GetVPanel());
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    Require(count > 0, "actual VGUI render window");
    SDL_Renderer* renderer = nullptr;
    for (int index = 0; index < count; ++index) {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(windows[index], &width, &height);
        if (width == root.GetWide() && height == root.GetTall()) {
            renderer = SDL_GetRenderer(windows[index]);
            break;
        }
    }
    SDL_free(windows);
    Require(renderer != nullptr, "actual SDL renderer");
    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    Require(pixels != nullptr, "actual layout pixels");
    const bool saved = SDL_SaveBMP(pixels, path.c_str());
    SDL_DestroySurface(pixels);
    Require(saved, "save layout screenshot");
}

vgui2::Panel* CreatePage(vgui2::Panel& parent, const char* name, const char* resource,
                        IFileSystem& filesystem) {
    auto* page = new vgui2::Panel(&parent, name);
    page->SetHDProportional(true);
    page->SetBounds(0, 0, PageWidth * HdScale, PageHeight * HdScale);
    auto* settings = new KeyValues("Layout");
    Require(settings->LoadFromFile(&filesystem, resource, "GAME"), "load actual ZBot resource");
    // Page bounds come from the fixed-size dialog's PropertySheet. Exercise each
    // actual resource/control with that geometry and explicit engine HD scale.
    for (auto* entry = settings->GetFirstSubKey(); entry; entry = entry->GetNextKey()) {
        const char* kind = entry->GetString("ControlName");
        vgui2::Panel* control = nullptr;
        if (!std::strcmp(kind, "CCvarToggleCheckButton") || !std::strcmp(kind, "CheckButton")) {
            control = new vgui2::CheckButton(page, entry->GetName(), "");
        } else if (!std::strcmp(kind, "RadioButton")) {
            control = new vgui2::RadioButton(page, entry->GetName(), "");
        } else if (!std::strcmp(kind, "Label")) {
            control = new vgui2::Label(page, entry->GetName(), "");
        } else if (!std::strcmp(kind, "ComboBox")) {
            control = new vgui2::ComboBox(page, entry->GetName(), 4, false);
        } else if (!std::strcmp(kind, "TextEntry")) {
            control = new vgui2::TextEntry(page, entry->GetName());
        } else if (!std::strcmp(kind, "Divider")) {
            control = new vgui2::Divider(page, entry->GetName());
        }
        if (control) {
            control->SetHDProportional(true);
            control->ApplySettings(entry);
        }
    }
    settings->deleteThis();
    return page;
}

bool Check(vgui2::Panel& page, std::ofstream& report) {
    bool fit = true;
    for (int index = 0; index < page.GetChildCount(); ++index) {
        auto* child = page.GetChild(index);
        auto* label = dynamic_cast<vgui2::Label*>(child);
        if (!label || !label->IsVisible()) {
            continue;
        }
        int width = 0;
        int height = 0;
        label->GetContentSize(width, height);
        int x = 0;
        int y = 0;
        label->GetPos(x, y);
        const bool fits = width <= label->GetWide() && height <= label->GetTall()
            && x >= 0 && y >= 0 && x + label->GetWide() <= page.GetWide()
            && y + label->GetTall() <= page.GetTall();
        report << page.GetName() << '\t' << child->GetName() << '\t' << label->GetWide() << '\t'
               << label->GetTall() << '\t' << width << '\t' << height << '\t' << fits << '\n';
        fit = fits && fit;
    }
    return fit;
}
}

int main(int argc, char** argv) {
    Require(argc == 9, "usage: zbot_layout vgui fs translations zbot platform valve output language");
    return RunCase([&] {
        void* guiImage = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void* fsImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(guiImage && fsImage, "load actual UI and filesystem providers");
        auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(guiImage, "CreateInterface"));
        auto fsFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(fsImage, "CreateInterface"));
        auto* fs = static_cast<IFileSystem*>(fsFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(guiImage,
            static_cast<vgui2::ISystem*>(factory("VGUI_System009", nullptr)));
        fs->Mount();
        for (const char* path : {argv[4], argv[6]}) {
            fs->AddSearchPathNoWrite(path, "GAME");
        }
        fs->AddSearchPathNoWrite(argv[5], "PLATFORM");
        for (const char* directory : {"platform", "valve", "cstrike"}) {
            fs->AddSearchPathNoWrite((std::string(argv[3]) + "/" + directory).c_str(), "GAME");
        }
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface*>(factory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = &geometry;
        geometry.Resize(ScreenWidth, ScreenHeight);
        CreateInterfaceFn factories[] = {system_fixture::Factory, geometry_fixture::Factory, factory, fsFactory};
        Require(vgui2::VGuiControls_Init("ZBOT_LAYOUT", factories, 4), "initialize actual controls");
        vgui2::surface()->SetLanguage(argv[8]);
        for (const char* path : {"resource/valve_%language%.txt", "resource/gameui_%language%.txt",
                                "resource/cstrike_%language%.txt", "resource/vgui_%language%.txt"}) {
            Require(vgui2::localize()->AddFile(fs, path), "load actual localized labels");
        }
        const auto scheme = vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", "Tracker");
        Require(scheme != 0, "actual game UI scheme");
        vgui2::ivgui()->Start();
        vgui2::ivgui()->SetSleep(false);
        auto* root = new vgui2::Panel(nullptr, "ZBotLayoutRoot");
        root->SetHDProportional(true);
        root->SetProportional(true);
        root->SetBounds(0, 0, PageWidth * HdScale, PageHeight * HdScale);
        root->SetScheme(scheme);
        geometry.Root(root->GetVPanel());
        auto* server = CreatePage(*root, "ServerPage", "resource/CseZBotServerPage.res", *fs);
        auto* bots = CreatePage(*root, "BotPage", "resource/CreateMultiplayerGameBotPage.res", *fs);
        for (auto* page : {server, bots}) {
            page->SetBounds(0, 0, PageWidth * HdScale, PageHeight * HdScale);
            static_cast<vgui2::Panel*>(page)->SetVisible(false);
            for (int index = 0; index < page->GetChildCount(); ++index) {
                if (auto* check = dynamic_cast<vgui2::CheckButton*>(page->GetChild(index))) {
                    check->SetSelected(true);
                }
            }
        }
        auto* team = dynamic_cast<vgui2::ComboBox*>(bots->FindChildByName("BotJoinTeamCombo", false));
        auto* chatter = dynamic_cast<vgui2::ComboBox*>(bots->FindChildByName("BotChatterCombo", false));
        Require(team && chatter, "actual bot dropdowns");
        team->AddItem("#Cstrike_Random", nullptr);
        team->ActivateItem(0);
        chatter->AddItem("#Cstrike_Bot_Chatter_Normal", nullptr);
        chatter->ActivateItem(0);
        const std::filesystem::path output = argv[7];
        std::filesystem::create_directories(output);
        std::ofstream report(output / "fit.tsv");
        bool fit = true;
        for (auto* page : {server, bots}) {
            static_cast<vgui2::Panel*>(page)->SetVisible(true);
            Settle(*root);
            fit = Check(*page, report) && fit;
            Capture(*root, output / (std::string(page->GetName()) + ".bmp"));
            static_cast<vgui2::Panel*>(page)->SetVisible(false);
        }
        report.close();
        Require(fit, "ZBot controls clip text or exceed their page; see fit.tsv");
        delete root;
        std::printf("ZBot server and bot options fit actual %s fonts at 2x HD scale\n", argv[8]);
    });
}
