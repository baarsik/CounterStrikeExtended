// Expose only fixture access to pending registrations; production layout and bodies remain unchanged.
#include "utlbuffer.h"
#include "utllinkedlist.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
#define private public
#include "admin_actual.inc"
#undef private
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                       \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
extern IGameServerData* ActualProvider();
extern void PopulateProviderFixture();
extern void NotifyDedicatedServerUI(const char*);
extern std::string commands;
extern int executions;
extern std::string mapcycle;
extern bool mapcycleMissing;
extern int mapcycleLoads;
extern int mapcycleFrees;
struct Receiver : IServerDataResponse {
    std::map<std::string, std::string> data;
    int responses = 0;
    void OnServerDataResponse(const char* name, const char* value) override {
        data[name] = value;
        ++responses;
    }
};
int main() {
    PopulateProviderFixture();
    CRemoteServer remote;
    Receiver receiver;
    CHECK(remote.Bind(ActualProvider(), 1));
    remote.SetValue("hostname", "Native fixture UTF-8 żółć");
    remote.SetValue("mapcycle", "crossfire\nboot_camp\n");
    remote.SendCommand("say \"quoted space\"");
    CHECK(commands == "say \"quoted space\"\n" && executions == 1);
    for (const auto* name :
         {"hostname", "mapcycle", "playerlist", "banlist", "maplist", "stats", "uptime", "provider_available",
          "provider_connected", "provider_secure_requested", "provider_secure", "provider_failure"}) {
        remote.RequestValue(&receiver, name);
    }
    CHECK(remote.ProcessServerResponse());
    CHECK(receiver.data.at("hostname") == "Native fixture UTF-8 żółć");
    CHECK(receiver.data.at("mapcycle") == "crossfire\nboot_camp\n");
    CHECK(receiver.data.at("maplist") == "crossfire\nboot.bsp.v2\ndownload\nlocalized\n");
    CHECK(receiver.data.at("playerlist").find("\"Fixture player\" STEAM_0:1:123") == 0);
    CHECK(receiver.data.at("banlist") == "1 STEAM_0:1:123 : 10.000 min\n");
    CHECK(receiver.data.at("stats") == "1 2 3 4 5 6 7");
    CHECK(receiver.data.at("uptime") == "50");
    CHECK(receiver.data.at("provider_secure") == "0");
    remote.AddServerMessageHandler(&receiver, "UpdatePlayers");
    NotifyDedicatedServerUI("UpdatePlayers");
    CHECK(remote.ProcessServerResponse());
    CHECK(receiver.data.count("UpdatePlayers") == 1);
    for (int iteration = 0; iteration < 20; ++iteration) {
        mapcycle = iteration == 0 ? "crossfire\n" : "";
        mapcycleMissing = iteration % 2 != 0;
        const int before = receiver.responses;
        remote.RequestValue(&receiver, "mapcycle");
        CHECK(remote.ProcessServerResponse());
        CHECK(receiver.responses == before + 1);
        CHECK(receiver.data.at("mapcycle") == mapcycle);
        CHECK(mapcycleLoads == mapcycleFrees);
        CHECK(remote.m_ResponseHandlers.empty());
    }
    remote.Shutdown();
    remote.SendCommand("quit");
    CHECK(executions == 1);
}
