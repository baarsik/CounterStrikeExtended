#include "model_fixture.inc"
#include "controls.h"
#include "../server_browser_ui/check.h"
#include "vgui/ISurface.h"
#include <map>
#include <string>
namespace geometry_fixture { inline std::map<vgui2::VPANEL, std::wstring> captions; }
#include "../server_browser_ui/geometry_fixture.h"
#include "../server_browser_ui/system_fixture.h"
#include "../../../src/public/localization_service.h"
#include "Label.h"
#include "TextEntry.h"
#include "Frame.h"
#include "ComboBox.h"
#include "Menu.h"
#include "RichText.h"
#include "ScrollBar.h"
#include "ListPanel.h"
#include "../../../src/public/localization_controls.h"
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
class SignalSink final : public vgui2::Panel {
public:
    explicit SignalSink(vgui2::Panel* parent) : Panel(parent, "Signals") {}
    int changes = 0;
    void OnMessage(const KeyValues* message, vgui2::VPANEL sender) override {
        if (std::strcmp(message->GetName(), "TextChanged") == 0) { ++changes; }
        else { Panel::OnMessage(message, sender); }
    }
};
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
            L"\"lang\" { \"Tokens\" { \"LiveToken\" \"English\" \"Fallback\" \"Base\" \"Count\" \"Count %count%\" \"Title\" \"Window %count%\" } }");
        WriteCatalogue(output / "resource/test_polish.txt",
            L"\"lang\" { \"Tokens\" { \"LiveToken\" \"Polski\" \"PolishOnly\" \"Tylko\" \"Count\" \"Liczba %count%\" \"Title\" \"Okno %count%\" } }");
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
        {
        auto* sink = new SignalSink(root);
        auto* countLabel = new vgui2::Label(root, "Count", "#Count");
        auto* missingLabel = new vgui2::Label(root, "Missing", "#LaterFormat");
        KeyValues variables("vars");
        variables.SetWString("count", L"7");
        countLabel->OnDialogVariablesChanged(&variables);
        missingLabel->OnDialogVariablesChanged(&variables);
        auto* title = new vgui2::Frame(root, "Title", false, false);
        title->SetTitle("#Title", true);
        title->OnDialogVariablesChanged(&variables);
        auto* literalTitle = new vgui2::Frame(root, "LiteralTitle", false, false);
        literalTitle->SetTitle(L"#LiveToken", true);
        auto* combo = new vgui2::ComboBox(root, "Readonly", 3, false);
        combo->AddActionSignalTarget(sink);
        const int selected = combo->AddItem("#LiveToken", nullptr);
        combo->ActivateItem(selected);
        auto* editable = new vgui2::ComboBox(root, "Editable", 3, true);
        editable->SetText(L"#LiveToken");
        auto* rows = new vgui2::ListPanel(root, "Rows");
        rows->AddColumnHeader(0, "name", "Name", 200, 0);
        KeyValues row("row");
        row.SetString("name", "#LiveToken");
        const int tokenRow = rows->AddItem(&row, 0, false, false);
        row.SetInt("__cse_literal_name", 1);
        const int literalRow = rows->AddItem(&row, 0, false, false);
        auto* rich = new vgui2::RichText(root, "Rich", false);
        rich->SetBounds(270, 0, 250, 100);
        rich->SetVerticalScrollbar(true);
        Settle(*root);
        std::wstring longBody;
        for (int line = 0; line < 60; ++line) { longBody += L"Selected scrolled content\n"; }
        rich->SetText(longBody.c_str());
        rich->OnThink();
        rich->_select[0] = 60;
        rich->_select[1] = 90;
        rich->_cursorPos = 85;
        rich->_vertScrollBar->SetValue(4);
        const int scroll = rich->_vertScrollBar->GetValue();
        Require(scroll > 0, "fixture has scrolled content");
        rich->SetTextPreservingView(longBody.c_str());
        Require(rich->_select[0] == 60 && rich->_select[1] == 90 && rich->_cursorPos == 85,
            "RichText preserves selection/cursor");
        Require(rich->_vertScrollBar->GetValue() == scroll, "RichText preserves scroll");
        rich->SetTextPreservingView(L"tiny");
        Require(rich->_select[0] == 4 && rich->_select[1] == 4 && rich->_cursorPos == 4,
            "RichText clamps selection/cursor");
        Require(rich->_vertScrollBar->GetValue() == 0, "RichText clamps viewport");
        rich->SelectNone();
        rich->SetTextPreservingView(L"replacement");
        Require(rich->_select[0] == -1 && rich->_select[1] == -1, "RichText absent selection");
        Settle(*root);
        const int initialChanges = sink->changes;
        vgui2::localize()->AddString("LaterFormat", const_cast<wchar_t*>(L"Later %count%"), "fixture");
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
        combo->RefreshLanguageText();
        title->RefreshLanguageText();
        Settle(*root);
        RequireText(*countLabel, L"Liczba 7");
        RequireText(*missingLabel, L"Later 7");
        wchar_t comboText[256]{};
        combo->GetText(comboText, 256);
        Require(std::wcscmp(comboText, L"Polski") == 0 && combo->GetActiveItem() == selected,
            "readonly combo refreshes display without reselection");
        editable->GetText(comboText, 256);
        Require(std::wcscmp(comboText, L"#LiveToken") == 0, "editable literal remains untouched");
        Require(sink->changes == initialChanges, "language refresh sends no TextChanged");
        Require(geometry_fixture::captions[title->GetVPanel()] == L"Okno 7", "native title variables refresh");
        Require(geometry_fixture::captions[literalTitle->GetVPanel()] == L"#LiveToken", "literal caption retained");
        wchar_t cell[256]{};
        Require(rows->GetCellText(tokenRow, 0, cell, 256) && std::wcscmp(cell, L"Polski") == 0,
            "token row follows revision");
        Require(rows->GetItem(tokenRow)->GetDataType("name") == KeyValues::TYPE_STRING,
            "rendering preserves source type");
        Require(rows->GetCellText(literalRow, 0, cell, 256) && std::wcscmp(cell, L"#LiveToken") == 0,
            "marked player row stays literal");
        std::wstring oversized(40000, L'x');
        {
            vgui2::TextImage image(oversized.c_str());
            Require(image._textLen == 32766 && image._textBufferLen == 32767, "signed-short text bounds");
            wchar_t small[4]{};
            image.GetText(small, 4);
            Require(small[3] == 0 && std::wcslen(small) == 3, "bounded wide output terminates");
        }

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
        geometry.Root(0);
        delete root;
        for (int frame = 0; frame < 4; ++frame) {
            vgui2::ivgui()->RunFrame();
        }
        for (const auto& entry : cse::localization::TextBindings()) {
            std::fprintf(stderr, "remaining binding %p token=%s\n", entry.first, entry.second.token.c_str());
        }
        Require(cse::localization::TextBindings().empty(), "text owners retire");
        Require(cse::localization::ControlRevisions().empty() && cse::localization::NativeCaptions().empty(),
            "control side states retire");
        }
        vgui2::ivgui()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        fs->Unmount();
        privateSystem.Restore();
        std::puts("PASS: staged live catalogue, stable bindings, literal state and repeated revision changes");
    });
}
