#include "admin_actual.inc"
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
struct Receiver : IServerDataResponse {
    std::map<std::string, std::string> data;
    void OnServerDataResponse(const char* name, const char* value) override {
        data[name] = value;
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
    CHECK(receiver.data.at("maplist") == "crossfire\nboot_camp\n");
    CHECK(receiver.data.at("playerlist").find("\"Fixture player\" STEAM_0:1:123") == 0);
    CHECK(receiver.data.at("banlist") == "1 STEAM_0:1:123 : 10.000 min\n");
    CHECK(receiver.data.at("stats") == "1 2 3 4 5 6 7");
    CHECK(receiver.data.at("uptime") == "50");
    CHECK(receiver.data.at("provider_secure") == "0");
    remote.AddServerMessageHandler(&receiver, "UpdatePlayers");
    NotifyDedicatedServerUI("UpdatePlayers");
    CHECK(remote.ProcessServerResponse());
    CHECK(receiver.data.count("UpdatePlayers") == 1);
    remote.Shutdown();
    remote.SendCommand("quit");
    CHECK(executions == 1);
}
