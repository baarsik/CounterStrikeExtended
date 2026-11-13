#include "model_fixture.inc"
#include "ServerBrowserDialog.h"
#include "detail_provider_fixture.h"
#include "controls.h"
#include "Label.h"
#include "Button.h"
#include "CheckButton.h"
#include "RadioButton.h"
#include "ToggleButton.h"
#include "PropertySheet.h"
#include "TextImage.h"
#include "vgui/IVGui.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "filesystem.h"
#include "check.h"
#include "system_fixture.h"
#include "geometry_fixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>

extern "C" void CseSteamModuleInitialize() {}

namespace {
constexpr int ScreenWidth = 640;
constexpr int ScreenHeight = 480;
constexpr int BrowserHeight = 384;
constexpr int TextBufferCells = 16384;
constexpr int MaximumUtf8BytesPerCell = 4;
constexpr int BrowserSettleFrames = 10;

struct Engine final : IConnectionEngine {
    std::string password;
    const char* Password() override {
        return password.c_str();
    }
    void SetPassword(const char* value) override {
        password = value;
    }
    void Connect(const char*) override {}
};

std::string FlatText(vgui2::Label& label) {
    wchar_t text[TextBufferCells]{};
    label.GetText(text, TextBufferCells);
    std::string result(TextBufferCells * MaximumUtf8BytesPerCell, '\0');
    vgui2::localize()->ConvertUnicodeToANSI(text, &result[0], static_cast<int>(result.size()));
    result.resize(std::strlen(result.c_str()));
    std::replace(result.begin(), result.end(), '\t', ' ');
    std::replace(result.begin(), result.end(), '\n', ' ');
    std::replace(result.begin(), result.end(), '\r', ' ');
    return result;
}

bool Record(std::ofstream& report, vgui2::Label& label, const char* source, const char* token,
    bool dynamicSummary = false) {
    bool textInstalled = false;
    for (int index = 0; index < label.GetImageCount(); ++index) {
        textInstalled = textInstalled || label.GetImageAtIndex(index) == label.GetTextImage();
    }
    if (!textInstalled || FlatText(label).empty()) {
        return true;
    }
    int width = 0;
    int height = 0;
    label.GetContentSize(width, height);
    int drawWidth = 0;
    label.GetTextImage()->GetDrawWidth(drawWidth);
    // This existing preview intentionally elides arbitrary game/map/filter data.
    // Complete translated filter controls are checked separately in the expanded page.
    const bool elided = dynamicSummary && width > label.GetWide() && height <= label.GetTall()
        && drawWidth > 0 && drawWidth <= label.GetWide();
    const bool fits = (width <= label.GetWide() && height <= label.GetTall()) || elided;
    report << source << '\t' << label.GetName() << '\t' << token << '\t'
        << label.GetWide() << '\t' << label.GetTall() << '\t' << width << '\t' << height << '\t'
        << (elided ? "elided-dynamic-summary" : fits ? "fit" : "overflow") << '\t' << FlatText(label) << '\n';
    return fits;
}

void Capture(vgui2::Panel& panel, const std::filesystem::path& path) {
    vgui2::surface()->SolveTraverse(panel.GetVPanel(), true);
    panel.Repaint();
    vgui2::surface()->PaintTraverse(panel.GetVPanel());
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    Require(count > 0, "actual VGUI surface did not create a native render target");
    SDL_Renderer* renderer = nullptr;
    for (int index = 0; index < count; ++index) {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(windows[index], &width, &height);
        if (width == panel.GetWide() && height == panel.GetTall()) {
            renderer = SDL_GetRenderer(windows[index]);
            break;
        }
    }
    SDL_free(windows);
    Require(renderer != nullptr, "actual VGUI renderer is missing");
    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    Require(pixels != nullptr, "read actual VGUI rendered pixels");
    const bool saved = SDL_SaveBMP(pixels, path.c_str());
    SDL_DestroySurface(pixels);
    Require(saved, "save actual rendered UI sample");
}

bool CheckTree(std::ofstream& report, vgui2::Panel& root) {
    bool fits = true;
    if (!root.IsVisible()) {
        return true;
    }
    if (auto* label = dynamic_cast<vgui2::Label*>(&root)) {
        fits = Record(report, *label, "actual-browser", "runtime", std::strcmp(label->GetName(), "FilterString") == 0);
    }
    for (int index = 0; index < root.GetChildCount(); ++index) {
        fits = CheckTree(report, *root.GetChild(index)) && fits;
    }
    return fits;
}
}

int main(int argc, char** argv) {
    Require(argc == 9 || argc == 10,
        "usage: translation_fit_tests vgui fs platform game pack constraints output language [negative]");
    return RunCase([&] {
        const std::filesystem::path output = argv[7];
        std::filesystem::create_directories(output);
        void* guiImage = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void* filesystemImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(guiImage && filesystemImage, "load actual VGUI and filesystem images");
        auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(guiImage, "CreateInterface"));
        auto filesystemFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(filesystemImage, "CreateInterface"));
        auto* filesystem = static_cast<IFileSystem*>(filesystemFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(guiImage,
            static_cast<vgui2::ISystem*>(guiFactory("VGUI_System009", nullptr)));
        filesystem->Mount();
        for (const char* directory : {"platform", "valve", "cstrike"}) {
            filesystem->AddSearchPathNoWrite((std::string(argv[5]) + "/" + directory).c_str(), "GAME");
        }
        filesystem->AddSearchPathNoWrite(argv[3], "PLATFORM");
        filesystem->AddSearchPathNoWrite((std::string(argv[4]) + "/cstrike").c_str(), "GAME");
        filesystem->AddSearchPathNoWrite((std::string(argv[4]) + "/valve").c_str(), "DEFAULTGAME");
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface*>(guiFactory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = &geometry;
        geometry.Resize(ScreenWidth, ScreenHeight);
        CreateInterfaceFn factories[] = {system_fixture::Factory, geometry_fixture::Factory, guiFactory, filesystemFactory};
        constexpr int FactoryCount = sizeof(factories) / sizeof(factories[0]);
        Require(vgui2::VGuiControls_Init("TRANSLATION_FIT", factories, FactoryCount), "initialize actual controls");
        auto* gui = vgui2::ivgui();
        gui->Start();
        gui->SetSleep(false);
        auto* root = new vgui2::Panel(nullptr, "TranslationFitRoot");
        root->SetBounds(0, 0, ScreenWidth, ScreenHeight);
        root->SetBgColor(Color(38, 43, 46, 255));
        geometry.Root(root->GetVPanel());
        vgui2::surface()->SetLanguage(argv[8]);
        const auto tracker = vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", "Tracker");
        const auto client = vgui2::scheme()->LoadSchemeFromFile("resource/ClientScheme.res", "ClientScheme");
        Require(tracker && client, "load actual Tracker and Client schemes");
        root->SetScheme(tracker);
        for (const char* path : {"resource/valve_%language%.txt", "resource/gameui_%language%.txt",
                 "resource/cstrike_%language%.txt", "resource/platform_%language%.txt",
                 "resource/vgui_%language%.txt", "servers/serverbrowser_%language%.txt",
                 "Admin/admin_%language%.txt", "Admin/server_%language%.txt", "resource/cse_%language%.txt"}) {
            Require(vgui2::localize()->AddFile(filesystem, path), "load selected-language catalogue");
        }

        // Query the image-owned font manager. GetFontForChar tests the actual face's
        // glyph coverage and native fallback before returning a font (not a tofu width).
        auto manager = reinterpret_cast<void* (*)()>(dlsym(guiImage, "_Z11FontManagerv"));
        auto fontForChar = reinterpret_cast<void* (*)(void*, vgui2::HFont, wchar_t)>(
            dlsym(guiImage, "_ZN12CFontManager14GetFontForCharEjw"));
        Require(manager && fontForChar, "actual native font coverage probes");
        std::set<wchar_t> alphabet;
        auto* localize = vgui2::localize();
        for (auto index = localize->GetFirstStringIndex(); index != INVALID_STRING_INDEX;
             index = localize->GetNextStringIndex(index)) {
            for (const wchar_t* text = localize->GetValueByIndex(index); text && *text; ++text) {
                if (*text > L' ') {
                    alphabet.insert(*text);
                }
            }
        }
        for (auto scheme : {tracker, client}) {
            for (const char* name : {"Default", "DefaultSmall", "DefaultVerySmall"}) {
                const auto font = vgui2::scheme()->GetIScheme(scheme)->GetFont(name, false);
                Require(font != 0, "actual scheme body font");
                for (wchar_t character : alphabet) {
                    Require(fontForChar(manager(), font, character) != nullptr, "translation glyph absent from face/fallback");
                }
            }
        }
        std::ofstream report(output / "fit.tsv");
        report << "source\tcontrol\ttoken\twidth\theight\tneeded_width\tneeded_height\tresult\ttext\n";
        auto* controls = new KeyValues("TranslationFit");
        const std::filesystem::path constraints = std::filesystem::absolute(argv[6]);
        filesystem->AddSearchPathNoWrite(constraints.parent_path().c_str(), "FIT");
        Require(controls->LoadFromFile(filesystem, constraints.filename().c_str(), "FIT"), "load resource fit constraints");
        bool allFit = true;
        int tested = 0;
        std::set<std::string> captured;
        for (auto* settings = controls->GetFirstSubKey(); settings; settings = settings->GetNextKey()) {
            const std::string kind = settings->GetString("kind");
            const std::string category = settings->GetString("category");
            const char* token = settings->GetString("labeltext");
            if (!localize->Find(token)) {
                report << settings->GetString("source") << '\t' << settings->GetString("control") << '\t'
                    << token << "\t0\t0\t0\t0\tunresolved-stock-token\t\n";
                continue;
            }
            vgui2::Label* label = nullptr;
            const char* name = settings->GetString("control");
            if (kind == "CheckButton" || kind == "CCvarToggleCheckButton") {
                label = new vgui2::CheckButton(root, name, token);
            } else if (kind == "RadioButton") {
                label = new vgui2::RadioButton(root, name, token);
            } else if (kind.find("Button") != std::string::npos) {
                label = new vgui2::Button(root, name, token);
            } else {
                label = new vgui2::Label(root, name, token);
            }
            const bool clientControl = category.rfind("cstrike", 0) == 0;
            label->SetScheme(clientControl ? client : tracker);
            label->SetProportional(clientControl);
            KeyValues* ownedResource = nullptr;
            auto* appliedSettings = settings;
            if (category == "admin") {
                ownedResource = new KeyValues("AdminFit");
                const auto path = std::filesystem::path(settings->GetString("source")).lexically_relative("platform");
                Require(ownedResource->LoadFromFile(filesystem, path.c_str()), "load actual owned admin layout");
                appliedSettings = ownedResource->FindKey(name, false);
                Require(appliedSettings != nullptr, "actual admin control exists");
            }
            static_cast<vgui2::Panel*>(label)->ApplySettings(appliedSettings);
            label->InvalidateLayout(true, true);
            vgui2::surface()->SolveTraverse(root->GetVPanel(), true);
            if (argc == 10 && tested == 0) {
                label->SetText(L"This deliberately overlong translated button must fail the actual font fit check");
            }
            allFit = Record(report, *label, settings->GetString("source"), token) && allFit;
            if (!captured.count(category) && std::wcschr(localize->Find(token), L' ') != nullptr) {
                Capture(*root, output / (category + ".bmp"));
                captured.insert(category);
            }
            delete label;
            if (ownedResource) {
                ownedResource->deleteThis();
            }
            ++tested;
        }
        controls->deleteThis();
        std::printf("Measured %d actual controls; %zu characters across six actual scheme fonts\n", tested, alphabet.size());

        DetailProvider provider;
        Favorites favorites;
        Engine engine;
        BrowserSession session(&provider, nullptr, filesystem, engine, 10, "cstrike", output.string(), &favorites);
        auto* browser = new ServerBrowserDialog(session);
        browser->SetParent(root);
        browser->SetBounds(0, 0, ScreenWidth, BrowserHeight);
        browser->Activate();
        browser->Pump();
        provider.lists.Complete();
        for (int frame = 0; frame < BrowserSettleFrames; ++frame) {
            browser->Pump();
            gui->RunFrame();
        }
        vgui2::surface()->SolveTraverse(root->GetVPanel(), true);
        allFit = CheckTree(report, *browser) && allFit;
        auto* tabs = dynamic_cast<vgui2::PropertySheet*>(browser->FindChildByName("GameTabs", false));
        Require(tabs != nullptr, "actual browser tabs");
        int measuredTabs = 0;
        for (int index = 0; index < tabs->GetChildCount(); ++index) {
            auto* child = tabs->GetChild(index);
            if (std::strcmp(child->GetName(), "tab") == 0) {
                int x = 0;
                int y = 0;
                child->GetPos(x, y);
                allFit = x + child->GetWide() <= tabs->GetWide() && allFit;
                ++measuredTabs;
                report << "actual-browser-tabstrip\t" << child->GetName() << "\truntime\t"
                    << tabs->GetWide() << '\t' << child->GetTall() << '\t' << x + child->GetWide()
                    << '\t' << child->GetTall() << '\t'
                    << (x + child->GetWide() <= tabs->GetWide() ? "fit" : "overflow") << "\t\n";
            }
        }
        Require(measuredTabs == tabs->GetNumPages(), "measured every actual browser tab edge");
        Capture(*browser, output / "browser.bmp");
        auto* activePage = tabs->GetActivePage();
        Require(activePage != nullptr, "actual active browser page");
        auto* filter = dynamic_cast<vgui2::ToggleButton*>(activePage->FindChildByName("Filter", false));
        Require(filter != nullptr, "actual filter toggle");
        filter->DoClick();
        gui->RunFrame();
        vgui2::surface()->SolveTraverse(root->GetVPanel(), true);
        Require(filter->IsSelected(), "actual browser filter layout is expanded");
        auto* mapFilter = activePage->FindChildByName("MapFilter", false);
        Require(mapFilter && mapFilter->IsVisible(), "active page filter controls are visible");
        allFit = CheckTree(report, *browser) && allFit;
        Capture(*browser, output / "browser-filters.bmp");
        delete browser;
        vgui2::surface()->SetEmbeddedPanel(0);
        delete root;
        gui->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        filesystem->Unmount();
        privateSystem.Restore();
        Require(allFit, "translated control content exceeds its retained layout; see fit.tsv");
    });
}
