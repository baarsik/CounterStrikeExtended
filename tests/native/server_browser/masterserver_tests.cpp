#include "ServerBrowserMasterserverQuery.h"
#include "provider_fixture.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <set>
#include <map>
#include <thread>
#define CHECK(value) do { if (!(value)) { throw std::runtime_error(#value); } } while (false)
struct MetadataProvider final : Provider {
    bool failPing = false;
    int canceledPings = 0;
    std::set<HServerQuery> canceledQueries;
    std::function<void()> duringPing;
    std::function<void()> duringCancel;
    static constexpr HServerQuery FirstQuery = 42;
    bool validQueries = false;
    bool deferPings = false;
    uint16 connectionPortOffset = 0;
    struct PendingPing {
        uint32 ip;
        uint16 port;
        ISteamMatchmakingPingResponse* response;
    };
    std::map<HServerQuery, PendingPing> pendingPings;
    int nextQuery = FirstQuery;
    void Respond(uint32 ip, uint16 port, ISteamMatchmakingPingResponse* response) {
        gameserveritem_t server;
        server.m_NetAdr.Init(ip, port, static_cast<uint16>(port + connectionPortOffset));
        server.m_nAppID = 10;
        server.SetName("Loopback server");
        response->ServerResponded(server);
    }
    HServerQuery PingServer(uint32 ip, uint16 port, ISteamMatchmakingPingResponse* response) override {
        const HServerQuery query = validQueries ? nextQuery++ : HSERVERQUERY_INVALID;
        if (deferPings) {
            pendingPings.emplace(query, PendingPing{ip, port, response});
        } else if (failPing) {
            response->ServerFailedToRespond();
        } else {
            Respond(ip, port, response);
        }
        if (duringPing) {
            auto callback = std::move(duringPing);
            callback();
        }
        return query;
    }
    void CompletePings() {
        auto completed = std::move(pendingPings);
        pendingPings.clear();
        for (const auto& entry : completed) {
            const auto& ping = entry.second;
            if (failPing) {
                ping.response->ServerFailedToRespond();
            } else {
                Respond(ping.ip, ping.port, ping.response);
            }
        }
    }
    void CancelServerQuery(HServerQuery query) override {
        CHECK(pendingPings.erase(query) == 1);
        ++canceledPings;
        CHECK(canceledQueries.insert(query).second);
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
    constexpr uint16 ConnectionPortOffset = 10;
    steam.connectionPortOffset = ConnectionPortOffset;
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

        // The directory is still unavailable, but each retained server has its own query address.
        steam.validQueries = true;
        steam.deferPings = true;
        const auto retained = model.OrderedKeys();
        CHECK(model.QuickRefresh());
        CHECK(!model.QuickRefresh());
        model.Pump();
        CHECK(steam.pendingPings.size() == retained.size());
        for (const auto& entry : steam.pendingPings) {
            const auto& ping = entry.second;
            CHECK(model.Find({ping.ip, static_cast<uint16>(ping.port + ConnectionPortOffset), ping.port}));
        }
        steam.CompletePings();
        constexpr auto EmptyRefreshWait = std::chrono::seconds(1);
        PumpUntilSettled(provider, model, EmptyRefreshWait);
        CHECK(model.GetStatus().phase == Phase::Complete);
        CHECK(model.GetStatus().responding == retained.size());
        CHECK(model.OrderedKeys() == retained);
    }
    CHECK(provider.BeginClose());
}

void LargeQuickRefresh(const char* directory, bool exceedsLimit) {
    MetadataProvider steam;
    MasterserverQueryProvider provider(&steam);
    provider.Select({"loopback", "Loopback", {directory}});
    Responses responses;
    constexpr AppId_t CounterStrikeApp = 10;
    constexpr int ServerCount = 65536;
    const auto request = provider.RequestInternetServerList(CounterStrikeApp, nullptr, 0, &responses);
    CHECK(request);

    const auto pumpUntilComplete = [&] {
        constexpr auto DiscoveryTimeout = std::chrono::seconds(10);
        const auto deadline = std::chrono::steady_clock::now() + DiscoveryTimeout;
        while (!responses.complete && std::chrono::steady_clock::now() < deadline) {
            constexpr size_t FrameBudget = 128;
            provider.Pump(FrameBudget);
            std::this_thread::yield();
        }
        CHECK(responses.complete);
    };
    pumpUntilComplete();
    CHECK(responses.responses == ServerCount);
    const auto discoveryResult = responses.result;

    responses = Responses{};
    const auto started = std::clock();
    constexpr int RepeatedRefreshes = 12;
    for (int refresh = 0; refresh < RepeatedRefreshes; ++refresh) {
        provider.RefreshQuery(request);
    }
    constexpr std::clock_t SchedulingBudget = 2 * CLOCKS_PER_SEC;
    const bool withinBudget = std::clock() - started < SchedulingBudget;
    CHECK(responses.responses == 0);
    provider.Pump(); // Some rows are now active and the rest are queued.
    provider.RefreshServer(request, 0);
    provider.RefreshServer(request, ServerCount - 1);
    pumpUntilComplete();
    CHECK(responses.responses == ServerCount);
    provider.ReleaseRequest(request);
    while (!provider.BeginClose()) {
    }
    CHECK(withinBudget);
    CHECK(discoveryResult == (exceedsLimit ? eServerFailedToRespond : eServerResponded));
}

void CompletedPings(const char* directory, bool cancelBeforePump, bool inlineCompletion, bool failed) {
    MetadataProvider steam;
    steam.validQueries = true;
    steam.deferPings = !inlineCompletion;
    steam.failPing = failed;
    MasterserverQueryProvider provider(&steam);
    provider.Select({"loopback", "Loopback", {directory}});
    Responses responses;
    constexpr AppId_t CounterStrikeApp = 10;
    auto request = provider.RequestInternetServerList(CounterStrikeApp, nullptr, 0, &responses);
    constexpr auto DirectoryWait = std::chrono::seconds(8);
    constexpr int DirectoryServerCount = 2; // The loopback directory fixture publishes two endpoints.
    constexpr int ExpectedNextQuery = MetadataProvider::FirstQuery + DirectoryServerCount;
    const auto deadline = std::chrono::steady_clock::now() + DirectoryWait;
    while (std::chrono::steady_clock::now() < deadline && steam.nextQuery < ExpectedNextQuery) {
        provider.Pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(steam.nextQuery == ExpectedNextQuery);
    steam.CompletePings();
    if (!cancelBeforePump) {
        while (std::chrono::steady_clock::now() < deadline && !responses.complete) {
            provider.Pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(responses.complete && responses.responses == DirectoryServerCount);
        CHECK(responses.failed == (failed ? DirectoryServerCount : 0));
    }
    provider.CancelQuery(request);
    provider.ReleaseRequest(request);
    while (std::chrono::steady_clock::now() < deadline && !provider.BeginClose()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(provider.BeginClose());
    CHECK(steam.canceledPings == 0);
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
        const std::string test = argv[2];
        if (test == "cancel-completes-other") {
            MetadataProvider steam;
            steam.validQueries = true;
            steam.deferPings = true;
            MasterserverQueryProvider provider(&steam);
            provider.Select({"loopback", "Loopback", {argv[1]}});
            Responses responses;
            constexpr AppId_t CounterStrikeApp = 10;
            auto request = provider.RequestInternetServerList(CounterStrikeApp, nullptr, 0, &responses);
            constexpr size_t DirectoryServerCount = 2;
            constexpr auto DirectoryWait = std::chrono::seconds(8);
            const auto deadline = std::chrono::steady_clock::now() + DirectoryWait;
            while (std::chrono::steady_clock::now() < deadline && steam.pendingPings.size() < DirectoryServerCount) {
                provider.Pump();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            CHECK(steam.pendingPings.size() == DirectoryServerCount);
            steam.duringCancel = [&] {
                steam.CompletePings();
            };
            provider.CancelQuery(request);
            provider.ReleaseRequest(request);
            while (std::chrono::steady_clock::now() < deadline && !provider.BeginClose()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            CHECK(provider.BeginClose() && steam.canceledPings == 1 && !responses.complete);
            return 0;
        }
        if (test == "completed-pump" || test == "completed-cancel" || test == "completed-inline" ||
            test == "failed-ping-cancel") {
            CompletedPings(argv[1], test == "completed-cancel" || test == "failed-ping-cancel",
                test == "completed-inline", test == "failed-ping-cancel");
            std::cout << "completed ping lifetime passed\n";
            return 0;
        }
        if (test == "large-quick-refresh" || test == "directory-limit-exceeded") {
            LargeQuickRefresh(argv[1], test == "directory-limit-exceeded");
            return 0;
        }
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
            steam.validQueries = true;
            steam.deferPings = true;
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
            if (cancelReentry && !retired && !steam.pendingPings.empty()) {
                provider.CancelQuery(request);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (cancel || reentry) {
            CHECK(!responses.complete && responses.responses == 0);
            CHECK(!reentry || (retired && steam.canceledPings >= 1));
            CHECK(!cancelReentry || steam.canceledPings == steam.nextQuery - MetadataProvider::FirstQuery);
        } else {
            CHECK(responses.complete && responses.result == eServerResponded);
            CHECK(provider.GetServerCount(request) == 2);
            CHECK(responses.responses == 2);
            CHECK(provider.GetServerDetails(request, 0)->m_NetAdr.GetIP() == 0x7f000001);
            CHECK(provider.GetServerDetails(request, 0)->m_NetAdr.GetQueryPort() == 27015);
            CHECK(provider.GetServerDetails(request, 1)->m_NetAdr.GetQueryPort() == 27016);
            CHECK(responses.failed == 0);
            responses.complete = false;
            steam.failPing = true;
            provider.RefreshServer(request, 0);
            for (int turn = 0; turn < 10 && responses.responses < 3; ++turn) {
                provider.Pump();
            }
            CHECK(responses.responses == 3 && responses.failed == 1);
            CHECK(!responses.complete);
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
