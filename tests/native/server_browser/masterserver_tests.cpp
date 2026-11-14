#include "ServerBrowserMasterserverQuery.h"
#include "provider_fixture.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <set>
#include <thread>
#define CHECK(value) do { if (!(value)) { throw std::runtime_error(#value); } } while (false)
struct MetadataProvider final : Provider {
    bool failPing = false;
    int canceledPings = 0;
    std::set<HServerQuery> canceledQueries;
    std::function<void()> duringPing;
    std::function<void()> duringCancel;
    bool validQueries = false;
    int nextQuery = 42;
    HServerQuery PingServer(uint32 ip, uint16 port, ISteamMatchmakingPingResponse* response) override {
        if (failPing) {
            response->ServerFailedToRespond();
        } else {
            gameserveritem_t server;
            server.m_NetAdr.Init(ip, port, port);
            server.m_nAppID = 10;
            server.SetName("Loopback server");
            response->ServerResponded(server);
        }
        if (duringPing) {
            auto callback = std::move(duringPing);
            callback();
            return 42;
        }
        return validQueries ? nextQuery++ : HSERVERQUERY_INVALID;
    }
    void CancelServerQuery(HServerQuery query) override {
        if (query >= 42) {
            ++canceledPings;
            CHECK(canceledQueries.insert(query).second);
        }
        if (duringCancel) {
            auto callback = std::move(duringCancel);
            callback();
        }
    }
};
struct Responses final : ISteamMatchmakingServerListResponse {
    int responses = 0;
    int failed = 0;
    bool complete = false;
    EMatchMakingServerResponse result = eServerFailedToRespond;

    void ServerResponded(HServerListRequest, int) override {
        ++responses;
    }

    void ServerFailedToRespond(HServerListRequest, int) override {
        ++responses;
        ++failed;
    }

    void RefreshComplete(HServerListRequest, EMatchMakingServerResponse value) override {
        complete = true;
        result = value;
    }
};

void PumpUntilSettled(MasterserverQueryProvider& provider, ServerBrowserModel& model,
                      std::chrono::steady_clock::duration timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        provider.Pump();
        model.Pump();
        model.TakeUpdates();
        const auto status = model.GetStatus();
        if (status.phase != Phase::Refreshing && !status.pending && !status.viewUpdating) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("model remained refreshing after the provider had no work");
}

void FailedEmptyRefresh(const char* directory, const char* unavailable) {
    MetadataProvider steam;
    MasterserverQueryProvider provider(&steam);
    provider.Select({"loopback", "Loopback", {directory}});
    {
        ServerBrowserModel model({&provider, nullptr});
        Filters filters;
        constexpr AppId_t CounterStrikeApp = 10;
        filters.app = CounterStrikeApp;
        constexpr auto DirectoryWait = std::chrono::seconds(8);
        CHECK(model.Refresh(ListKind::Internet, filters));
        PumpUntilSettled(provider, model, DirectoryWait);
        CHECK(model.GetStatus().phase == Phase::Complete);
        CHECK(model.OrderedKeys().size() == 2);

        provider.Select({"unavailable", "Unavailable", {unavailable}});
        CHECK(model.Refresh(ListKind::Internet, filters));
        PumpUntilSettled(provider, model, DirectoryWait);
        CHECK(model.GetStatus().phase == Phase::Failed);
        CHECK(model.OrderedKeys().size() == 2);

        CHECK(model.QuickRefresh());
        constexpr auto EmptyRefreshWait = std::chrono::seconds(1);
        PumpUntilSettled(provider, model, EmptyRefreshWait);
        CHECK(model.GetStatus().phase == Phase::Failed);
        CHECK(model.OrderedKeys().size() == 2);
    }
    CHECK(provider.BeginClose());
}

int main(int argc, char** argv) {
    try {
        CHECK(argc >= 3);
        if (std::string(argv[2]) == "failed-empty-refresh") {
            CHECK(argc == 4);
            FailedEmptyRefresh(argv[1], argv[3]);
            std::cout << "failed empty refresh completed\n";
            return 0;
        }
        CHECK(argc == 3);
        MetadataProvider steam;
        MasterserverQueryProvider provider(&steam);
        MasterserverSettings settings;
        CHECK(settings.Add("Loopback", argv[1]));
        settings.selected = settings.custom.front().id;
        provider.Select(settings.Selected());
        Responses responses;
        auto request = provider.RequestInternetServerList(10, nullptr, 0, &responses);
        CHECK(request);
        const bool cancel = std::string(argv[2]) == "cancel";
        const bool cancelReentry = std::string(argv[2]) == "cancel-reentry";
        const bool reentry = std::string(argv[2]) == "reentry" || cancelReentry;
        bool retired = false;
        if (reentry) {
            const auto release = [&] {
                provider.ReleaseRequest(request);
                retired = true;
                CHECK(!provider.BeginClose());
            };
            if (cancelReentry) {
                steam.validQueries = true;
                steam.duringCancel = release;
            } else {
                steam.duringPing = release;
            }
        }
        if (cancel) {
            provider.CancelQuery(request);
            provider.ReleaseRequest(request);
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        while (std::chrono::steady_clock::now() < deadline &&
            (reentry ? (!retired || !provider.BeginClose()) : cancel ? !provider.BeginClose() : !responses.complete)) {
            provider.Pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (cancel || reentry) {
            CHECK(!responses.complete && responses.responses == 0);
            CHECK(!reentry || (retired && steam.canceledPings >= 1));
            CHECK(!cancelReentry || steam.canceledPings == steam.nextQuery - 42);
        } else {
            CHECK(responses.complete && responses.result == eServerResponded);
            CHECK(provider.GetServerCount(request) == 2);
            CHECK(responses.responses == 2);
            CHECK(provider.GetServerDetails(request, 0)->m_NetAdr.GetIP() == 0x7f000001);
            CHECK(provider.GetServerDetails(request, 0)->m_NetAdr.GetQueryPort() == 27015);
            CHECK(provider.GetServerDetails(request, 1)->m_NetAdr.GetQueryPort() == 27016);
            CHECK(responses.failed == 0);
            steam.failPing = true;
            provider.RefreshServer(request, 0);
            for (int turn = 0; turn < 10 && responses.responses < 3; ++turn) {
                provider.Pump();
            }
            CHECK(responses.responses == 3 && responses.failed == 1);
            provider.ReleaseRequest(request);
        }
        if (!cancel && !reentry) {
            CHECK(!provider.BeginClose(1));
        }
        CHECK(provider.BeginClose());
        std::cout << "masterserver query passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
