#include "dedicated/AdminServer.h"
#include "engine/controls.h"
#include "public/FileSystem.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui2/controls/ListPanel.h"
#include "vgui2/controls/controls.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
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
