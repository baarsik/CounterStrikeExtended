#pragma once
#include "ServerBrowserModel.h"
#include <functional>
#include <map>
#include <vector>
using namespace cse::server_browser;

// The pinned address type supplies assignment, not a copy constructor. Exercise its declared
// assignment path explicitly so the fixture does not trigger deprecated implicit copies.
struct FixtureServer : gameserveritem_t {
    FixtureServer() = default;
    FixtureServer(const FixtureServer& other) {
        gameserveritem_t::operator=(other);
    }
    FixtureServer& operator=(const FixtureServer& other) {
        gameserveritem_t::operator=(other);
        return *this;
    }
};

// Provider boundary fixture: real model decisions run against the exact Valve interface.
struct Provider : ISteamMatchmakingServers {
    struct Request {
        ISteamMatchmakingServerListResponse* callback;
        std::vector<FixtureServer> rows;
        bool refreshing = true;
    };
    std::map<HServerListRequest, Request> requests;
    std::vector<FixtureServer> nextRows;
    std::function<void()> duringRequest;
    std::function<void()> duringDetails;
    bool synchronous = false;
    bool cancelCallbacks = false;
    bool failRequest = false;
    int canceled = 0;
    int released = 0;
    int refreshed = 0;
    int lastIndex = -1;
    ListKind lastKind = ListKind::Internet;
    AppId_t lastApp = 0;
    std::map<std::string, std::string> lastFilters;
    HServerListRequest current = nullptr;
    uintptr_t serial = 0;
    HServerListRequest Start(ListKind kind, AppId_t app, MatchMakingKeyValuePair_t** filters,
        uint32 count, ISteamMatchmakingServerListResponse* callback) {
        lastKind = kind;
        lastApp = app;
        lastFilters.clear();
        for (uint32 index = 0; index < count; ++index) {
            lastFilters[filters[index]->m_szKey] = filters[index]->m_szValue;
        }
        if (failRequest) {
            return nullptr;
        }
        current = reinterpret_cast<HServerListRequest>(++serial);
        requests.emplace(current, Request{callback, nextRows});
        if (synchronous) {
            callback->ServerResponded(current, 0);
            Complete();
        }
        if (duringRequest) {
            duringRequest();
        }
        return current;
    }
#define LIST_METHOD(method, kind) \
    HServerListRequest method(AppId_t app, MatchMakingKeyValuePair_t** filters, uint32 count, \
        ISteamMatchmakingServerListResponse* callback) override { \
        return Start(ListKind::kind, app, filters, count, callback); \
    }
    LIST_METHOD(RequestInternetServerList, Internet)
    LIST_METHOD(RequestFavoritesServerList, Favorites)
    LIST_METHOD(RequestHistoryServerList, History)
    LIST_METHOD(RequestFriendsServerList, Friends)
    LIST_METHOD(RequestSpectatorServerList, Spectate)
    HServerListRequest RequestLANServerList(AppId_t app, ISteamMatchmakingServerListResponse* callback) override {
        return Start(ListKind::LAN, app, nullptr, 0, callback);
    }
    void CancelQuery(HServerListRequest request) override {
        ++canceled;
        requests.at(request).refreshing = false;
        if (cancelCallbacks) {
            requests.at(request).callback->ServerResponded(request, 0);
            requests.at(request).callback->RefreshComplete(request, eServerFailedToRespond);
        }
    }
    void ReleaseRequest(HServerListRequest request) override {
        ++released;
        if (cancelCallbacks) {
            requests.at(request).callback->ServerFailedToRespond(request, 0);
        }
        requests.erase(request);
    }
    gameserveritem_t* GetServerDetails(HServerListRequest request, int index) override {
        if (duringDetails) {
            duringDetails();
        }
        auto& rows = requests.at(request).rows;
        return index >= 0 && static_cast<size_t>(index) < rows.size() ? &rows[static_cast<size_t>(index)] : nullptr;
    }
    int GetServerCount(HServerListRequest request) override {
        return static_cast<int>(requests.at(request).rows.size());
    }
    bool IsRefreshing(HServerListRequest request) override {
        return requests.at(request).refreshing;
    }
    void RefreshQuery(HServerListRequest request) override {
        ++refreshed;
        requests.at(request).refreshing = true;
    }
    void RefreshServer(HServerListRequest request, int index) override {
        lastIndex = index;
        requests.at(request).callback->ServerResponded(request, index);
    }
    HServerQuery PingServer(uint32, uint16, ISteamMatchmakingPingResponse*) override {
        return HSERVERQUERY_INVALID;
    }
    HServerQuery PlayerDetails(uint32, uint16, ISteamMatchmakingPlayersResponse*) override {
        return HSERVERQUERY_INVALID;
    }
    HServerQuery ServerRules(uint32, uint16, ISteamMatchmakingRulesResponse*) override {
        return HSERVERQUERY_INVALID;
    }
    void CancelServerQuery(HServerQuery) override {}
    void Complete(EMatchMakingServerResponse result = eServerResponded) {
        requests.at(current).refreshing = false;
        requests.at(current).callback->RefreshComplete(current, result);
    }
};
