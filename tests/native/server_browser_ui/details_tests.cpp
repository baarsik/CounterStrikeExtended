#include "model_fixture.inc"
#include "ServerBrowserDetails.h"
#include "check.h"
#include "detail_provider_fixture.h"

namespace {
constexpr Address ServerAddress{0x7f000001U, 27015, 27016};
}
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        DetailProvider provider;
        ServerBrowserDetails details(&provider);
        if (name == "synchronous-copy") {
            provider.synchronous = true;
            Require(details.Start(ServerAddress), "query startup failed");
            auto updates = details.TakeUpdates(32);
            Require(provider.port == ServerAddress.queryPort, "details used connection port");
            Require(updates.size() == 2 && updates[0].name == "player name", "player data not sanitized/copied");
            Require(updates[1].name == "name;quit" && updates[1].value == "value text", "rule copy failed");
            Require(details.Status().playersComplete && details.Status().rulesComplete, "sync completion lost");
            Require(details.BeginClose(), "completed queries did not close");
            Require(provider.canceled == 0, "already completed query handle canceled");
        } else if (name == "cancel-reentry") {
            Require(details.Start(ServerAddress), "query startup failed");
            provider.duringCancel = [&] {
                Require(!details.BeginClose(), "reentrant cancellation authorized callback deletion");
            };
            Require(details.BeginClose(), "cancellation did not finish");
            Require(provider.canceled == 2, "outstanding queries not canceled exactly once");
            Require(details.TakeUpdates(32).empty(), "cancellation callback leaked stale display data");
        } else if (name == "start-reentry") {
            provider.duringStart = [&] {
                Require(!details.BeginClose(), "in-flight query startup authorized deletion");
            };
            Require(!details.Start(ServerAddress), "retired startup reported active");
            Require(details.BeginClose(), "returned startup handle not cancelable");
            Require(provider.canceled == 1 && !provider.rules, "retired startup created second query");
        } else if (name == "bounded-drain") {
            Require(details.Start(ServerAddress), "query startup failed");
            constexpr int RowCount = 10000;
            for (int index = 0; index < RowCount; ++index) {
                provider.rules->RulesResponded("rule", "value");
            }
            Require(details.TakeUpdates(32).size() == 32, "detail drain ignored budget");
            Require(!details.BeginClose(), "close synchronously reclaimed unbounded callback queue");
            int attempts = 1;
            while (!details.BeginClose()) {
                ++attempts;
            }
            Require(attempts > 300, "detail close exceeded 32-unit reclamation budget");
        } else {
            throw std::runtime_error("unknown details case");
        }
    });
}
