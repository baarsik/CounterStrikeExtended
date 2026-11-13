#include "model_fixture.inc"
#include "controls.h"
#include "../server_browser_ui/check.h"
#include "../server_browser_ui/geometry_fixture.h"
#include "../server_browser_ui/system_fixture.h"
#include "../../../src/public/localization_service.h"
#include "Label.h"
#include "TextEntry.h"
#include "Frame.h"
#include "ComboBox.h"
#include "Menu.h"
#include "vgui/IVGui.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
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
constexpr int TextCells = 256;
void RequireText(vgui2::Label& label, const wchar_t* expected) {
    wchar_t text[TextCells]{};
    label.GetText(text, TextCells);
    Require(std::wcscmp(text, expected) == 0, "bound label did not refresh to committed language");
}
void WriteCatalogue(const std::filesystem::path& path, const std::wstring& body) {
    std::ofstream stream(path, std::ios::binary);
    const unsigned char bom[] = {0xff, 0xfe};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    for (const wchar_t cell : body) {
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
}

int main(int argc, char** argv) {
    Require(argc == 5, "usage: live_localization_vgui vgui fs platform output");
    return RunCase([&] {
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void* fsImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(image && fsImage, "load actual provider images");
        auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(image, "CreateInterface"));
        auto fsFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(fsImage, "CreateInterface"));
        const auto* runtime = cse::localization::QueryRuntime(factory);
        Require(runtime != nullptr, "engine-coordinated VGUI localization provider is absent");
        auto* fs = static_cast<IFileSystem*>(fsFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(image,
            static_cast<vgui2::ISystem*>(factory("VGUI_System009", nullptr)));
        const std::filesystem::path output = std::filesystem::absolute(argv[4]);
        std::filesystem::create_directories(output / "resource");
        WriteCatalogue(output / "resource/test_english.txt",
            L"\"lang\" { \"Tokens\" { \"LiveToken\" \"English\" \"Fallback\" \"Base\" } }");
        WriteCatalogue(output / "resource/test_polish.txt",
            L"\"lang\" { \"Tokens\" { \"LiveToken\" \"Polski\" \"PolishOnly\" \"Tylko\" } }");
        WriteCatalogue(output / "resource/test_russian.txt",
            L"\"lang\" { \"Tokens\" { \"LiveToken\" \"Русский\" } }");
        fs->Mount();
        fs->AddSearchPathNoWrite(output.c_str(), "GAME");
        fs->AddSearchPathNoWrite(argv[3], "PLATFORM");
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface*>(factory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = &geometry;
        geometry.Resize(Width, Height);
        CreateInterfaceFn factories[] = {system_fixture::Factory, geometry_fixture::Factory, factory, fsFactory};
        Require(vgui2::VGuiControls_Init("LIVE_LOCALIZATION", factories, 4), "initialize actual controls");
        vgui2::surface()->SetLanguage("english");
        Require(vgui2::localize()->AddFile(fs, "resource/test_%language%.txt"), "initial logical catalogue load");
        const auto index = vgui2::localize()->FindIndex("LiveToken");
        const wchar_t* borrowedEnglish = vgui2::localize()->Find("LiveToken");
        const char* borrowedName = vgui2::localize()->GetNameByIndex(index);
        const auto tracker = vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", "Tracker");
        Require(tracker != 0, "load actual scheme");
        vgui2::ivgui()->Start();
        vgui2::ivgui()->SetSleep(false);
        auto* root = new vgui2::Panel(nullptr, "LiveRoot");
        root->SetBounds(0, 0, Width, Height);
        root->SetScheme(tracker);
        geometry.Root(root->GetVPanel());
        auto* label = new vgui2::Label(root, "Bound", "#LiveToken");
        label->SetBounds(0, 0, 300, 40);
        auto* literal = new vgui2::Label(root, "Literal", L"#LiveToken");
        literal->SetBounds(0, 40, 300, 40);
        auto* input = new vgui2::TextEntry(root, "Input");
        input->SetText("player supplied #LiveToken");
        input->SetBounds(0, 80, 300, 40);
        Settle(*root);
        Require(runtime->PrepareLanguage(fs, "polish"), "prepare supported language");
        RequireText(*label, L"English");
        Require(std::wcscmp(vgui2::localize()->Find("LiveToken"), L"English") == 0,
            "prepare mutated active catalogue");
        vgui2::surface()->SetLanguage("polish");
        runtime->CommitLanguage(2);
        Settle(*root);
        Require(runtime->Revision() == 2, "committed provider revision");
        Require(vgui2::localize()->FindIndex("LiveToken") == index, "token identity changed on commit");
        Require(std::wcscmp(vgui2::localize()->Find("Fallback"), L"Base") == 0, "English token fallback");
        RequireText(*label, L"Polski");
        RequireText(*literal, L"#LiveToken");
        wchar_t inputText[TextCells]{};
        input->GetText(inputText, TextCells);
        Require(std::wcscmp(inputText, L"player supplied #LiveToken") == 0, "literal input changed");
        Require(std::wcscmp(borrowedEnglish, L"English") == 0 && std::strcmp(borrowedName, "LiveToken") == 0,
            "borrowed legacy catalogue memory was invalidated");
        Require(runtime->PrepareLanguage(fs, "russian"), "prepare second language");
        runtime->DiscardPrepared();
        RequireText(*label, L"Polski");
        for (std::uint64_t revision = 3; revision < 43; ++revision) {
            const bool russian = revision % 2 != 0;
            const char* language = russian ? "russian" : "english";
            Require(runtime->PrepareLanguage(fs, language), "repeated prepare");
            vgui2::surface()->SetLanguage(language);
            runtime->CommitLanguage(revision);
            Settle(*root);
            RequireText(*label, russian ? L"Русский" : L"English");
            Require(vgui2::localize()->FindIndex("LiveToken") == index, "stable repeated token identity");
            Require(vgui2::localize()->Find("PolishOnly") == nullptr, "stale old-language-only token retained");
        }
        label->SetText(L"literal after token");
        Require(runtime->PrepareLanguage(fs, "polish"), "prepare final language");
        vgui2::surface()->SetLanguage("polish");
        runtime->CommitLanguage(43);
        Settle(*root);
        RequireText(*label, L"literal after token");
        Require(std::wcscmp(borrowedEnglish, L"English") == 0, "retained old value after repeated switches");
        vgui2::surface()->SetEmbeddedPanel(0);
        delete root;
        vgui2::ivgui()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        fs->Unmount();
        privateSystem.Restore();
        std::puts("PASS: staged live catalogue, stable bindings, literal state and repeated revision changes");
    });
}
