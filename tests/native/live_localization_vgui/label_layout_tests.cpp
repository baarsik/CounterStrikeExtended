#include "model_fixture.inc"
#include "controls.h"
#include "../server_browser_ui/check.h"
#include "../server_browser_ui/geometry_fixture.h"
#include "../server_browser_ui/system_fixture.h"
#include "../../../src/public/localization_service.h"
#include "Label.h"
#include "TextImage.h"
#include "Button.h"
#include "vgui/IVGui.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "filesystem.h"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cwchar>

extern "C" void CseSteamModuleInitialize() {}

namespace {
constexpr int Width = 640;
constexpr int Height = 480;
constexpr int LabelWidth = 236;
constexpr int TextCells = 256;
class RootPanel : public vgui2::Panel {
public:
    RootPanel() : Panel(nullptr, "LabelLayoutRoot") {}
    void OnCommand(const char* command) override {
        Require(std::strcmp(command, "preserved_action") == 0, "button action binding preserved");
        ++commands;
    }
    int commands = 0;
};
void WriteCatalogue(const std::filesystem::path& path, const std::wstring& text) {
    std::ofstream stream(path, std::ios::binary);
    const unsigned char bom[] = {0xff, 0xfe};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    for (const wchar_t cell : text) {
        const unsigned char bytes[] = {static_cast<unsigned char>(cell & 0xff),
            static_cast<unsigned char>((cell >> 8) & 0xff)};
        stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
}
void Settle(vgui2::Panel& root) {
    for (int frame = 0; frame < 4; ++frame) {
        vgui2::ivgui()->RunFrame();
        vgui2::surface()->SolveTraverse(root.GetVPanel(), true);
    }
}
int DrawWidth(vgui2::Label& label) {
    int width = 0;
    label.GetTextImage()->GetDrawWidth(width);
    return width;
}
int ContentWidth(vgui2::Label& label) {
    int width = 0;
    int height = 0;
    label.GetTextImage()->GetContentSize(width, height);
    return width;
}
}

int main(int argc, char** argv) {
    Require(argc == 5, "usage: label_layout_tests vgui fs platform output");
    return RunCase([&] {
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void* fsImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(image && fsImage, "load actual provider images");
        auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(image, "CreateInterface"));
        auto fsFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(fsImage, "CreateInterface"));
        const auto* runtime = cse::localization::QueryRuntime(factory);
        Require(runtime != nullptr, "current localization runtime provider");
        auto* fs = static_cast<IFileSystem*>(fsFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(image,
            static_cast<vgui2::ISystem*>(factory("VGUI_System009", nullptr)));
        const std::filesystem::path output = std::filesystem::absolute(argv[4]);
        std::filesystem::create_directories(output / "resource");
        WriteCatalogue(output / "resource/layout_english.txt",
            L"\"lang\" { \"Tokens\" { \"LayoutToken\" \"&Go\" } }");
        WriteCatalogue(output / "resource/layout_polish.txt",
            L"\"lang\" { \"Tokens\" { \"LayoutToken\" \"&Choose your team\" } }");
        fs->Mount();
        fs->AddSearchPathNoWrite(output.c_str(), "GAME");
        fs->AddSearchPathNoWrite(argv[3], "PLATFORM");
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface*>(factory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = &geometry;
        geometry.Resize(Width, Height);
        CreateInterfaceFn factories[] = {system_fixture::Factory, geometry_fixture::Factory, factory, fsFactory};
        Require(vgui2::VGuiControls_Init("LABEL_LAYOUT", factories, 4), "initialize actual controls");
        vgui2::surface()->SetLanguage("english");
        Require(vgui2::localize()->AddFile(fs, "resource/layout_%language%.txt"), "initial catalogue");
        const auto tracker = vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", "Tracker");
        Require(tracker != 0, "actual scheme");
        vgui2::ivgui()->Start();
        vgui2::ivgui()->SetSleep(false);
        auto* root = new RootPanel;
        root->SetBounds(0, 0, Width, Height);
        root->SetScheme(tracker);
        geometry.Root(root->GetVPanel());
        auto* label = new vgui2::Label(root, "WideLabel", "#LayoutToken");
        label->SetBounds(0, 0, LabelWidth, 40);
        auto* button = new vgui2::Button(root, "WideButton", "#LayoutToken");
        button->SetBounds(0, 50, LabelWidth, 40);
        button->SetCommand("preserved_action");
        button->AddActionSignalTarget(root);
        auto* literal = new vgui2::Label(root, "Literal", L"#LayoutToken");
        literal->SetBounds(0, 100, LabelWidth, 40);
        button->SetSelected(true);
        Settle(*root);
        const int originalLabelWidth = DrawWidth(*label);
        const int originalButtonWidth = DrawWidth(*button);
        Require(label->HasHotkey(L'g') == label && button->HasHotkey(L'g') == button, "initial hotkeys");
        Require(runtime->PrepareLanguage(fs, "polish"), "prepare long label language");
        vgui2::surface()->SetLanguage("polish");
        runtime->CommitLanguage(2);
        // A getter may lazily consume the TextImage revision before Label Think.
        // The label must independently observe the revision and update its layout.
        wchar_t text[TextCells]{};
        label->GetText(text, TextCells);
        Require(std::wcscmp(text, L"&Choose your team") == 0, "long translated text already available");
        Settle(*root);
        const int needed = ContentWidth(*label);
        std::ofstream report(output / "draw-widths.tsv");
        report << "label\t" << originalLabelWidth << '\t' << DrawWidth(*label) << '\t' << needed << '\n';
        report << "button\t" << originalButtonWidth << '\t' << DrawWidth(*button) << '\t' << ContentWidth(*button) << '\n';
        Require(needed > originalLabelWidth && needed <= LabelWidth, "replacement fits the unchanged wide label");
        Require(DrawWidth(*label) >= needed, "Label drawWidth remains stale after short-to-long token refresh");
        Require(DrawWidth(*button) >= ContentWidth(*button), "Button inherited label drawWidth refresh");
        Require(label->HasHotkey(L'c') == label && label->HasHotkey(L'g') == nullptr,
            "localized label accelerator updates with layout");
        Require(button->HasHotkey(L'c') == button && button->IsSelected() && button->IsVisible(),
            "button accelerator changes without selection or visibility reset");
        literal->GetText(text, TextCells);
        Require(std::wcscmp(text, L"#LayoutToken") == 0, "literal label remains literal");
        Require(root->commands == 0, "revision update does not dispatch an action");
        button->DoClick();
        Settle(*root);
        Require(root->commands == 1, "one later user click retains its original action binding");
        vgui2::surface()->SetEmbeddedPanel(0);
        delete root;
        vgui2::ivgui()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        fs->Unmount();
        privateSystem.Restore();
        std::puts("PASS actual Label/Button: token revision refreshes drawWidth/hotkey without resetting action or selection");
    });
}
