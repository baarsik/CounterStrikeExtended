"""Verbatim production snapshot methods, with controlled initialized-owner/provider values.
This tests absence of initialization/checkbox-derived success, not Steam authentication acceptance.
"""
import pathlib
import sys
text = pathlib.Path(sys.argv[1]).read_text()
methods = text[text.index('AdminProviderStatus CSteam3Server::QueryAdminStatus()'):text.index('extern void NotifyDedicatedServerUI')]
def function(signature):
    start = text.index(signature)
    opening = text.index('{', start)
    end = opening + 1
    depth = 1
    while depth:
        if text[end] == '{': depth += 1
        if text[end] == '}': depth -= 1
        end += 1
    return text[start:end]
methods += function('void CSteam3Server::OnLogonFailure(')
methods += function('void CSteam3Server::OnLogonSuccess(')
methods += function('void CSteam3Server::OnGSPolicyResponse(')
methods += function('bool CSteam3Server::PollRestartRequest()')
pathlib.Path(sys.argv[2]).write_text('''
#include <string>
int calls = 0;
int notifications = 0;
std::string logText;
void NotifyDedicatedServerUI(const char*) { ++notifications; }
void Con_Printf(const char* text) { logText += text; }
constexpr int k_EResultServiceUnavailable = 20;
struct SteamServerConnectFailure_t { int m_eResult; };
struct SteamServersConnected_t {};
struct GSPolicyResponse_t {};
struct ControlledProvider {
    bool connected = false;
    bool secure = false;
    bool restart = false;
    int GetSteamID() { return 1; }
    bool BLoggedOn() { ++calls; return connected; }
    bool BSecure() { ++calls; return secure; }
    bool WasRestartRequested() {
        ++calls;
        const bool requested = restart;
        restart = false;
        return requested;
    }
} sdk;
ControlledProvider* SteamGameServer() { ++calls; return &sdk; }
struct CSteam3Server {
    bool m_adminInitialized = false;
    bool m_adminRestartRequested = false;
    bool m_bLoggedOn = false;
    bool m_bWantToBeSecure = false;
    int m_adminFailure = 0;
    bool m_bLogOnResult = false;
    bool m_bLanOnly = false;
    int m_SteamIDGS = 0;
    void SendUpdatedServerDetails() {}
    void OnLogonFailure(SteamServerConnectFailure_t*);
    void OnLogonSuccess(SteamServersConnected_t*);
    void OnGSPolicyResponse(GSPolicyResponse_t*);
    AdminProviderStatus QueryAdminStatus();
    bool PollRestartRequest();
};
CSteam3Server* s_Steam3Server = nullptr;
''' + methods)
