#pragma once

// Typed controlled provider shared by owner and actual VGUI tests.
namespace {
class DetailProvider final : public ISteamMatchmakingServers {
public:
    Provider lists;
    ISteamMatchmakingPlayersResponse *players = nullptr;
    ISteamMatchmakingRulesResponse *rules = nullptr;
    std::function<void()> duringStart;
    std::function<void()> duringCancel;
    bool synchronous = false;
    int canceled = 0;
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
        lists.RefreshServer(request, index);
    }
    HServerQuery PingServer(uint32, uint16, ISteamMatchmakingPingResponse *) override {
        return HSERVERQUERY_INVALID;
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
        } else {
            rules->RulesResponded("stale", "stale");
            rules = nullptr;
        }
    }
};
}
