#pragma once

#include "ServerBrowserMasterservers.h"
#include "ServerBrowserModel.h"

namespace cse::server_browser {
// Main-thread adapter. Workers only resolve/query directories; Steam still owns A2S metadata queries.
class MasterserverQueryProvider final : public ISteamMatchmakingServers {
public:
    explicit MasterserverQueryProvider(ISteamMatchmakingServers* steam);
    ~MasterserverQueryProvider();
    void Select(const Masterserver& provider);
    void Pump(size_t budget = 128);
    bool BeginClose(size_t budget = 128);
    HServerListRequest RequestInternetServerList(AppId_t, MatchMakingKeyValuePair_t**, uint32,
        ISteamMatchmakingServerListResponse*) override;
    HServerListRequest RequestSpectatorServerList(AppId_t, MatchMakingKeyValuePair_t**, uint32,
        ISteamMatchmakingServerListResponse*) override;
    HServerListRequest RequestLANServerList(AppId_t, ISteamMatchmakingServerListResponse*) override;
    HServerListRequest RequestFriendsServerList(AppId_t, MatchMakingKeyValuePair_t**, uint32,
        ISteamMatchmakingServerListResponse*) override;
    HServerListRequest RequestFavoritesServerList(AppId_t, MatchMakingKeyValuePair_t**, uint32,
        ISteamMatchmakingServerListResponse*) override;
    HServerListRequest RequestHistoryServerList(AppId_t, MatchMakingKeyValuePair_t**, uint32,
        ISteamMatchmakingServerListResponse*) override;
    void ReleaseRequest(HServerListRequest) override;
    gameserveritem_t* GetServerDetails(HServerListRequest, int) override;
    void CancelQuery(HServerListRequest) override;
    void RefreshQuery(HServerListRequest) override;
    bool IsRefreshing(HServerListRequest) override;
    int GetServerCount(HServerListRequest) override;
    void RefreshServer(HServerListRequest, int) override;
    HServerQuery PingServer(uint32, uint16, ISteamMatchmakingPingResponse*) override;
    HServerQuery PlayerDetails(uint32, uint16, ISteamMatchmakingPlayersResponse*) override;
    HServerQuery ServerRules(uint32, uint16, ISteamMatchmakingRulesResponse*) override;
    void CancelServerQuery(HServerQuery) override;
private:
    struct State;
    std::unique_ptr<State> state_;
    HServerListRequest Start(AppId_t, MatchMakingKeyValuePair_t**, uint32,
        ISteamMatchmakingServerListResponse*, bool spectator);
};
}
