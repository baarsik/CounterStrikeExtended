#include "steam_server_transport.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace {
void* client;
int factoryCalls;
bool missingClient;
constexpr int Creates = 0;
constexpr int Shutdowns = 2;
constexpr int LivePipes = 3;
void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "transport_fixture_failure=%s\n", message);
        std::exit(1);
    }
}
void* S_CALLTYPE Factory(const char* name) {
    ++factoryCalls;
    Require(std::strcmp(name, "SteamClient020") == 0, "exact SDK core interface version");
    return missingClient ? nullptr : client;
}
}
int main(int argc, char** argv) {
    Require(argc == 3, "two controlled provider paths");
    void* complete = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
    void* incomplete = cse_platform::LoadModule(argv[2], cse_platform::ModuleFallback::None);
    Require(complete && incomplete, "controlled provider load");
    const auto getClient = reinterpret_cast<void* (*)()>(cse_platform::GetSymbol(complete, "CseTransportFixtureClient"));
    const auto value = reinterpret_cast<int (*)(int)>(cse_platform::GetSymbol(complete, "CseTransportFixtureValue"));
    const auto failure = reinterpret_cast<void (*)(int, int)>(
        cse_platform::GetSymbol(complete, "CseTransportFixtureFailure"));
    Require(getClient && value && failure, "controlled fixture exports");
    client = getClient();
    if (sizeof(void*) == 8) {
        Require(reinterpret_cast<std::uintptr_t>(client) > UINT32_MAX, "SDK opaque core above 4 GiB");
    }
    cse_steam::Api api{};
    api.createInterface = &Factory;
    {
        cse_steam::ServerTransport transport;
        Require(!transport.Retain(api, nullptr), "reject absent provider");
        Require(!transport.Retain(api, incomplete), "reject incomplete flat API atomically");
        Require(factoryCalls == 0, "incomplete binding never reaches factory or create");
        Require(std::strcmp(transport.LastError(), "SteamAPI_ISteamClient_BShutdownIfAllPipesClosed") == 0,
            "first missing export diagnostic");
        missingClient = true;
        Require(!transport.Retain(api, complete) && value(Creates) == 0, "missing client cannot create a pipe");
        missingClient = false;
        failure(1, 0);
        Require(!transport.Retain(api, complete) && value(LivePipes) == 0, "failed pipe creation owns nothing");
        failure(0, 0);
        Require(transport.Retain(api, complete) && value(LivePipes) != 0, "create owned anonymous transport");
        const int firstCreateCount = value(Creates);
        Require(transport.Retain(api, complete) && value(Creates) == firstCreateCount, "repeat retains one pipe");
        failure(0, 1);
        Require(!transport.Close() && value(LivePipes) != 0 && value(Shutdowns) == 0, "release failure preserves ownership");
        failure(0, 0);
        Require(transport.Close() && value(LivePipes) == 0 && value(Shutdowns) == 1, "release then global shutdown");
        Require(transport.Close() && value(Shutdowns) == 1, "close is idempotent");
        Require(transport.Retain(api, complete), "closed owner can retain a new transport");
    }
    Require(value(LivePipes) == 0 && value(Shutdowns) == 2, "destructor releases final transport");
    {
        cse_steam::ServerTransport first;
        cse_steam::ServerTransport second;
        Require(first.Retain(api, complete) && second.Retain(api, complete) && value(LivePipes) == 2,
            "two independent consumers retain distinct pipes");
        Require(first.Close() && value(LivePipes) == 1, "one consumer closes while SDK stays alive for its peer");
        Require(second.Close() && value(LivePipes) == 0 && value(Shutdowns) == 4, "last peer closes global transport");
    }
    cse_platform::UnloadModule(incomplete);
    cse_platform::UnloadModule(complete);
    std::puts("native_steam_server_transport_passed=1");
}
