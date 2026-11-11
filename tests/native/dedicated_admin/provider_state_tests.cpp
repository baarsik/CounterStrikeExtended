#include "engine/admin_provider_status.h"
#include "provider_state_actual.inc"
#include <cstdio>
#include <cstdlib>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                       \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
int main() {
    auto state = Steam_QueryAdminStatus();
    CHECK(!state.available && calls == 0);
    CSteam3Server owner;
    s_Steam3Server = &owner;
    state = Steam_QueryAdminStatus();
    CHECK(!state.available && calls == 0);
    owner.m_bLoggedOn = true;
    owner.m_adminInitialized = true;
    owner.m_bWantToBeSecure = true;
    state = Steam_QueryAdminStatus();
    CHECK(state.available && state.secureRequested && !state.connected && !state.secure);
    sdk.connected = true;
    sdk.secure = true;
    sdk.restart = true;
    owner.m_adminFailure = 42;
    state = Steam_QueryAdminStatus();
    CHECK(state.connected && state.secure && state.restartRequested && state.failure == 42);
    SteamServerConnectFailure_t failure{k_EResultServiceUnavailable};
    owner.OnLogonFailure(&failure);
    CHECK(Steam_QueryAdminStatus().failure == k_EResultServiceUnavailable);
    CHECK(logText.find("successful") == std::string::npos);
    owner.OnLogonSuccess(nullptr);
    CHECK(Steam_QueryAdminStatus().failure == 0);
    CHECK(notifications == 2);
    owner.m_adminInitialized = false;
    owner.OnLogonFailure(&failure);
    CHECK(notifications == 2);
    owner.m_bLoggedOn = false;
    const int before = calls;
    state = Steam_QueryAdminStatus();
    CHECK(!state.available && !state.secure && !state.failure && calls == before);
}
