#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#include <exception>
#endif
// Typed controlled provider shared by owner and actual VGUI tests.
namespace {
#if defined(_WIN32)
// Match the launcher's Winsock 2.0 reference for standalone resolver fixtures.
class FixtureWinsockContext final {
public:
    FixtureWinsockContext() {
        constexpr WORD RequiredWinsockVersion = MAKEWORD(2, 0);
        WSADATA data{};
        Require(WSAStartup(RequiredWinsockVersion, &data) == 0, "fixture Winsock startup failed");
        if (data.wVersion != RequiredWinsockVersion) {
            Cleanup();
            throw std::runtime_error("fixture requires launcher Winsock 2.0");
        }
    }

    ~FixtureWinsockContext() {
        Cleanup();
    }

    FixtureWinsockContext(const FixtureWinsockContext &) = delete;
    FixtureWinsockContext &operator=(const FixtureWinsockContext &) = delete;

private:
    static void Cleanup() noexcept {
        if (WSACleanup() != 0) {
            std::terminate();
        }
    }
};

#endif
class DetailProvider final : public ISteamMatchmakingServers {
public:
    Provider lists;
    bool deferListRefresh = false;
    std::vector<int> refreshedIndexes;
    std::function<void()> duringListRefresh;
    ISteamMatchmakingPlayersResponse *players = nullptr;
    ISteamMatchmakingRulesResponse *rules = nullptr;
    std::function<void()> duringStart;
    std::function<void()> duringCancel;
    bool synchronous = false;
    int canceled = 0;
    int pings = 0;
    std::map<HServerQuery, ISteamMatchmakingPingResponse *> pingCallbacks;
    std::vector<uint16_t> pingPorts;
    bool pingSynchronous = false;
    std::function<void()> duringPing;
    HServerQuery nextPing = 100;
    uint16_t port = 0;
#define FORWARD_LIST(name)                                                                                             \
    HServerListRequest name(AppId_t app, MatchMakingKeyValuePair_t **filters, uint32 count,                            \
                            ISteamMatchmakingServerListResponse *response) override {                                  \
        return lists.name(app, filters, count, response);                                                              \
    }
    FORWARD_LIST(RequestInternetServerList)
    FORWARD_LIST(RequestFavoritesServerList)
    FORWARD_LIST(RequestHistoryServerList)
    FORWARD_LIST(RequestFriendsServerList)
    FORWARD_LIST(RequestSpectatorServerList)
#undef FORWARD_LIST
    HServerListRequest RequestLANServerList(AppId_t app, ISteamMatchmakingServerListResponse *response) override {
        return lists.RequestLANServerList(app, response);
    }
    void ReleaseRequest(HServerListRequest request) override {
        lists.ReleaseRequest(request);
    }
    gameserveritem_t *GetServerDetails(HServerListRequest request, int index) override {
        return lists.GetServerDetails(request, index);
    }
    void CancelQuery(HServerListRequest request) override {
        lists.CancelQuery(request);
    }
    void RefreshQuery(HServerListRequest request) override {
        lists.RefreshQuery(request);
    }
    bool IsRefreshing(HServerListRequest request) override {
        return lists.IsRefreshing(request);
    }
    int GetServerCount(HServerListRequest request) override {
        return lists.GetServerCount(request);
    }
    void RefreshServer(HServerListRequest request, int index) override {
        refreshedIndexes.push_back(index);
        if (!deferListRefresh) {
            lists.RefreshServer(request, index);
        }
        if (duringListRefresh) {
            duringListRefresh();
        }
    }
    HServerQuery PingServer(uint32 ip, uint16 queryPort, ISteamMatchmakingPingResponse *response) override {
        ++pings;
        pingPorts.push_back(queryPort);
        const auto handle = nextPing++;
        pingCallbacks.emplace(handle, response);
        if (pingSynchronous) {
            auto item = Server(ip);
            item.m_NetAdr.Init(ip, queryPort, 27015);
            response->ServerResponded(item);
        }
        if (duringPing) {
            duringPing();
        }
        return handle;
    }
    HServerQuery PlayerDetails(uint32, uint16 queryPort, ISteamMatchmakingPlayersResponse *response) override {
        port = queryPort;
        players = response;
        if (synchronous) {
            char name[] = "player\nname";
            players->AddPlayerToList(name, 42, 60.0f);
            name[0] = '!';
            players->PlayersRefreshComplete();
        }
        if (duringStart) {
            duringStart();
        }
        return 1;
    }
    HServerQuery ServerRules(uint32, uint16 queryPort, ISteamMatchmakingRulesResponse *response) override {
        port = queryPort;
        rules = response;
        if (synchronous) {
            rules->RulesResponded("name;quit", "value\ntext");
            rules->RulesRefreshComplete();
        }
        return 2;
    }
    void CancelServerQuery(HServerQuery handle) override {
        ++canceled;
        if (duringCancel) {
            duringCancel();
        }
        if (handle == 1) {
            players->AddPlayerToList("stale", 0, 0);
            players = nullptr;
        } else if (handle == 2) {
            rules->RulesResponded("stale", "stale");
            rules = nullptr;
        } else {
            const auto found = pingCallbacks.find(handle);
            if (found != pingCallbacks.end()) {
                auto item = Server();
                found->second->ServerResponded(item);
                pingCallbacks.erase(found);
            }
        }
    }
};
}
