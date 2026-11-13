#include "FileSystem.h"
#include "interface.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <dlfcn.h>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

}

int main(int argc, char** argv) {
    constexpr int RequiredArguments = 3;
    Require(argc == RequiredArguments, "usage: loader_tests runtime-root translation-root");
    const std::string runtime = argv[1];
    const std::string resources = argv[2];
    void* filesystemImage = dlopen((runtime + "/filesystem_stdio.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* guiImage = dlopen((runtime + "/vgui2.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    Require(filesystemImage && guiImage, "load actual runtime filesystem and VGUI libraries");
    auto filesystemFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(filesystemImage, "CreateInterface"));
    auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(guiImage, "CreateInterface"));
    Require(filesystemFactory && guiFactory, "runtime interface factories");
    auto* filesystem = static_cast<IFileSystem*>(filesystemFactory("VFileSystem009", nullptr));
    auto* gui = static_cast<vgui2::IVGui*>(guiFactory("VGUI_ivgui006", nullptr));
    auto* localize = static_cast<vgui2::ILocalize*>(guiFactory("VGUI_Localize003", nullptr));
    auto* surface = static_cast<vgui2::ISurface*>(guiFactory("VGUI_Surface026", nullptr));
    Require(filesystem && gui && localize && surface, "actual localization service and dependencies");
    filesystem->Mount();
    filesystem->AddSearchPathNoWrite((resources + "/platform").c_str(), "PLATFORM");
    filesystem->AddSearchPathNoWrite((resources + "/cstrike").c_str(), "GAME");
    filesystem->AddSearchPathNoWrite((resources + "/valve").c_str(), "DEFAULTGAME");
    CreateInterfaceFn factories[] = {guiFactory, filesystemFactory};
    constexpr int FactoryCount = sizeof(factories) / sizeof(factories[0]);
    Require(gui->Init(factories, FactoryCount), "initialize actual VGUI service");

    const struct {
        const char* language;
        const wchar_t* serverName;
    } cases[] = {
        {"english", L"Server name"},
        {"french", L"Nom du serveur"},
        {"german", L"Servername"},
        {"italian", L"Nome del server"},
        {"spanish", L"Nombre del servidor"},
        {"schinese", L"服务器名称"},
        {"tchinese", L"伺服器名稱"},
        {"koreana", L"서버 이름"},
        {"korean", L"서버 이름"},
        {"russian", L"Имя сервера"},
        {"polish", L"Nazwa serwera"}
    };
    for (const auto& test : cases) {
        localize->RemoveAll();
        surface->SetLanguage(test.language);
        Require(localize->AddFile(filesystem, "resource/cse_%language%.txt"), "load bundled CSE translations");
        const wchar_t* actual = localize->Find("CSEServerBrowser_Name");
        Require(actual && std::wcscmp(actual, test.serverName) == 0, "selected-language CSE text survives real loader");
        for (const char* path : {"resource/valve_%language%.txt", "resource/gameui_%language%.txt",
                 "resource/cstrike_%language%.txt", "resource/platform_%language%.txt",
                 "resource/vgui_%language%.txt", "servers/serverbrowser_%language%.txt",
                 "Admin/admin_%language%.txt", "Admin/server_%language%.txt"}) {
            Require(localize->AddFile(filesystem, path), "load complete stock translation family");
        }
        Require(localize->Find("Console_Submit") != nullptr, "CSE administration addition resolves");
    }
    localize->RemoveAll();
    surface->SetLanguage("english");
    Require(localize->AddFile(filesystem, "resource/valve_%language%.txt"), "load English stock fallback");
    const wchar_t* english = localize->Find("Valve_Movement_Title");
    Require(english != nullptr, "English stock token exists");
    const std::wstring saved = english;
    surface->SetLanguage("unavailable-language");
    Require(!localize->AddFile(filesystem, "resource/valve_%language%.txt"), "missing selected-language file is reported");
    Require(std::wcscmp(localize->Find("Valve_Movement_Title"), saved.c_str()) == 0,
        "English fallback survives a missing selected-language file");
    gui->Shutdown();
    filesystem->Unmount();
    std::puts("PASS actual VGUI loader: eight languages, Korean alias, stock families and English fallback");
    return EXIT_SUCCESS;
}
