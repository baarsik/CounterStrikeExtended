#include "engine/registry.h"
#include "engine/utlvector.h"
#include "public/native_restart_command_line.h"
#include "engine/native_video_restart.h"
#include <cassert>
#include <climits>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// Only unrelated GUI/server operations are substituted. Menu command generation,
// command handlers, native video setters and disk registry are extracted verbatim.
namespace menu_fixture {
#define Q_strncpy std::strncpy
#include "menu_registry.inc"
#undef Q_strncpy
#include "menu_video.inc"

constexpr int ca_dedicated = 1;
constexpr int ca_active = 2;
constexpr int DLL_QUIT = 3;
constexpr int FALSE = 0;
struct ClientState {
    int state = 0;
    char trueaddress[sizeof("loopback")]{};
    char servername[sizeof("localhost:27015")]{};
} cls;
struct ServerState {
    bool active = false;
} sv;
struct ServerStatic {
    int maxclients = 0;
} svs;
int giActive;
int giStateInfo;
char* g_pPostRestartCmdLineArgs;
std::vector<std::string> tokens;
int Cmd_Argc() {
    return static_cast<int>(tokens.size());
}
const char* Cmd_Argv(int index) {
    return tokens.at(index).c_str();
}
int Q_strcasecmp(const char* left, const char* right) {
    return strcasecmp(left, right);
}
int Q_atoi(const char* text) {
    return std::atoi(text);
}
void Cbuf_AddText(const char*) {}
void Cbuf_Execute() {}
bool Host_IsServerActive() {
    return false;
}
void Host_ShutdownServer(int) {}
void CL_Disconnect() {}
void NET_Config(int) {}
void Cache_Force_Flush() {}
void Sys_Error(const char*, ...) {
    throw std::runtime_error("restart buffer overflow");
}
#if defined(CSE_MENU_WIN32_BEHAVIOR)
#undef __APPLE__
#endif
#include "menu_host.inc"
#if defined(CSE_MENU_WIN32_BEHAVIOR)
#define __APPLE__ 1
#endif

void ClientCommand(char* text) {
    tokens = cse_restart::GeneratedArguments(text);
    if (tokens[0] == "_setvideomode") {
        Host_SetVideoMode_f();
    } else if (tokens[0] == "_setrenderer") {
        Host_SetRenderer_f();
    } else if (tokens[0] == "_restart") {
        Host_Quit_Restart_f();
    } else {
        throw std::runtime_error("unexpected menu command");
    }
}
struct EngineFunctions {
    void (*pfnClientCmd)(char*) = ClientCommand;
} engineFunctions;
EngineFunctions* engine = &engineFunctions;
struct Control {
    const char* text = "";
    bool selected = false;
    void GetText(char* output, std::size_t capacity) {
        std::snprintf(output, capacity, "%s", text);
    }
    bool IsSelected() {
        return selected;
    }
    void ApplyChanges() {}
};
class COptionsSubVideo {
public:
    struct Settings {
        int w = 800;
        int h = 600;
        int bpp = 16;
        bool windowed = true;
        char renderer[16] = "software";
    };
    Settings m_CurrentSettings;
    Settings m_OrigSettings;
    Control* m_pMode = nullptr;
    Control* m_pRenderer = nullptr;
    Control* m_pWindowed = nullptr;
    Control* m_pVsync = nullptr;
    const char* m_pszRenderNames[2] = {"Software", "OpenGL"};
    void ApplyVidSettings(bool forceRefresh);
};
#define stricmp strcasecmp
#include "menu_gameui.inc"
#undef stricmp
}

void FixtureApplyVideoSettings(char* restart, const char* renderer) {
    using namespace menu_fixture;
    menu_fixture::registry->Init();
    g_pPostRestartCmdLineArgs = restart;
    const char* requestedResolution = std::getenv("CSE_BOOTSTRAP_MENU_RESOLUTION");
    Control mode{requestedResolution ? requestedResolution : "1024 x 768", false};
    Control selectedRenderer{renderer, false};
    Control window{"", false};
    COptionsSubVideo menu;
    menu.m_pMode = &mode;
    menu.m_pRenderer = &selectedRenderer;
    menu.m_pWindowed = &window;
    if (std::strcmp(renderer, "@size-only") == 0) {
        ClientCommand(const_cast<char*>("_setvideomode 1024 768 16"));
        ClientCommand(const_cast<char*>("_restart"));
        return;
    }
    if (std::strcmp(renderer, "@renderer-only") == 0) {
        ClientCommand(const_cast<char*>("_setrenderer gl fullscreen"));
        ClientCommand(const_cast<char*>("_restart"));
        return;
    }
    menu.ApplyVidSettings(false);
    assert(menu_fixture::registry->ReadInt("ScreenWindowed", -1) == 0);
    assert(menu_fixture::registry->ReadInt("DisplayWidth", -1) == menu.m_CurrentSettings.w);
    assert(menu_fixture::registry->ReadInt("DisplayHeight", -1) == menu.m_CurrentSettings.h);
    menu_fixture::registry->Shutdown();
}

#if defined(CSE_MENU_CHECKS)
int main() {
    using namespace menu_fixture;
    char output[cse_restart::GeneratedCommandCapacity]{};
    g_pPostRestartCmdLineArgs = output;
    // Repeated updates retain the final requested value, then drain exactly once.
    nativeVideoRestart.SetRenderer(0, true);
    nativeVideoRestart.SetRenderer(1, false);
    nativeVideoRestart.SetSize(640, 480);
    nativeVideoRestart.SetSize(1024, 768);
    Host_Quit_Restart_f();
#if defined(CSE_MENU_WIN32_BEHAVIOR)
    assert(output[0] == '\0');
    std::puts("retained non-Apple host command branch: no native restart flags");
#else
    assert(std::strcmp(output, " -gl -fullscreen -w 1024 -h 768") == 0);
    output[0] = '\0';
    Host_Quit_Restart_f();
    assert(output[0] == '\0');
    // Both actual Host_Init and Host_Shutdown invoke this exact reset body;
    // extraction also checks placement before their session-specific operations.
    nativeVideoRestart.SetRenderer(0, false);
    nativeVideoRestart.SetSize(1920, 1080);
    Host_ResetNativeVideoRestart();
    Host_Quit_Restart_f();
    assert(output[0] == '\0');
    // A renderer-only request must not synthesize dimensions, and vice versa.
    nativeVideoRestart.SetRenderer(0, false);
    Host_Quit_Restart_f();
    assert(std::strcmp(output, " -soft -fullscreen") == 0);
    output[0] = '\0';
    nativeVideoRestart.SetSize(INT_MIN, INT_MAX);
    Host_Quit_Restart_f();
    assert(std::strcmp(output, " -w -2147483648 -h 2147483647") == 0);
    // Existing reconnect/load suffixes and video updates share the same bounded
    // append, preserving the actual host's network/single-player restart signal.
    output[0] = '\0';
    cls.state = ca_active;
    std::strcpy(cls.trueaddress, "loopback");
    std::strcpy(cls.servername, "localhost:27015");
    nativeVideoRestart.SetRenderer(0, false);
    Host_Quit_Restart_f();
    assert(std::strcmp(output, " +connect localhost:27015 -soft -fullscreen") == 0);
    output[0] = '\0';
    sv.active = true;
    svs.maxclients = 1;
    Host_Quit_Restart_f();
    assert(std::strcmp(output, " +load quick") == 0);
    cls.state = 0;
    sv.active = false;
    // The shared caller capacity is exact, including NUL. Rejection must not
    // partially overwrite the command, and a retry keeps the pending settings.
    nativeVideoRestart.SetRenderer(0, false);
    const std::string addition = " -soft -fullscreen";
    std::fill(std::begin(output), std::end(output), 'x');
    output[sizeof(output) - addition.size()] = '\0';
    const std::string before = output;
    bool rejected = false;
    try {
        Host_Quit_Restart_f();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected && before == output);
    output[sizeof(output) - addition.size() - 1] = '\0';
    Host_Quit_Restart_f();
    assert(std::strlen(output) == sizeof(output) - 1);
    assert(std::strcmp(output + sizeof(output) - addition.size() - 1, addition.c_str()) == 0);
    nativeVideoRestart.SetSize(1, 1);
    std::fill(std::begin(output), std::end(output), 'x');
    assert(!nativeVideoRestart.Append(output, sizeof(output)));
    assert(std::all_of(std::begin(output), std::end(output), [](char c) { return c == 'x'; }));
    assert(!nativeVideoRestart.Append(nullptr, sizeof(output)));
    assert(!nativeVideoRestart.Append(output, 0));
    Host_ResetNativeVideoRestart();
    std::puts("native host restart: replacement, drain, lifecycle reset, independent groups, exact bounds passed");
#endif
    return 0;
}
#endif
