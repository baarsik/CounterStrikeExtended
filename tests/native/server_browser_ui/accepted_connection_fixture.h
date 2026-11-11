// Actual accepted owner bodies with controlled unrelated career/loading/UI leaves.
namespace accepted_connection_fixture {
namespace cse = ::cse;
struct Career {
    bool IsPlayingMatch() {
        return false;
    }
    void FinishMatchStart() {}
};
Career *TheCareerGame = nullptr;
struct BaseUI {
    void ActivateGameUI() {}
    void HideGameUI() {}
} baseUI;
BaseUI *baseuifuncs = &baseUI;
struct CBasePanel {
    enum { BACKGROUND_CAREERLOAD };
    void SetBackgroundRenderState(int) {}
} panel;
CBasePanel *staticPanel = &panel;
struct Loading {
    void SwitchToPausedCareerDialog() {}
} loading;
Loading *g_hLoadingDialog = &loading;
struct Loader {
    void PostMessageToAllModules(KeyValues *message) {
        message->deleteThis();
    }
} g_VModuleLoader;
int stricmp(const char *left, const char *right) {
    return std::strcmp(left, right);
}
class CGameUI {
public:
    void ConnectToServer(const char *, int, int);
    void DisconnectFromServer();
    void StartProgressBar(const char *, int) {}
    void ContinueProgressBar(int, float) {}
    void SetProgressBarStatusText(const char *) {}
    void ActivateGameUI() {}
    int m_iGameIP = 0;
    int m_iGamePort = 0;
};
#include "accepted_connection.inc"
}
