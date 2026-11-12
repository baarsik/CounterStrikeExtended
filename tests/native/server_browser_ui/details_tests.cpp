#include "model_fixture.inc"
#include "ServerBrowserDetails.h"
#include "check.h"
#include "detail_provider_fixture.h"
#include <chrono>
#include <thread>

namespace {
constexpr Address ServerAddress{0x7f000001U, 27015, 27016};
}
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
#if defined(_WIN32)
        const FixtureWinsockContext winsock;
#endif
        DetailProvider provider;
        ServerBrowserDetails details(&provider);
        if (name == "original-metadata") {
            Require(details.Start(ServerAddress), "metadata query startup failed");
            Require(provider.pings == 1 && !provider.players && !provider.rules,
                    "GameInfo must start only metadata PingServer");
        } else if (name == "synchronous-copy") {
            provider.pingSynchronous = true;
            provider.synchronous = true;
            Require(details.Start(ServerAddress), "query startup failed");
            const auto metadata = details.TakeMetadata();
            Require(metadata.size() == 1 && metadata[0].key == ServerAddress, "metadata endpoint copy failed");
            Require(details.StartPlayers(), "player query startup failed");
            const auto updates = details.TakeUpdates(32);
            Require(provider.port == ServerAddress.queryPort, "details used connection port");
            Require(updates.size() == 1 && updates[0].name == "player name", "player data not sanitized/copied");
            Require(!provider.rules, "original GameInfo must not query rules");
            Require(details.BeginClose(), "completed queries did not close");
            Require(provider.canceled == 0, "already completed query handle canceled");
        } else if (name == "cancel-reentry") {
            Require(details.Start(ServerAddress) && details.StartPlayers(), "query startup failed");
            provider.duringCancel = [&] {
                Require(!details.BeginClose(), "reentrant cancellation authorized callback deletion");
            };
            Require(details.BeginClose(), "cancellation did not finish");
            Require(provider.canceled == 2, "outstanding queries not canceled exactly once");
            Require(details.TakeUpdates(32).empty() && details.TakeMetadata().empty(),
                    "cancellation callback leaked stale display data");
        } else if (name == "start-reentry") {
            provider.duringPing = [&] {
                Require(!details.BeginClose(), "in-flight query startup authorized deletion");
            };
            Require(!details.Start(ServerAddress), "retired startup reported active");
            Require(details.BeginClose(), "returned startup handle not cancelable");
            Require(provider.canceled == 1 && !provider.players, "retired startup created second query");
        } else if (name == "bounded-drain") {
            Require(details.Start(ServerAddress) && details.StartPlayers(), "query startup failed");
            constexpr int RowCount = 10000;
            for (int index = 0; index < RowCount; ++index) {
                provider.players->AddPlayerToList("player", 0, 0);
            }
            Require(details.TakeUpdates(32).size() == 32, "detail drain ignored budget");
            Require(!details.BeginClose(), "close synchronously reclaimed unbounded callback queue");
            int attempts = 1;
            while (!details.BeginClose()) {
                ++attempts;
            }
            Require(attempts > 300, "detail close exceeded 32-unit reclamation budget");
        } else if (name == "independent-refresh") {
            Require(details.Start(ServerAddress) && details.StartPlayers(), "query startup failed");
            auto *players = provider.players;
            Require(details.RefreshMetadata(), "metadata replacement failed");
            Require(provider.players == players && provider.canceled == 1,
                    "metadata refresh canceled independent player query");
            players->AddPlayerToList("still active", 7, 0);
            Require(details.TakeUpdates().size() == 1, "metadata refresh lost independent players");
            Require(details.TakeMetadata().empty(), "cancel callback contaminated successor generation");
        } else if (name == "player-generation") {
            Require(details.Start(ServerAddress) && details.StartPlayers(), "query startup failed");
            provider.players->AddPlayerToList("obsolete generation", 1, 0);
            Require(details.StartPlayers(), "replacement player query failed");
            provider.players->AddPlayerToList("current generation", 2, 0);
            const auto players = details.TakeUpdates();
            Require(players.size() == 1 && players[0].name == "current generation",
                    "old queued players contaminated successor generation");
        } else if (name == "wrong-endpoint") {
            details.Start(ServerAddress);
            auto wrong = Server();
            wrong.m_NetAdr.SetConnectionPort(27017);
            provider.pingCallbacks.begin()->second->ServerResponded(wrong);
            Require(details.TakeMetadata().empty(), "metadata retargeted selected connection port");
        } else if (name == "common-ports") {
            ServerBrowserPing ping(&provider);
            Require(ping.Start({ServerAddress.ip, 0, 0}, true), "common-port test failed");
            const std::vector<uint16_t> expected{27015, 26900, 27016, 26901, 27017, 26902, 27018,
                26903, 27019, 26904, 27020, 26905, 4242, 27215};
            Require(provider.pingPorts == expected, "original port ordering changed");
            Require(ping.BeginClose() && provider.canceled == 14, "not all test handles retired");
            Require(ping.TakeUpdates().empty(), "retired test callback leaked response");
        } else if (name == "address-input") {
            ServerAddressInput input;
            Require(ParseServerAddressInput("example.test:27016", input) && input.host == "example.test" &&
                input.port == 27016, "hostname/port rejected");
            Require(ParseServerAddressInput("127.0.0.1", input) && input.port == 0, "default-port intent lost");
            for (const char *bad : {"", "256.0.0.1", "127.0.0.1:65536", "host:999999999999999999999",
                    "host;quit", "host:1:2", "host:", "host name", "host\nquit"}) {
                Require(!ParseServerAddressInput(bad, input), "invalid address accepted");
            }
            ServerAddressResolver resolver;
            Require(resolver.Start("127.0.0.1:27016") && resolver.Complete(), "numeric resolution asynchronous");
            Require(resolver.Result() == std::optional<Address>{{ServerAddress.ip, 27016, 27016}},
                    "numeric resolver changed endpoint order");
            Require(resolver.BeginClose() && !resolver.Result(), "retired resolver returned stale address");
        } else if (name == "dns-retirement") {
            ServerAddressResolver resolver;
            Require(resolver.Start("localhost:27019"), "hostname resolver failed to start");
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!resolver.Complete() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }
            Require(resolver.Complete() && resolver.Result() && resolver.Result()->queryPort == 27019,
                    "local hostname resolver failed to publish owned IPv4 endpoint");
            Require(resolver.BeginClose() && !resolver.Result(), "DNS result survived retirement");
            ServerAddressResolver canceled;
            Require(canceled.Start("localhost:27020"), "cancel resolver failed to start");
            while (!canceled.BeginClose() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }
            Require(canceled.BeginClose() && !canceled.Result(), "closed resolver leaked late result");
        } else if (name == "history-flag") {
            Favorites favorites;
            ServerBrowserModel model({nullptr, &favorites});
            Require(model.RecordAcceptedConnection(10, ServerAddress, 123) && model.AddFavorite(10, ServerAddress),
                    "history/favorite setup failed");
            Require(model.RemoveHistory(10, ServerAddress), "history removal failed");
            Require(favorites.records.front().flags == k_unFavoriteFlagFavorite,
                    "history removal destroyed independent favorite flag");
        } else {
            throw std::runtime_error("unknown details case");
        }
    });
}
