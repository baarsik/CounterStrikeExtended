#include "model_fixture.inc"
#include "controls.h"
#include "../server_browser_ui/check.h"
#include "../server_browser_ui/geometry_fixture.h"
#include "../server_browser_ui/system_fixture.h"
#include "../../../src/public/localization_service.h"
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
const cse::localization::FontBackend* realFonts = nullptr;
int fontStageCalls = 0;
bool CSE_ABI_CALL BeginFailingFonts(const char* language) { return realFonts->Begin(language); }
bool CSE_ABI_CALL StageFailingFont(std::uint32_t font, const char*, int tall, int weight,
    int blur, int scanlines, int flags) {
    ++fontStageCalls;
    return realFonts->Stage(font, "CSEFontThatDoesNotExist_5927", tall, weight, blur, scanlines, flags);
}
void CSE_ABI_CALL FinishFonts() { realFonts->Finish(); }
void CSE_ABI_CALL CommitFonts() { realFonts->Commit(); }
void CSE_ABI_CALL DiscardFonts() { realFonts->Discard(); }
const cse::localization::FontBackend failingFonts{sizeof(cse::localization::FontBackend),
    cse::localization::InterfaceVersion, BeginFailingFonts, StageFailingFont, FinishFonts,
    CommitFonts, DiscardFonts};
int resetCalls = 0;
void CSE_ABI_CALL CountReset(std::uint32_t) {
    ++resetCalls;
}
}

int main(int argc, char** argv) {
    Require(argc == 5, "usage: provider_tests vgui filesystem platform output");
    return RunCase([&] {
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void* fsImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(image && fsImage, "load actual providers");
        auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(image, "CreateInterface"));
        auto fsFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(fsImage, "CreateInterface"));
        const auto* runtime = cse::localization::QueryRuntime(factory);
        Require(runtime, "runtime localization provider exists");
        auto* fs = static_cast<IFileSystem*>(fsFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(image,
            static_cast<vgui2::ISystem*>(factory("VGUI_System009", nullptr)));
        const std::filesystem::path output = std::filesystem::absolute(argv[4]);
        std::filesystem::create_directories(output / "resource");
        const std::wstring russian = L"\"lang\" { \"Tokens\" { \"LiveToken\" \"Русский\" } }";
        WriteCatalogue(output / "resource/test_english.txt",
            L"\"lang\" { \"Tokens\" { \"LiveToken\" \"English\" \"Fallback\" \"Base\" \"Override\" \"Base\" } }");
        WriteCatalogue(output / "resource/test_polish.txt",
            L"\"lang\" { \"Tokens\" { \"LiveToken\" \"Polski\" \"PolishOnly\" \"Tylko\" \"Emergency\" \"Awaryjny\" } }");
        WriteCatalogue(output / "resource/test_russian.txt", russian);
        std::ofstream scheme(output / "resource/language_scheme.res");
        scheme << "\"Scheme\" { \"Fonts\" { \"LanguageFont\" { \"1\" { \"name\" \"Helvetica\" \"tall\" \"20\" \"weight\" \"400\" } } } }";
        scheme.close();
        fs->Mount();
        fs->AddSearchPathNoWrite(output.c_str(), "GAME");
        fs->AddSearchPathNoWrite(argv[3], "PLATFORM");
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface*>(factory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = &geometry;
        geometry.Resize(640, 480);
        CreateInterfaceFn factories[] = {system_fixture::Factory, geometry_fixture::Factory, factory, fsFactory};
        Require(vgui2::VGuiControls_Init("PROVIDER_LOCALIZATION", factories, 4), "initialize actual provider owners");
        auto* localize = vgui2::localize();
        vgui2::surface()->SetLanguage("english");
        Require(localize->AddFile(fs, "resource/test_%language%.txt"), "initial logical catalogue");
        const auto index = localize->FindIndex("LiveToken");
        const wchar_t* oldEnglish = localize->Find("LiveToken");
        const char* oldName = localize->GetNameByIndex(index);
        localize->AddString("Override", const_cast<wchar_t*>(L"Literal Override"), "fixture");
        localize->AddString("NewLiteral", const_cast<wchar_t*>(L"New Literal"), "fixture");
        localize->AddString("Emergency", const_cast<wchar_t*>(L"Emergency"), "CSEServerBrowser");
        localize->AddString("AlwaysMissing", const_cast<wchar_t*>(L"Emergency Only"), "CSEServerBrowser");
        const auto schemeHandle = vgui2::scheme()->LoadSchemeFromFile("resource/language_scheme.res", "LanguageTest");
        Require(schemeHandle != 0, "load language-metric scheme");
        auto* loadedScheme = vgui2::scheme()->GetIScheme(schemeHandle);
        const auto font = loadedScheme->GetFont("LanguageFont", false);
        const int englishHeight = vgui2::surface()->GetFontTall(font);
        Require(font && englishHeight > 0, "font initially configured");
        Require(!runtime->PrepareLanguage(fs, "../invalid"), "reject invalid language identifier");
        realFonts = runtime->BindFontBackend(nullptr);
        Require(realFonts != nullptr, "standalone staged font backend");
        runtime->BindFontBackend(&failingFonts);
        Require(!runtime->PrepareLanguage(fs, "polish"), "font creation failure rejects prepare");
        Require(fontStageCalls > 0 && runtime->Revision() == 0, "failed fonts do not publish revision");
        Require(vgui2::surface()->GetFontTall(font) == englishHeight && localize->Find("LiveToken") == oldEnglish,
            "failed font preparation preserves active font and catalogue");
        runtime->BindFontBackend(realFonts);
        Require(runtime->PrepareLanguage(fs, "polish"), "prepare polish");
        Require(std::wcscmp(localize->Find("LiveToken"), L"English") == 0, "prepare leaves active text unchanged");
        runtime->DiscardPrepared();
        Require(localize->Find("LiveToken") == oldEnglish, "discard preserves borrowed active pointer");
        Require(runtime->PrepareLanguage(fs, "polish"), "prepare polish again");
        vgui2::surface()->SetLanguage("polish");
        runtime->CommitLanguage(2);
        Require(runtime->Revision() == 2, "publish revision");
        Require(localize->FindIndex("LiveToken") == index, "stable token id");
        Require(std::wcscmp(localize->Find("LiveToken"), L"Polski") == 0, "polish committed");
        Require(std::wcscmp(localize->Find("Fallback"), L"Base") == 0, "English fallback");
        Require(std::wcscmp(localize->Find("Override"), L"Literal Override") == 0, "programmatic override retained");
        Require(std::wcscmp(localize->Find("NewLiteral"), L"New Literal") == 0, "new literal retained");
        Require(std::wcscmp(localize->Find("Emergency"), L"Awaryjny") == 0, "translated text beats emergency fallback");
        Require(std::wcscmp(localize->Find("AlwaysMissing"), L"Emergency Only") == 0, "missing emergency retained");
        const auto polishOnly = localize->FindIndex("PolishOnly");
        const wchar_t* oldPolish = localize->Find("LiveToken");
        WriteCatalogue(output / "resource/test_russian.txt", L"\"lang\" { \"Tokens\" { \"LiveToken\" } }");
        Require(!runtime->PrepareLanguage(fs, "russian"), "reject malformed target catalogue");
        Require(runtime->Revision() == 2 && localize->Find("LiveToken") == oldPolish,
            "failed preparation preserves active revision and pointers");
        WriteCatalogue(output / "resource/test_russian.txt", russian);
        WriteCatalogue(output / "resource/rejected_english.txt", L"\"lang\" { \"Tokens\" { \"Key\" } }");
        Require(!localize->AddFile(fs, "resource/rejected_%language%.txt"), "failed request is not registered");
        std::filesystem::remove(output / "resource/rejected_english.txt");
        WriteCatalogue(output / "resource/lazy_english.txt", L"\"lang\" { \"Tokens\" { \"Lazy\" \"Lazy Base\" } }");
        WriteCatalogue(output / "resource/lazy_polish.txt", L"\"lang\" { \"Tokens\" { \"Lazy\" \"Pozny\" } }");
        Require(localize->AddFile(fs, "resource/lazy_%language%.txt"), "register a lazy logical catalogue");
        const wchar_t* internedRussian = nullptr;
        for (std::uint64_t revision = 3; revision < 43; ++revision) {
            const bool useRussian = revision % 2 != 0;
            const char* language = useRussian ? "russian" : "english";
            Require(runtime->PrepareLanguage(fs, language), "prepare repeated language");
            vgui2::surface()->SetLanguage(language);
            runtime->CommitLanguage(revision);
            Require(localize->FindIndex("LiveToken") == index, "stable repeated token id");
            Require(localize->Find("PolishOnly") == nullptr && localize->GetValueByIndex(polishOnly) == nullptr,
                "old-language-only token hidden");
            Require(std::wcscmp(localize->Find("Lazy"), L"Lazy Base") == 0, "lazy catalogue falls back to English");
            Require(loadedScheme->GetFont("LanguageFont", false) == font, "font handle remains stable");
            const int height = vgui2::surface()->GetFontTall(font);
            Require(useRussian ? height < englishHeight : height == englishHeight,
                "language height shrinks and restores on existing HFont");
            if (useRussian) {
                if (internedRussian) {
                    Require(localize->Find("LiveToken") == internedRussian, "Russian value storage is interned");
                }
                internedRussian = localize->Find("LiveToken");
            } else {
                Require(localize->Find("LiveToken") == oldEnglish, "English value storage is interned");
            }
        }
        Require(std::wcscmp(oldEnglish, L"English") == 0 && std::wcscmp(oldPolish, L"Polski") == 0
            && std::strcmp(oldName, "LiveToken") == 0, "old raw pointers remain valid");
        localize->SetValueByIndex(index, const_cast<wchar_t*>(L"Edited Literal"));
        Require(runtime->PrepareLanguage(fs, "polish"), "prepare after editor value update");
        vgui2::surface()->SetLanguage("polish");
        runtime->BindFontReset(CountReset);
        runtime->CommitLanguage(43);
        runtime->BindFontReset(nullptr);
        Require(resetCalls == 0, "commit uses already staged fonts rather than destructive reset");
        Require(std::wcscmp(localize->Find("LiveToken"), L"Edited Literal") == 0, "editor value override retained");
        Require(localize->FindIndex("PolishOnly") == polishOnly, "returning token keeps its old index");
        Require(localize->GetNameByIndex(static_cast<vgui2::StringIndex_t>(-1)) == nullptr,
            "invalid name index is safe");
        Require(localize->GetFileNameByIndex(static_cast<vgui2::StringIndex_t>(-1)) == nullptr,
            "invalid file-name index is safe");
        localize->AddString("SaveKey", const_cast<wchar_t*>(L"Zażółć gęślą jaźń"), "resource/saved.txt");
        fs->AddSearchPath(output.c_str(), "GAMECONFIG");
        Require(localize->SaveToFile(fs, "resource/saved.txt"), "save stable-storage values as disk UTF16");
        localize->AddString("SaveKey", const_cast<wchar_t*>(L"changed"), "fixture");
        Require(localize->AddFile(fs, "resource/saved.txt"), "reload saved UTF16 catalogue");
        Require(std::wcscmp(localize->Find("SaveKey"), L"Zażółć gęślą jaźń") == 0, "saved value round trip");
        localize->AddString("FormatKey", const_cast<wchar_t*>(L"Value: %s1"), "fixture");
        wchar_t formatted[64]{};
        localize->ConstructString(formatted, sizeof(formatted), localize->Find("FormatKey"), 1, L"42");
        Require(std::wcscmp(formatted, L"Value: 42") == 0, "format stable-storage value");
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        fs->Unmount();
        privateSystem.Restore();
        std::puts("PASS actual provider: staged rollback, stable raw pointers/IDs, overrides, lazy fallback,40 switches,font metrics");
    });
}
