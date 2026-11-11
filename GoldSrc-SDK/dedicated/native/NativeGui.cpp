#include "NativeGui.h"
#include "../AdminServer.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include "../../public/native_standalone_interfaces.h"
#include "../../GameUI/common/IVGuiModule.h"
#include "../../common/IAdminServer.h"
#include "../../common/IManageServer.h"
#include "../../engine/interface.h"
#include "../../vgui2/controls/ProgressBox.h"
#include "../../vgui2/controls/controls.h"
#include "../CreateMultiplayerGameServerPage.h"
#include "../sys_ded.h"
#include "vgui/Cursor.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static CMainPanel* g_pMainPanelInstance = nullptr;
static char* gpszCvars = nullptr;
vgui2::MessageMapItem_t CMainPanel::m_MessageMap[1] = {{"Quit", (vgui2::MessageFunc_t)&CMainPanel::Shutdown, 0,
                                                        vgui2::DATATYPE_VOID, NULL, vgui2::DATATYPE_VOID, NULL, 0, 0,
                                                        0}};

vgui2::PanelMap_t CMainPanel::m_PanelMap = {m_MessageMap, 1, "CMainPanel", &vgui2::Panel::m_PanelMap, 0};

#pragma warning(push)
#pragma warning(disable : 26495)
CMainPanel::CMainPanel(CreateInterfaceFn*, int) : Panel(NULL, "CMainPanel") {
    m_pManageServer = nullptr;
    m_pConfig = nullptr;
    m_hResourceWait = 0;
    m_Server = {};
    m_bStarting = false;
    m_bStarted = false;
    m_bQuit = false;
    m_bWaiting = true;
    SetPaintBackgroundEnabled(false);
    SetFgColor(Color(0, 0, 0, 0));
    m_bStarting = false;
    m_flProgress = 0.0f;
}
#pragma warning(pop)

CMainPanel::~CMainPanel() {
    vgui2::ivgui()->RemoveTickSignal(GetVPanel());
    if (m_hResourceWait) {
        vgui2::filesystem()->CancelWaitForResources(m_hResourceWait);
        m_hResourceWait = 0;
    }
    if (m_hProgress.Get()) {
        delete m_hProgress.Get();
        m_hProgress = nullptr;
    }
}

void CMainPanel::Printf(char* text) {
    if (m_pManageServer) {
        m_pManageServer->AddToConsole(text);
    }
}

vgui2::PanelMap_t* CMainPanel::GetPanelMap() {
    return &m_PanelMap;
}

CMainPanel* MainPanel() {
    return g_pMainPanelInstance;
}

void CMainPanel::Initialize(CreateInterfaceFn*, int) {
    g_pMainPanelInstance = this;
    m_pManageServer = NULL;
    m_bStarted = false;
    m_bQuit = false;
    m_hProgress = NULL;

    MoveToFront();
    m_pConfig = new CCreateMultiplayerGameServerPage(this, "Config");
}

void CMainPanel::ShowConfig() {
    m_pConfig->SetVisible(true);
    m_pConfig->MoveToFront();
}

void CMainPanel::Shutdown() {
    Stop();
}

void CMainPanel::Stop() {
    vgui2::surface()->SetCursor(vgui2::dc_user);
    m_bStarted = false;
    m_bQuit = true;
    if (m_hProgress.Get()) {
        m_hProgress->Close();
        m_hProgress = NULL;
    }
    vgui2::ivgui()->Stop();
}

void CMainPanel::StartServer(char* command) {
    vgui2::surface()->SetCursor(vgui2::dc_hourglass);
    m_pConfig->GetServerInfo(&m_Server);
    m_pConfig->SetVisible(false);

    std::free(gpszCvars);
    gpszCvars = (char*)malloc(strlen(command) + 1);
#pragma warning(suppress : 6387)
    strcpy(gpszCvars, command);
    m_hProgress = new vgui2::ProgressBox("#Start_Server_Loading_Title", "#Server_UpdatingSteamResources",
                                         "Starting dedicated server...", this);
    m_hProgress->SetCancelButtonVisible(true);
    m_hProgress->ShowWindow(NULL);

    char resourceList[MAX_PATH];
    _snprintf(resourceList, sizeof(resourceList), "reslists/%s/dedicated_preloads.lst",
              m_pConfig->m_szMod + CCreateMultiplayerGameServerPage::GAME_DIR_LENGTH);
    m_hResourceWait = vgui2::filesystem()->WaitForResources(resourceList);
    if (!m_hResourceWait) {
        m_bStarting = true;
        m_bWaiting = false;
        m_hProgress->SetCancelButtonEnabled(false);
    }

    vgui2::ivgui()->AddTickSignal(GetVPanel(), 0);
}

void CMainPanel::OnTick() {
    if (!m_hResourceWait) {
        return;
    }
    if (!m_hProgress.Get() || !m_hProgress->IsVisible()) {
        vgui2::filesystem()->CancelWaitForResources(m_hResourceWait);
        m_hResourceWait = 0;
        Stop();
        return;
    }
    float fraction = 0;
    bool complete = false;
    if (vgui2::filesystem()->GetWaitForResourcesProgress(m_hResourceWait, &fraction, &complete)) {
        m_hProgress->SetProgress(fraction);
    }
    if (complete) {
        m_hResourceWait = 0;
        m_bStarting = true;
        m_bWaiting = false;
        m_hProgress->SetCancelButtonEnabled(false);
    }
}
namespace cse::dedicated {
NativeGui::~NativeGui() {
    Close();
}
bool NativeGui::Open(DedicatedModuleSet& modules, std::string& error, std::uint64_t generation,
    const std::string& runtimeRoot) {
    if (!RemoteServer().Bind(modules.Admin(), generation)) {
        error = "Missing admitted dedicated administration provider";
        return false;
    }
    bound_ = true;
    filesystem_ = modules.Filesystem();
    const char* overridePath = std::getenv("CSE_DEDICATED_CONFIG_DIR");
    if (overridePath) {
        configPath_ = overridePath;
    } else {
        char* preferencePath = SDL_GetPrefPath("CounterStrikeExtended", "hlds");
        if (preferencePath) {
            configPath_ = preferencePath;
            SDL_free(preferencePath);
        }
    }
    std::error_code pathError;
    if (configPath_.empty() || !std::filesystem::u8path(configPath_).is_absolute()) {
        error = "Dedicated CONFIG requires an absolute writable settings directory";
        Close();
        return false;
    }
    std::filesystem::create_directories(std::filesystem::u8path(configPath_), pathError);
    if (pathError) {
        error = "Cannot create dedicated settings directory: " + pathError.message();
        Close();
        return false;
    }
    filesystem_->AddSearchPath(configPath_.c_str(), "CONFIG");
    configAttached_ = true;
    filesystem_->AddSearchPathNoWrite(runtimeRoot.c_str(), "MAIN");
    const auto platformPath = (std::filesystem::u8path(runtimeRoot) / "platform").u8string();
    filesystem_->AddSearchPathNoWrite(platformPath.c_str(), "PLATFORM");
    CreateInterfaceFn factories[] = {modules.VguiFactory(), modules.FilesystemFactory(), Sys_GetFactoryThis(),
                                     modules.EngineFactory()};
    if (!factories[0]) {
        error = "Missing admitted standalone VGUI factory";
        Close();
        return false;
    }
    if (!vgui2::VGuiControls_Init("DEDICATED", factories, 4)) {
        error = "VGUI controls initialization failed";
        Close();
        return false;
    }
    initialized_ = true;
    vgui2::system()->SetUserConfigFile("DialogConfig.vdf", "CONFIG");
    auto factory = Sys_GetFactoryThis();
    admin_ = static_cast<IAdminServer*>(factory(ADMINSERVER_INTERFACE_VERSION, nullptr));
    adminModule_ = static_cast<IVGuiModule*>(factory("VGuiModuleAdminServer001", nullptr));
    if (!admin_ || !adminModule_ || !adminModule_->Initialize(factories, 4) ||
        !adminModule_->PostInitialize(&factory, 1)) {
        error = "Dedicated admin initialization failed";
        Close();
        return false;
    }
    panel_ = new CMainPanel(factories, 4);
    vgui2::surface()->SetEmbeddedPanel(panel_->GetVPanel());
    adminModule_->SetParent(panel_->GetVPanel());
    panel_->SetVisible(true);
    if (!vgui2::scheme()->LoadSchemeFromFile("Resource/TrackerScheme.res", nullptr)) {
        error = "Cannot load retained dedicated VGUI scheme from runtime platform resources";
        Close();
        return false;
    }
    vgui2::localize()->AddFile(modules.Filesystem(), "Resource/platform_%language%.txt");
    vgui2::localize()->AddFile(modules.Filesystem(), "Resource/vgui_%language%.txt");
    vgui2::localize()->AddFile(modules.Filesystem(), "Admin/server_%language%.txt");
    vgui2::ivgui()->SetSleep(false);
    vgui2::ivgui()->Start();
    panel_->Initialize(factories, 4);
    panel_->ShowConfig();
    return true;
}
bool NativeGui::Pump() {
    if (!panel_ || panel_->m_bQuit || !vgui2::ivgui()->IsRunning()) {
        return false;
    }
    vgui2::ivgui()->RunFrame();
    return !panel_->m_bQuit;
}
bool NativeGui::ReadyToStart() const {
    return panel_ && !panel_->m_bWaiting && panel_->m_bStarting;
}
const char* NativeGui::Commands() const {
    return gpszCvars;
}
bool NativeGui::Started(std::string& error) {
    if (!panel_ || !admin_) {
        error = "Admin panel disappeared during startup";
        return false;
    }
    // Engine filesystem initialization may have rebuilt search paths.
    filesystem_->AddSearchPath(configPath_.c_str(), "CONFIG");
    serveritem_t server{};
    panel_->m_pConfig->GetServerInfo(&server);
    manageHandle_ = admin_->OpenManageServerDialog(server.name, server.gameDir);
    panel_->m_pManageServer = admin_->GetManageServerInterface(manageHandle_);
    if (!panel_->m_pManageServer) {
        error = "Cannot create dedicated administration panel";
        return false;
    }
    panel_->m_bStarting = false;
    panel_->m_bStarted = true;
    if (panel_->m_hProgress.Get()) {
        panel_->m_hProgress->Close();
        panel_->m_hProgress = nullptr;
    }
    return true;
}
void NativeGui::Stop() noexcept {
    if (bound_) {
        RemoteServer().Shutdown();
        bound_ = false;
    }
    if (panel_) {
        vgui2::ivgui()->RemoveTickSignal(panel_->GetVPanel());
        if (panel_->m_hResourceWait) {
            vgui2::filesystem()->CancelWaitForResources(panel_->m_hResourceWait);
            panel_->m_hResourceWait = 0;
        }
    }
    if (adminModule_) {
        adminModule_->Deactivate();
    }
}
void NativeGui::Close() noexcept {
    Stop();
    if (filesystem_ && configAttached_) {
        // Engine shutdown may clear its search paths; panels still persist their user configuration below.
        filesystem_->AddSearchPath(configPath_.c_str(), "CONFIG");
    }
    if (adminModule_) {
        adminModule_->Shutdown();
        adminModule_ = nullptr;
    }
    if (panel_) {
        vgui2::surface()->SetEmbeddedPanel(0);
        delete panel_;
        panel_ = nullptr;
        g_pMainPanelInstance = nullptr;
    }
    admin_ = nullptr;
    manageHandle_ = 0;
    if (initialized_) {
        vgui2::ivgui()->RunFrame();
        vgui2::system()->SaveUserConfigFile();
        vgui2::ivgui()->Shutdown();
        vgui2::system()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        initialized_ = false;
    }
    if (filesystem_ && configAttached_) {
        filesystem_->RemoveSearchPath(configPath_.c_str());
    }
    filesystem_ = nullptr;
    configPath_.clear();
    configAttached_ = false;
    std::free(gpszCvars);
    gpszCvars = nullptr;
}
void NativeGui::Print(char* text) {
    // A close action may have deleted the frame before engine shutdown emits its final log.
    auto* receiver = admin_ ? admin_->GetManageServerInterface(manageHandle_) : nullptr;
    if (receiver) {
        receiver->AddToConsole(text);
    } else if (text) {
        std::fputs(text, stderr);
    }
}
} // namespace cse::dedicated
