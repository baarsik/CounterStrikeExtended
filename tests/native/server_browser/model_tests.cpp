#include "ServerBrowserModel.h"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
#include <ctime>
using namespace cse::server_browser;
#define CHECK(value) do { if (!(value)) { throw std::runtime_error(#value); } } while (false)

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
struct Provider final : ISteamMatchmakingServers {
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
    bool failRefresh = false;
    bool deferRefresh = false;
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
        if (deferRefresh) {
            return;
        }
        auto *callback = requests.at(request).callback;
        if (failRefresh) {
            callback->ServerFailedToRespond(request, index);
        } else {
            callback->ServerResponded(request, index);
        }
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

struct Favorites final : IFavoriteStore {
    std::vector<FavoriteRecord> records;
    bool failing = false;
    FavoriteReadResult ReadAll() override { return {!failing, records, failing ? "read failure" : ""}; }
    FavoriteWriteResult Add(const FavoriteRecord& record) override {
        if (failing) {
            return {false, "write failure"};
        }
        for (auto& existing : records) {
            if (existing.app == record.app && existing.address == record.address) {
                existing.flags = record.flags;
                existing.lastPlayed = record.lastPlayed;
                return {true, {}};
            }
        }
        records.push_back(record);
        return {true, {}};
    }
    FavoriteWriteResult Remove(const FavoriteRecord& record) override {
        for (auto& existing : records) {
            if (existing.app == record.app && existing.address == record.address) {
                existing.flags &= ~record.flags;
            }
        }
        return {!failing, failing ? "remove failure" : ""};
    }
};

FixtureServer Server(uint32 ip = 0x7f000001U, const char* name = "Alpha") {
    FixtureServer row;
    row.m_NetAdr.Init(ip, 27016, 27015);
    row.SetName(name);
    std::strcpy(row.m_szGameDir, "cstrike");
    std::strcpy(row.m_szMap, "de_dust2");
    row.m_nAppID = 10;
    row.m_bHadSuccessfulResponse = true;
    row.m_nPing = 42;
    row.m_nPlayers = 3;
    row.m_nMaxPlayers = 16;
    return row;
}
Filters DefaultFilters() {
    Filters filters;
    filters.app = 10;
    return filters;
}
void Drain(ServerBrowserModel& model) {
    for (int step = 0; step < 1000; ++step) {
        const size_t work = model.Pump(37);
        CHECK(work <= 37);
        model.TakeUpdates(37);
        if (!model.GetStatus().viewUpdating && model.GetStatus().pending == 0) {
            return;
        }
    }
    throw std::runtime_error("bounded drain did not converge");
}
void Addresses() {
    Address address;
    CHECK(ParseAddress("255.128.0.1:65535", address));
    CHECK(address.ip == 0xff800001U && address.connectionPort == 65535 && address.queryPort == 65535);
    CHECK(ConnectionAddress(address) == "255.128.0.1:65535");
    for (const char* bad : {"1.2.3.4", "1.2.3.256:1", "1.2.3.4:0", "1.2.3.4:65536", "a:1",
             "1.2.3.4:1;quit", "1.2.3.4:1\n", " 1.2.3.4:1", "1.2.3.4:-1"}) {
        CHECK(!ParseAddress(bad, address));
    }
}
void ListsAndRetirement() {
    Provider provider;
    provider.nextRows = {Server()};
    provider.synchronous = true;
    provider.cancelCallbacks = true;
    ServerBrowserModel model({&provider, nullptr});
    int count = 0;
    for (ListKind kind : {ListKind::Internet, ListKind::Favorites, ListKind::History,
             ListKind::LAN, ListKind::Friends, ListKind::Spectate}) {
        CHECK(model.Refresh(kind, DefaultFilters()));
        Drain(model);
        CHECK(provider.lastKind == kind && provider.lastApp == 10);
        CHECK(model.OrderedKeys().size() == 1);
        CHECK(model.GetStatus().phase == Phase::Complete);
        ++count;
        CHECK(provider.released == count - 1);
    }
    CHECK(model.BeginClose());
    CHECK(model.IsQuiescent());
    CHECK(provider.released == 6 && provider.canceled == 6 && provider.requests.empty());
    CHECK(!model.Refresh(ListKind::Internet, DefaultFilters()));
}
void PartialFiltersAndSort() {
    Provider provider;
    auto unanswered = Server(0x80000001U, "No response");
    unanswered.m_bHadSuccessfulResponse = false;
    provider.nextRows = {Server(0x7f000001U, "Z\n\t\177\303\251"), Server(0x7f000002U, "Alpha"), unanswered};
    ServerBrowserModel model({&provider, nullptr});
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    CHECK(!model.QuickRefresh());
    provider.requests.at(provider.current).callback->ServerFailedToRespond(provider.current, 2);
    provider.Complete(eServerFailedToRespond);
    Drain(model);
    CHECK(model.GetStatus().phase == Phase::Failed);
    CHECK(model.OrderedKeys().size() == 3);
    const RowKey key{0x7f000001U, 27015, 27016};
    CHECK(model.Find(key)->name == "Z\303\251");
    model.Select(key);
    auto filters = DefaultFilters();
    filters.nonempty = true;
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 2);
    CHECK(model.Selection() == key);
    model.SetSort({Column::Name, true});
    Drain(model);
    CHECK(model.OrderedKeys().front().ip == 0x7f000002U);
    model.SetSort({Column::Name, false});
    Drain(model);
    CHECK(model.OrderedKeys().front() == key);
    CHECK(model.QuickRefresh(key));
    CHECK(provider.lastIndex == 0);
    CHECK(model.QuickRefresh());
    CHECK(!model.QuickRefresh());
    CHECK(provider.refreshed == 1);
    provider.Complete();
    CHECK(model.GetStatus().error.empty());
    model.Stop();
    CHECK(model.GetStatus().phase == Phase::Stopped && provider.requests.empty());
    CHECK(model.Find(key) != nullptr);
    filters.region = 3;
    CHECK(!model.Refresh(ListKind::LAN, filters));
    CHECK(!model.GetStatus().error.empty());
    filters.region = -1;
    filters.notFull = true;
    CHECK(model.Refresh(ListKind::Internet, filters));
    Drain(model);
    CHECK(provider.lastFilters.count("hasplayers") && provider.lastFilters.count("notfull"));
}
void LargeList() {
    Provider provider;
    for (uint32 index = 0; index < 10000; ++index) {
        provider.nextRows.push_back(Server(0x0a000001U + index));
    }
    ServerBrowserModel model({&provider, nullptr});
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    for (int index = 0; index < 10000; ++index) {
        provider.requests.at(provider.current).callback->ServerResponded(provider.current, index);
    }
    provider.Complete();
    CHECK(model.Pump(11) <= 11);
    CHECK(model.OrderedKeys().size() <= 11);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 10000);
    auto filters = DefaultFilters();
    filters.name = "missing";
    model.SetFilters(filters);
    CHECK(model.GetStatus().viewUpdating);
    CHECK(model.Pump(11) <= 11);
    CHECK(model.GetStatus().viewUpdating);
    Drain(model);
    CHECK(model.OrderedKeys().empty());
    filters.app = 70;
    model.SetFilters(filters);
    CHECK(model.Find({0x0a000001U, 27015, 27016}) == nullptr);
    CHECK(provider.requests.empty());
}
void ReentrantClose() {
    Provider provider;
    provider.nextRows = {Server()};
    provider.cancelCallbacks = true;
    ServerBrowserModel model({&provider, nullptr});
    provider.duringRequest = [&] {
        CHECK(!model.BeginClose()); CHECK(!model.IsQuiescent());
    };
    CHECK(!model.Refresh(ListKind::Internet, DefaultFilters()));
    CHECK(model.BeginClose());
    CHECK(provider.canceled == 1 && provider.released == 1);
    Provider second;
    second.nextRows = {Server()};
    ServerBrowserModel other({&second, nullptr});
    CHECK(other.Refresh(ListKind::Internet, DefaultFilters()));
    second.duringDetails = [&] {
        CHECK(!other.BeginClose());
    };
    other.Pump();
    CHECK(other.BeginClose());
    CHECK(second.released == 1);
}
void ReentrantViewChange() {
    Provider provider;
    provider.nextRows = {Server()};
    ServerBrowserModel model({&provider, nullptr});
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    provider.Complete();
    provider.duringDetails = [&] {
        auto filters = DefaultFilters();
        filters.app = 70;
        model.SetFilters(filters);
        model.SetSort({Column::Ping, false});
    };
    model.Pump();
    CHECK(!provider.requests.empty());
    CHECK(model.Find({0x7f000001U, 27015, 27016}) != nullptr);
    CHECK(!model.GetStatus().error.empty());
    CHECK(model.BeginClose());
}
void FavoritePreservation() {
    Favorites store;
    Address address{0x7f000001U, 27015, 27016};
    store.records = {{70, address, 3, 42}, {10, address, 2, 57}};
    ServerBrowserModel model({nullptr, &store});
    CHECK(model.AddFavorite(10, address));
    CHECK(store.records.size() == 2 && store.records[0].flags == 3 && store.records[0].lastPlayed == 42);
    CHECK(store.records[1].flags == 3 && store.records[1].lastPlayed == 57);
    CHECK(model.RemoveFavorite(10, address));
    CHECK(store.records[1].flags == 2 && store.records[0].flags == 3);
    CHECK(model.RecordAcceptedConnection(10, address, 123));
    CHECK(store.records[1].lastPlayed == 123 && store.records[1].address.queryPort == 27016);
    CHECK(store.records[0].lastPlayed == 42);
    const auto before = std::time(nullptr);
    CHECK(model.AddFavorite(10, {0x7f000002U, 27015, 27016}));
    const auto after = std::time(nullptr);
    CHECK(store.records.back().lastPlayed > 0);
    CHECK(store.records.back().lastPlayed >= before && store.records.back().lastPlayed <= after);
    store.failing = true;
    CHECK(!model.AddFavorite(10, address));
    CHECK(!model.GetStatus().actionError.empty());
    CHECK(model.GetStatus().error.empty());
}
void EverySortAndFilter() {
    Provider provider;
    auto first = Server(0x7f000001U, "Zulu");
    first.m_bPassword = true;
    first.m_bSecure = true;
    first.m_nPlayers = 16;
    first.m_nPing = 90;
    std::strcpy(first.m_szMap, "de_train");
    std::strcpy(first.m_szGameDescription, "Zulu game");
    auto second = Server(0x7f000002U, "Alpha");
    std::strcpy(second.m_szGameDescription, "Alpha game");
    provider.nextRows = {first, second};
    ServerBrowserModel model({&provider, nullptr});
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    provider.Complete();
    Drain(model);
    for (Column column : {Column::Password, Column::Secure, Column::Name, Column::Game,
             Column::Map, Column::Players, Column::Ping, Column::Address}) {
        model.SetSort({column, true});
        Drain(model);
        CHECK(model.OrderedKeys().front().ip == (column == Column::Address ? 0x7f000001U : 0x7f000002U));
        model.SetSort({column, false});
        Drain(model);
        CHECK(model.OrderedKeys().front().ip == (column == Column::Address ? 0x7f000002U : 0x7f000001U));
    }
    auto filters = DefaultFilters();
    filters.secure = Requirement::Yes;
    filters.password = Requirement::Yes;
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 1 && model.OrderedKeys().front().ip == 0x7f000001U);
    filters = DefaultFilters();
    filters.notFull = true;
    filters.maxPing = 50;
    filters.map = "DE_DUST2";
    filters.gameDirectory = "CSTRIKE";
    filters.name = "LPH";
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 1 && model.OrderedKeys().front().ip == 0x7f000002U);
    filters.password = Requirement::No;
    filters.secure = Requirement::No;
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 1);
}
void OriginalMapAndHumanFilters() {
    Provider provider;
    auto human = Server(0x7f000001U);
    std::strcpy(human.m_szMap, "De_Dust2");
    human.m_nPlayers = 3;
    human.m_nBotPlayers = 2;
    auto bots = human;
    bots.m_NetAdr.SetIP(0x7f000002U);
    bots.m_nPlayers = 2;
    provider.nextRows = {human, bots};
    ServerBrowserModel model({&provider, nullptr});
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    provider.Complete();
    Drain(model);
    auto filters = DefaultFilters();
    filters.map = "dE_dU";
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 2);
    filters.nonempty = true;
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 1);
    CHECK(model.OrderedKeys().front().ip == 0x7f000001U);
    filters.map = "dust";
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().empty());
}
void OriginalAccountAndHistoryData() {
    Provider provider;
    auto nonzeroAccount = Server(0x7f000001U);
    nonzeroAccount.m_steamID.SetFromUint64(7); // Invalid type/universe, but original nonzero account predicate passes.
    nonzeroAccount.m_ulTimeLastPlayed = 200;
    nonzeroAccount.m_nBotPlayers = 5;
    auto upperBitsOnly = Server(0x7f000002U);
    upperBitsOnly.m_steamID.SetFromUint64(uint64_t{1} << 56);
    upperBitsOnly.m_bSecure = true;
    upperBitsOnly.m_ulTimeLastPlayed = 300;
    upperBitsOnly.m_nBotPlayers = 1;
    provider.nextRows = {nonzeroAccount, upperBitsOnly};
    ServerBrowserModel model({&provider, nullptr});
    CHECK(model.Refresh(ListKind::History, DefaultFilters()));
    provider.Complete();
    Drain(model);
    CHECK(!nonzeroAccount.m_steamID.IsValid());
    const auto *copied = model.Find({0x7f000001U, 27015, 27016});
    CHECK(copied && copied->hasSteamAccount && copied->lastPlayed == 200 && copied->bots == 5);
    CHECK(!model.Find({0x7f000002U, 27015, 27016})->hasSteamAccount);
    model.SetSort({Column::LastPlayed, false});
    Drain(model);
    CHECK(model.OrderedKeys().front().ip == 0x7f000002U);
    model.SetSort({Column::Bots, false});
    Drain(model);
    CHECK(model.OrderedKeys().front().ip == 0x7f000001U);
    auto filters = DefaultFilters();
    filters.validSteamAccount = true;
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().size() == 1 && model.OrderedKeys().front().ip == 0x7f000001U);
    filters.secure = Requirement::Yes;
    model.SetFilters(filters);
    Drain(model);
    CHECK(model.OrderedKeys().empty()); // VAC cannot substitute for the separate account predicate.
}

void OfflineSavedRowsWithCurrentGameFilters() {
    constexpr size_t RowBudget = 37;
    constexpr int MaximumAcceptedPing = 100;
    struct Game {
        AppId_t app;
        const char* directory;
        const char* otherDirectory;
    };
    for (const Game game : {Game{10, "cstrike", "valve"}, Game{70, "valve", "cstrike"}}) {
        for (const ListKind kind : {ListKind::Favorites, ListKind::History}) {
            Provider provider;
            Favorites store;
            FixtureServer offline;
            offline.m_NetAdr.Init(0x7f000001U, 27016, 27015);
            offline.m_nAppID = game.app;
            offline.SetName("Saved endpoint");
            const Address address{0x7f000001U, 27015, 27016};
            const uint32_t flag = kind == ListKind::Favorites ? k_unFavoriteFlagFavorite : k_unFavoriteFlagHistory;
            store.records = {{game.app, address, flag, 0}};
            auto otherGame = Server(0x7f000002U);
            otherGame.m_nAppID = game.app;
            std::strcpy(otherGame.m_szGameDir, game.otherDirectory);
            auto unresponsiveOtherGame = otherGame;
            unresponsiveOtherGame.m_NetAdr.SetIP(0x7f000003U);
            unresponsiveOtherGame.m_bHadSuccessfulResponse = false;
            auto otherApp = offline;
            otherApp.m_NetAdr.SetIP(0x7f000004U);
            otherApp.m_nAppID = game.app == 10 ? 70 : 10;
            provider.nextRows = {offline, otherGame, unresponsiveOtherGame, otherApp};
            Filters filters;
            filters.app = game.app;
            filters.gameDirectory = game.directory;
            ServerBrowserModel model({&provider, &store});
            CHECK(model.Refresh(kind, filters));
            provider.requests.at(provider.current).callback->ServerFailedToRespond(provider.current, 0);
            provider.Complete();
            model.Pump(RowBudget);
            bool visibleUpdate = false;
            for (const auto& update : model.TakeUpdates(RowBudget)) {
                if (!update.reset && update.row.key == address) {
                    visibleUpdate = update.visible;
                }
            }
            CHECK(visibleUpdate);
            Drain(model);
            CHECK(provider.lastFilters.at("gamedir") == game.directory);
            CHECK(model.OrderedKeys() == std::vector<RowKey>{address});
            CHECK(model.IsVisible(address));
            CHECK(model.Find(address)->failed && !model.Find(address)->responded);
            CHECK(!model.IsVisible({0x7f000002U, 27015, 27016}));
            CHECK(!model.IsVisible({0x7f000003U, 27015, 27016}));
            CHECK(!model.IsVisible({0x7f000004U, 27015, 27016}));
            model.Select(address);
            CHECK(model.Selection() == address);
            provider.failRefresh = true;
            CHECK(model.QuickRefresh(address));
            CHECK(provider.lastIndex == 0);
            Drain(model);
            CHECK(kind == ListKind::Favorites ? model.RemoveFavorite(game.app, address) :
                model.RemoveHistory(game.app, address));
            CHECK((store.records.front().flags & flag) == 0);

            // Unknown metadata must never satisfy an explicit filter, including negative requirements.
            for (const auto& configure : std::vector<std::function<void(Filters&)>>{
                     [](Filters& value) {
                         value.map = "de_";
                     },
                     [](Filters& value) {
                         value.nonempty = true;
                     },
                     [](Filters& value) {
                         value.notFull = true;
                     },
                     [](Filters& value) {
                         value.maxPing = MaximumAcceptedPing;
                     },
                     [](Filters& value) {
                         value.password = Requirement::Yes;
                     },
                     [](Filters& value) {
                         value.password = Requirement::No;
                     },
                     [](Filters& value) {
                         value.secure = Requirement::Yes;
                     },
                     [](Filters& value) {
                         value.secure = Requirement::No;
                     },
                     [](Filters& value) {
                         value.validSteamAccount = true;
                     },
                     [](Filters& value) {
                         value.name = "missing";
                     }}) {
                auto filtered = filters;
                configure(filtered);
                model.SetFilters(filtered);
                Drain(model);
                CHECK(model.OrderedKeys().empty());
                CHECK(!model.IsVisible(address));
            }
            model.SetFilters(filters);
            Drain(model);
            CHECK(model.OrderedKeys() == std::vector<RowKey>{address});
            CHECK(model.IsVisible(address));
        }
    }
    for (const ListKind kind : {ListKind::Internet, ListKind::LAN, ListKind::Friends, ListKind::Spectate}) {
        Provider provider;
        auto unanswered = Server();
        unanswered.m_bHadSuccessfulResponse = false;
        unanswered.m_szGameDir[0] = '\0';
        provider.nextRows = {unanswered};
        auto filters = DefaultFilters();
        filters.gameDirectory = "cstrike";
        ServerBrowserModel model({&provider, nullptr});
        CHECK(model.Refresh(kind, filters));
        provider.Complete();
        Drain(model);
        CHECK(model.OrderedKeys().empty());
        CHECK(!model.IsVisible({0x7f000001U, 27015, 27016}));
    }
}

void FailedSavedRowsRetainSuccessfulMetadata() {
    constexpr int MaximumAcceptedPing = 100;
    for (const ListKind kind : {ListKind::Favorites, ListKind::History}) {
        Provider provider;
        auto responded = Server();
        responded.m_bSecure = true;
        provider.nextRows = {responded};
        auto filters = DefaultFilters();
        filters.gameDirectory = "cstrike";
        filters.map = "de_";
        filters.maxPing = MaximumAcceptedPing;
        filters.secure = Requirement::Yes;
        ServerBrowserModel model({&provider, nullptr});
        CHECK(model.Refresh(kind, filters));
        provider.Complete();
        Drain(model);
        const Address address{0x7f000001U, 27015, 27016};
        CHECK(model.IsVisible(address));
        auto& failed = provider.requests.at(provider.current).rows.front();
        failed.m_bHadSuccessfulResponse = false;
        failed.m_szGameDir[0] = '\0';
        failed.m_szMap[0] = '\0';
        failed.m_bSecure = false;
        provider.failRefresh = true;
        CHECK(model.QuickRefresh(address));
        Drain(model);
        CHECK(model.Find(address)->failed && model.Find(address)->responded);
        CHECK(model.Find(address)->gameDirectory == "cstrike");
        CHECK(model.Find(address)->map == "de_dust2");
        CHECK(model.OrderedKeys() == std::vector<RowKey>{address});
        CHECK(model.IsVisible(address));
    }
}

void ResolvedConnectionPortReplacesDirectoryAddress() {
    constexpr uint32_t ServerIP = 0x7f000001U;
    constexpr uint16_t QueryPort = 27016;
    constexpr uint16_t GamePort = 27015;
    const RowKey directoryAddress{ServerIP, QueryPort, QueryPort};
    const RowKey gameAddress{ServerIP, GamePort, QueryPort};

    for (const bool duplicate : {false, true}) {
        Provider provider;
        auto pending = Server(ServerIP);
        pending.m_NetAdr.Init(ServerIP, QueryPort, QueryPort);
        pending.m_bHadSuccessfulResponse = false;
        provider.nextRows = {pending};
        if (duplicate) {
            provider.nextRows.push_back(pending);
        }
        ServerBrowserModel model({&provider, nullptr});
        CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
        Drain(model);
        CHECK(model.Find(directoryAddress));
        CHECK(model.GetStatus().responding == 0);
        model.Select(directoryAddress);

        auto& request = provider.requests.at(provider.current);
        request.rows.front() = Server(ServerIP);
        request.rows.front().m_bHadSuccessfulResponse = false;
        request.callback->ServerResponded(provider.current, 0);
        provider.Complete();
        Drain(model);
        CHECK(model.GetStatus().rows == provider.nextRows.size());
        CHECK(model.GetStatus().responding == 1);
        CHECK(model.Find(gameAddress));
        CHECK((model.Find(directoryAddress) != nullptr) == duplicate);
        CHECK(model.Selection().has_value() == duplicate);
        CHECK(model.QuickRefresh(directoryAddress) == duplicate);
        if (duplicate) {
            CHECK(provider.lastIndex == 1);
        }
        CHECK(model.QuickRefresh(gameAddress));
        CHECK(provider.lastIndex == 0);
    }
}

void ReplacementAndFailure() {
    Provider provider;
    provider.nextRows = {Server()};
    ServerBrowserModel model({&provider, nullptr});
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    provider.Complete();
    Drain(model);
    provider.nextRows = {Server(0x7f000002U)};
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    CHECK(model.Find({0x7f000001U, 27015, 27016}) != nullptr);
    Drain(model);
    provider.Complete();
    Drain(model);
    CHECK(model.OrderedKeys().size() == 1);
    CHECK(model.Find({0x7f000001U, 27015, 27016}) == nullptr);
    provider.failRequest = true;
    // Replacement is accepted while the old request storage retires; failure is then asynchronous.
    CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
    Drain(model);
    CHECK(model.GetStatus().phase == Phase::Failed && !model.GetStatus().error.empty());
    CHECK(provider.requests.empty());
    ServerBrowserModel unavailable({});
    CHECK(!unavailable.Refresh(ListKind::Internet, DefaultFilters()));
    CHECK(unavailable.GetStatus().phase == Phase::Unavailable);
}

void RefreshPartialSelection() {
    constexpr RowKey Indexed{0x7f000001U, 27015, 27016};
    constexpr RowKey Retained{0x7f000002U, 27015, 27016};
    for (const bool indexedFirst : {true, false}) {
        Provider provider;
        provider.nextRows = {Server(Indexed.ip), Server(Retained.ip)};
        ServerBrowserModel model({&provider, nullptr});
        CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
        provider.Complete();
        Drain(model);

        provider.nextRows = {Server(Indexed.ip)};
        CHECK(model.Refresh(ListKind::Internet, DefaultFilters()));
        Drain(model);
        provider.Complete(eServerFailedToRespond);
        Drain(model);
        provider.deferRefresh = true;
        CHECK(model.QuickRefresh(indexedFirst ? Indexed : Retained));
        CHECK(model.QuickRefresh(indexedFirst ? Retained : Indexed));
        CHECK(provider.lastIndex == 0);
        const auto finishIndexed = [&] {
            provider.requests.at(provider.current).callback->ServerResponded(provider.current, provider.lastIndex);
        };
        if (indexedFirst) {
            finishIndexed();
            provider.Complete(eServerFailedToRespond);
        } else {
            Drain(model);
        }
        CHECK(model.GetStatus().phase == Phase::Refreshing);
        CHECK(!model.QuickRefresh());
        if (!indexedFirst) {
            finishIndexed();
        }
        Drain(model);
        CHECK(model.GetStatus().phase == Phase::Complete);
        CHECK(model.Find(Retained)->failed);
        CHECK(!model.Find(Indexed)->failed);
    }
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "wrong-thread") {
        ServerBrowserModel model({});
        std::thread wrong([&] {
            model.Pump();
        });
        wrong.join();
        return 2;
    }
    if (argc == 2 && std::string(argv[1]) == "wrong-thread-callback") {
        Provider provider;
        ServerBrowserModel model({&provider, nullptr});
        model.Refresh(ListKind::Internet, DefaultFilters());
        std::thread wrong([&] {
            provider.requests.at(provider.current).callback->ServerResponded(provider.current, 0);
        });
        wrong.join();
        return 2;
    }
    if (argc == 2 && std::string(argv[1]) == "live-destruction") {
        Provider provider;
        auto model = std::make_unique<ServerBrowserModel>(SteamServices{&provider, nullptr});
        provider.duringRequest = [&] {
            model.reset();
        };
        model->Refresh(ListKind::Internet, DefaultFilters());
        return 2;
    }

    try {
        OfflineSavedRowsWithCurrentGameFilters();
        FailedSavedRowsRetainSuccessfulMetadata();
        ResolvedConnectionPortReplacesDirectoryAddress();
        OriginalMapAndHumanFilters();
        OriginalAccountAndHistoryData();
        Addresses();
        ListsAndRetirement();
        PartialFiltersAndSort();
        LargeList();
        ReentrantClose();
        ReentrantViewChange();
        FavoritePreservation();
        ReplacementAndFailure();
        RefreshPartialSelection();
        EverySortAndFilter();
        std::cout << "13 model cases passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
