#include "dedicated/AdminServer.h"
#include "engine/controls.h"
#include "public/FileSystem.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui2/controls/Button.h"
#include "vgui2/controls/ComboBox.h"
#include "vgui2/controls/ListPanel.h"
#include "vgui2/controls/TextEntry.h"
#include "vgui2/controls/controls.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <string>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #expr, SDL_GetError());                  \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
extern IGameServerData* ActualProvider();
extern void PopulateProviderFixture();

static void PumpMessages() {
    constexpr int MessageFrames = 10;
    for (int frame = 0; frame < MessageFrames; ++frame) {
        vgui2::ivgui()->RunFrame();
    }
}

static bool HasFile(vgui2::ListPanel* list, const char* name) {
    for (int row = 0; row < list->GetItemCount(); ++row) {
        if (std::string(list->GetItem(list->GetItemIDFromRow(row))->GetString("text")) == name) {
            return true;
        }
    }
    return false;
}

static void CheckBanImportDirectoryChanges(CGamePanelInfo* panel, const std::string& config) {
    namespace fs = std::filesystem;
    constexpr int PathBytes = 1024;
    const fs::path directory = fs::path(config) / "ban-import zażółć";
    fs::create_directories(directory / "empty");
    std::ofstream(directory / "cse-ban-refresh-fixture.cfg") << "banid 0 STEAM_0:1:123\n";
    std::ofstream(directory / "ignored.txt") << "ignore\n";

    auto* bans = panel->FindChildByName("Ban List", true);
    CHECK(bans);
    vgui2::ivgui()->PostMessage(bans->GetVPanel(), new KeyValues("importban"), panel->GetVPanel(), 0.0f);
    PumpMessages();
    auto* dialog = bans->FindChildByName("FileOpenDialog", true);
    CHECK(dialog);
    auto* path = dynamic_cast<vgui2::ComboBox*>(dialog->FindChildByName("FullPathEdit", false));
    auto* list = dynamic_cast<vgui2::ListPanel*>(dialog->FindChildByName("FileList", false));
    auto* filename = dynamic_cast<vgui2::TextEntry*>(dialog->FindChildByName("FileNameEdit", false));
    auto* open = dynamic_cast<vgui2::Button*>(dialog->FindChildByName("OpenButton", false));
    CHECK(path && list && filename && open);

    const int directoryItem = path->AddItem(directory.u8string().c_str(), nullptr);
    filename->SetText("stale.cfg");
    path->ActivateItem(directoryItem);
    PumpMessages();
    CHECK(HasFile(list, "cse-ban-refresh-fixture.cfg"));
    CHECK(HasFile(list, "empty"));
    CHECK(!HasFile(list, "ignored.txt"));
    char selectedName[PathBytes];
    filename->GetText(selectedName, sizeof(selectedName));
    CHECK(selectedName[0] == '\0');
    CHECK(dialog->IsVisible() && open->IsEnabled());

    path->ActivateItemByRow(0); // The native roots list starts with /.
    PumpMessages();
    char selectedPath[PathBytes];
    path->GetText(selectedPath, sizeof(selectedPath));
    CHECK(std::string(selectedPath) == "/");
    CHECK(HasFile(list, "tmp"));
    CHECK(!HasFile(list, "cse-ban-refresh-fixture.cfg"));

    const int emptyItem = path->AddItem((directory / "empty").u8string().c_str(), nullptr);
    path->ActivateItem(emptyItem);
    PumpMessages();
    CHECK(list->GetItemCount() == 0);
    CHECK(dialog->IsVisible() && open->IsEnabled());

    const int missingItem = path->AddItem((directory / "missing").u8string().c_str(), nullptr);
    path->ActivateItem(missingItem);
    PumpMessages();
    CHECK(list->GetItemCount() == 0);
    CHECK(!open->IsEnabled());
}

int main(int argc, char** argv) {
    CHECK(argc == 4);
    const std::string runtime = argv[1];
    const std::string assets = argv[2];
    const std::string config = argv[3];
    CHECK(std::getenv("SDL_VIDEODRIVER") && std::string(std::getenv("SDL_VIDEODRIVER")) == "dummy");
    std::filesystem::create_directories(config);
    void* filesystemImage = dlopen((runtime + "/filesystem_stdio.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* guiImage = dlopen((runtime + "/vgui2.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    CHECK(filesystemImage && guiImage);
    auto filesystemFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(filesystemImage, "CreateInterface"));
    auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(guiImage, "CreateInterface"));
    auto* filesystem = static_cast<IFileSystem*>(filesystemFactory("VFileSystem009", nullptr));
    filesystem->Mount();
    filesystem->AddSearchPathNoWrite(assets.c_str(), "PLATFORM");
    filesystem->AddSearchPath(config.c_str(), "CONFIG");
    PopulateProviderFixture();
    CreateInterfaceFn factories[] = {guiFactory, filesystemFactory};
    for (std::uint64_t generation = 1; generation <= 3; ++generation) {
        CHECK(vgui2::VGuiControls_Init("ADMIN_FIXTURE", factories, 2));
        auto* gui = vgui2::ivgui();
        gui->SetSleep(false);
        gui->Start();
        CHECK(vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", nullptr));
        vgui2::localize()->AddFile(filesystem, "resource/platform_english.txt");
        vgui2::localize()->AddFile(filesystem, "resource/vgui_english.txt");
        vgui2::localize()->AddFile(filesystem, "Admin/server_english.txt");
        vgui2::system()->SetUserConfigFile("DialogConfig.vdf", "CONFIG");
        CHECK(RemoteServer().Bind(ActualProvider(), generation));
        CAdminServer admin;
        CHECK(admin.Initialize(factories, 2));
        CHECK(admin.IsValid());
        CHECK(admin.GetManageServerInterface(static_cast<ManageServerUIHandle_t>(-1)) == nullptr);
        const auto handle = admin.OpenManageServerDialog("Controlled native admin", "valve");
        auto* managed = admin.GetManageServerInterface(handle);
        CHECK(managed);
        auto* panel = dynamic_cast<CGamePanelInfo*>(managed);
        CHECK(panel);
        managed->AddToConsole("Controlled actual admin log record\n");
        for (int frame = 0; frame < 20; ++frame) {
            gui->RunFrame();
            SDL_Delay(10);
        }
        CHECK(vgui2::surface()->GetPopupCount() > 0);
        auto* info = panel->FindChildByName("ServerInfo", true);
        CHECK(info);
        auto* rules = dynamic_cast<vgui2::ListPanel*>(info->FindChildByName("RulesList", true));
        CHECK(rules && rules->GetItemCount() >= 6);
        CheckBanImportDirectoryChanges(panel, config);
        admin.Shutdown();
        CHECK(!admin.IsValid());
        CHECK(admin.GetManageServerInterface(handle) == nullptr);
        admin.Shutdown();
        gui->RunFrame();
        for (int index = 0; index < vgui2::surface()->GetPopupCount(); ++index) {
            const auto popup = vgui2::surface()->GetPopup(index);
            std::fprintf(stderr, "Retained popup %s parent=%p\n", vgui2::ipanel()->GetName(popup),
                         reinterpret_cast<void*>(vgui2::ipanel()->GetParent(popup)));
        }
        CHECK(vgui2::surface()->GetPopupCount() == 0);
        vgui2::system()->SaveUserConfigFile();
        gui->Shutdown();
        vgui2::system()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
    }
    CHECK(std::filesystem::exists(std::filesystem::path(config) / "DialogConfig.vdf"));
    filesystem->Unmount();
    dlclose(guiImage);
    dlclose(filesystemImage);
    std::puts("PASS actual retained AdminServer frames: three close/reopen generations with dummy SDL");
}
