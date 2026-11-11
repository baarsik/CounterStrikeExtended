// Actual setup controls, NativeGui and DedicatedSession; only engine image boundary is controlled.
#include "common/icommandline.h"
#include "common/netapi.h"
#include "dedicated/CreateMultiplayerGameServerPage.h"
#include "dedicated/native/DedicatedSession.h"
#include "dedicated/sys_ded.h"
#include "engine/controls.h"
#include "vgui/IVGui.h"
#include "vgui2/controls/CheckButton.h"
#include "vgui2/controls/ComboBox.h"
#include "vgui2/controls/TextEntry.h"
#include "vgui2/controls/controls.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expr);                                        \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
extern IGameServerData* ActualProvider();
extern void PopulateProviderFixture();
namespace {
CreateInterfaceFn filesFactory;
CreateInterfaceFn guiFactory;
std::string capturedCommand;
std::string capturedText;
int initCalls = 0;
bool failInit = false;
int shutdownResult = 0;
void* ControlledEngineFactory(const char*, int*) {
    return nullptr;
}
class ControlledNet final : public INetAPI {
  public:
    void NetAdrToSockAddr(netadr_t*, sockaddr*) override {
    }
    void SockAddrToNetAdr(sockaddr*, netadr_t*) override {
    }
    char* AdrToString(netadr_t*) override {
        return nullptr;
    }
    bool StringToAdr(const char*, netadr_t*) override {
        return false;
    }
    void GetSocketAddress(int, netadr_t*) override {
    }
    bool CompareAdr(netadr_t*, netadr_t*) override {
        return false;
    }
    void GetLocalIP(netadr_t* address) override {
        *address = {};
        address->ip[0] = 127;
        address->ip[3] = 1;
    }
} controlledNet;
} // namespace
INetAPI* net = &controlledNet;
namespace cse::dedicated {
DedicatedModuleSet::~DedicatedModuleSet() noexcept {
    Close();
}
bool DedicatedModuleSet::Open(const char*, bool gui, std::string&) {
    CHECK(gui);
    filesystemFactory_ = filesFactory;
    vguiFactory_ = guiFactory;
    engineFactory_ = ControlledEngineFactory;
    filesystem_ = static_cast<IFileSystem*>(filesFactory("VFileSystem009", nullptr));
    admin_ = ActualProvider();
    return filesystem_ && admin_;
}
void DedicatedModuleSet::Close() noexcept {
    filesystem_ = nullptr;
    admin_ = nullptr;
}
bool DedicatedModuleSet::Initialize(char*, char* command, CreateInterfaceFn) {
    ++initCalls;
    capturedCommand = command;
    return !failInit;
}
bool DedicatedModuleSet::Frame() {
    return false;
}
void DedicatedModuleSet::AddText(char* text) {
    capturedText += text;
}
void DedicatedModuleSet::Status(float*, int*, int*, char*) {
}
int DedicatedModuleSet::Shutdown() noexcept {
    return shutdownResult;
}
bool DedicatedModuleSet::Retire() noexcept {
    return true;
}
} // namespace cse::dedicated
static CCreateMultiplayerGameServerPage* Configure(bool secure = true) {
    auto* page = dynamic_cast<CCreateMultiplayerGameServerPage*>(MainPanel()->FindChildByName("Config", true));
    CHECK(page);
    auto* mods = dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("MODCombo", true));
    CHECK(mods && mods->GetItemCount() == 2);
    mods->ActivateItemByRow(1);
    for (int i = 0; i < 4; ++i) {
        vgui2::ivgui()->RunFrame();
    }
    auto* maps = dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("MapList", true));
    CHECK(maps && maps->GetItemCount() == 1);
    maps->ActivateItemByRow(0);
    dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("NumPlayers", true))->ActivateItemByRow(11);
    dynamic_cast<vgui2::ComboBox*>(page->FindChildByName("NetworkCombo", true))->ActivateItemByRow(1);
    dynamic_cast<vgui2::CheckButton*>(page->FindChildByName("SecureCheck", true))->SetSelected(secure);
    page->SetControlString("ServerNameEdit", "Accepted UTF-8 żółć server");
    page->SetControlString("RCONPasswordEdit", "Abc123");
    page->SetControlInt("PortEdit", 28015);
    CHECK(dynamic_cast<vgui2::TextEntry*>(page->FindChildByName("ServerNameEdit", true)));
    CHECK(dynamic_cast<vgui2::TextEntry*>(page->FindChildByName("RCONPasswordEdit", true)));
    for (int i = 0; i < 4; ++i) {
        vgui2::ivgui()->RunFrame();
    }
    return page;
}
int main(int argc, char** argv) {
    CHECK(argc == 4);
    const std::string runtime = argv[1];
    const std::filesystem::path assets = argv[2];
    const std::filesystem::path root = std::filesystem::canonical(argv[0]).parent_path();
    const std::string mode = argv[3];
    const auto config = root / ("setup-config-" + mode);
    CHECK(std::string(std::getenv("SDL_VIDEODRIVER")) == "dummy");
    std::filesystem::create_directories(config);
    setenv("CSE_DEDICATED_CONFIG_DIR", config.c_str(), 1);
    if (!std::filesystem::exists(root / "platform")) {
        std::filesystem::create_directory_symlink(assets, root / "platform");
    }
    for (const auto* game : {"valve", "cstrike"}) {
        std::filesystem::create_directories(root / game / "maps");
        std::ofstream(root / game / "liblist.gam")
            << "game \"" << (std::string(game) == "valve" ? "Half-Life" : "Counter-Strike")
            << "\"\ntype \"multiplayer_only\"\n";
        std::ofstream(root / game / "maps" / (std::string(game) == "valve" ? "crossfire.bsp" : "de_fixture.bsp"));
    }
    void* files = dlopen((runtime + "/filesystem_stdio.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* gui = dlopen((runtime + "/vgui2.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    CHECK(files && gui);
    filesFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(files, "CreateInterface"));
    guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(gui, "CreateInterface"));
    PopulateProviderFixture();
    const char* initial[] = {"fixture name", "-game",       "valve", "+map",    "old", "-port",
                             "1111",         "-maxplayers", "2",     "+sv_lan", "0",   "-insecure"};
    std::string error;
    cse::dedicated::DedicatedSession session;
    if (mode == "config") {
        CHECK(session.Open(12, initial, error));
        session.Close();
        CHECK(std::filesystem::exists(config / "DialogConfig.vdf"));
        CHECK(chmod(config.c_str(), 0500) == 0);
        const bool opened = session.Open(12, initial, error);
        if (opened) {
            session.Close();
        }
        CHECK(chmod(config.c_str(), 0700) == 0);
        CHECK(!opened && !error.empty());
        CHECK(initCalls == 0);
        for (const auto& entry : std::filesystem::directory_iterator(config)) {
            CHECK(entry.path().filename().string().find(".cse-config-write-") != 0);
        }
    } else if (mode == "text") {
        for (const auto* field : {"ServerNameEdit", "RCONPasswordEdit"}) {
            for (const std::string value : {std::string(63, 'a'), std::string(64, 'a'), std::string(63, 'a') + "\"",
                                            std::string(61, 'a') + "ż", std::string(62, 'a') + "ż"}) {
                CHECK(session.Open(12, initial, error));
                auto* page = Configure();
                page->SetControlString(field, value.c_str());
                for (int i = 0; i < 4; ++i) {
                    vgui2::ivgui()->RunFrame();
                }
                const std::string before = cmdline->GetCmdLine();
                const int callsBefore = initCalls;
                const auto settingsPath = config / "Server.vdf";
                std::ifstream previousSettings(settingsPath);
                const std::string settingsBefore((std::istreambuf_iterator<char>(previousSettings)), {});
                page->OnCommand("start");
                if (value.size() > 63 ||
                    (std::string(field) == "RCONPasswordEdit" && value.find("ż") != std::string::npos)) {
                    CHECK(page->IsVisible());
                    CHECK(std::string(cmdline->GetCmdLine()) == before);
                    CHECK(initCalls == callsBefore);
                    std::ifstream currentSettings(settingsPath);
                    CHECK(std::string((std::istreambuf_iterator<char>(currentSettings)), {}) == settingsBefore);
                    session.Close();
                } else {
                    CHECK(!page->IsVisible());
                    CHECK(session.Run() == 0);
                    CHECK(capturedText.find(value) != std::string::npos);
                }
            }
        }
    } else if (mode == "bounds") {
        const std::string prefix = "\"fixture name\" -padding " + std::string(450, 'x') + " -another ";
        const std::string suffix = " -game cstrike -maxplayers 12 +sv_lan 1 +map de_fixture -port 28015";
        for (const std::size_t finalBytes : {1023, 1024}) {
            const std::string padding(finalBytes - prefix.size() - suffix.size(), 'y');
            const std::string firstPadding(450, 'x');
            const char* arguments[] = {"fixture name", "-padding", firstPadding.c_str(), "-another", padding.c_str()};
            CHECK(session.Open(5, arguments, error));
            auto* page = Configure();
            page->OnCommand("start");
            const int callsBefore = initCalls;
            const std::string original = session.CommandLine().Encoded();
            const int result = session.Run();
            if (finalBytes == 1023) {
                CHECK(result == 0 && initCalls == callsBefore + 1);
                CHECK(capturedCommand == prefix + padding + suffix && capturedCommand.size() == 1023);
            } else {
                CHECK(result != 0 && initCalls == callsBefore);
                CHECK(session.CommandLine().Encoded() == original);
            }
        }
        for (const int argumentCount : {39, 40}) {
            std::vector<const char*> arguments(static_cast<std::size_t>(argumentCount), "value");
            arguments[0] = "hlds";
            CHECK(session.Open(argumentCount, arguments.data(), error));
            auto* page = Configure();
            page->OnCommand("start");
            const int callsBefore = initCalls;
            const int result = session.Run();
            CHECK((result == 0) == (argumentCount == 39));
            CHECK(initCalls == callsBefore + (argumentCount == 39 ? 1 : 0));
            if (argumentCount == 39) {
                CHECK(session.CommandLine().Arguments().size() == 49);
            }
        }
    } else {
        CHECK(session.Open(12, initial, error));
        auto* page = Configure();
        page->OnCommand("start");
        CHECK(!page->IsVisible());
        shutdownResult = 5;
        CHECK(session.Run() == 0);
        CHECK(capturedCommand == "\"fixture name\" -game cstrike -maxplayers 12 +sv_lan 1 +map de_fixture -port 28015");
        CHECK(session.CommandLine().Encoded() == capturedCommand);
        CHECK(session.RestartRequested());
        CHECK(std::filesystem::path(session.Executable()).is_absolute());
        CHECK(session.Open(12, initial, error));
        page = Configure(false);
        page->OnCommand("start");
        failInit = true;
        CHECK(session.Run() != 0);
        CHECK(capturedCommand.find(" -insecure") != std::string::npos);
        failInit = false;
        CHECK(session.Open(12, initial, error));
        page = Configure(true);
        page->OnCommand("start");
        CHECK(session.Run() == 0);
        CHECK(capturedCommand.find("-insecure") == std::string::npos);
    }
    dlclose(gui);
    dlclose(files);
    std::puts("PASS actual setup/session boundary");
}
