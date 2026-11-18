#include "provider_fixture.h"
#include "../server_browser_ui/check.h"
#include <iostream>

struct PingProvider final : Provider {
    struct Pending {
        uint32 ip;
        uint16 port;
        ISteamMatchmakingPingResponse* callback;
    };
    std::map<HServerQuery, Pending> pending;
    bool inlineCompletion = false;
    bool failed = false;
    static constexpr HServerQuery FirstQuery = 1;
    HServerQuery nextQuery = FirstQuery;
    int cancellations = 0;

    void Complete(Pending ping) {
        if (failed) {
            ping.callback->ServerFailedToRespond();
        } else {
            FixtureServer server;
            server.m_NetAdr.Init(ping.ip, ping.port, ping.port);
            server.m_nAppID = 10;
            server.SetName("Refreshed server name");
            ping.callback->ServerResponded(server);
        }
    }
    void CompletePings() {
        auto completed = std::move(pending);
        pending.clear();
        for (const auto& entry : completed) {
            Complete(entry.second);
        }
    }
    HServerQuery PingServer(uint32 ip, uint16 port, ISteamMatchmakingPingResponse* response) override {
        const auto handle = nextQuery++;
        const Pending ping{ip, port, response};
        if (inlineCompletion) {
            Complete(ping);
        } else {
            pending.emplace(handle, ping);
        }
        return handle;
    }
    void CancelServerQuery(HServerQuery handle) override {
        Require(pending.erase(handle) == 1, "only outstanding individual queries may be cancelled");
        ++cancellations;
    }
};

void Drain(ServerBrowserModel& model) {
    constexpr int DrainFrames = 20;
    for (int frame = 0; frame < DrainFrames; ++frame) {
        model.Pump();
        model.TakeUpdates();
    }
}

int main(int argc, char** argv) {
    return RunCase([&] {
        Require(argc == 2, "expected a ping lifetime scenario");
        const std::string scenario = argv[1];
        PingProvider provider;
        provider.inlineCompletion = scenario == "inline";
        provider.failed = scenario == "failed-retire";
        constexpr RowKey Address{0x7f000001, 27015, 27015};
        FixtureServer server;
        server.m_NetAdr.Init(Address.ip, Address.queryPort, Address.connectionPort);
        server.m_nAppID = 10;
        server.m_bHadSuccessfulResponse = true;
        provider.nextRows.push_back(server);
        Filters filters;
        filters.app = 10;
        ServerBrowserModel model({&provider, nullptr});
        Require(model.Refresh(ListKind::Internet, filters), "initial list request starts");
        provider.Provider::Complete();
        Drain(model);
        Require(model.Find(Address) != nullptr, "list discovery stores the refresh target");
        model.Stop();
        Drain(model);
        Require(model.QuickRefresh(Address), "a retained row starts an individual refresh after list retirement");
        model.Pump();
        Require(provider.nextQuery == PingProvider::FirstQuery + 1, "one individual ping was submitted");
        const bool pendingCancel = scenario == "pending-retire";
        if (!pendingCancel) {
            provider.CompletePings();
        }
        if (scenario == "pump" || scenario == "inline") {
            Drain(model);
            Require(model.GetStatus().phase == Phase::Complete, "terminal refresh is processed");
            const auto *row = model.Find(Address);
            Require(row && row->responded && row->name == "Refreshed server name",
                "successful direct ping replaces prior list metadata");
        }
        model.Stop();
        Drain(model);
        Require(model.BeginClose() && model.IsQuiescent(), "query callbacks retire before their owner");
        Require(provider.cancellations == (pendingCancel ? 1 : 0),
            "pending queries cancel exactly once; completed queries need no cancellation");
        std::cout << "individual ping lifetime passed\n";
    });
}
