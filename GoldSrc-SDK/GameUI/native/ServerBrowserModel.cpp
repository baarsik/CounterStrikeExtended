#include "ServerBrowserModel.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <map>
#include <set>
#include <thread>
#include <utility>

namespace cse::server_browser {
namespace {
constexpr uint32_t MaximumOctet = 255;
constexpr uint32_t MaximumPort = 65535;
constexpr int BitsPerOctet = 8;
constexpr int MaximumRegion = 7;
constexpr int NoRegion = -1;
constexpr size_t MaximumRefreshPings = 32;

void RequireThread(std::thread::id owner) {
    if (std::this_thread::get_id() != owner) {
        std::abort();
    }
}
bool ValidAddress(Address address) {
    return address.ip != 0 && address.connectionPort != 0 && address.queryPort != 0;
}
std::string Fold(std::string text) {
    for (char& character : text) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character + ('a' - 'A'));
        }
    }
    return text;
}
std::string CopyText(const char* source, size_t bound) {
    std::string result;
    constexpr unsigned char FirstPrintable = 32;
    constexpr unsigned char DeleteCharacter = 127;
    for (size_t index = 0; index < bound && source[index] != '\0'; ++index) {
        const auto character = static_cast<unsigned char>(source[index]);
        if (character >= FirstPrintable && character != DeleteCharacter) {
            result.push_back(source[index]);
        }
    }
    return result;
}
bool Matches(const Row& row, const Filters& filters) {
    if (filters.app != 0 && row.app != filters.app) {
        return false;
    }
    if (!filters.name.empty() && Fold(row.name).find(Fold(filters.name)) == std::string::npos) {
        return false;
    }
    const bool needsResponse = !filters.gameDirectory.empty() || !filters.map.empty() || filters.nonempty ||
        filters.notFull || filters.maxPing > 0 || filters.password != Requirement::Any ||
        filters.secure != Requirement::Any || filters.validSteamAccount;
    if (needsResponse && !row.responded) {
        return false;
    }
    return (filters.gameDirectory.empty() || Fold(filters.gameDirectory) == Fold(row.gameDirectory)) &&
        (filters.map.empty() || Fold(row.map).compare(0, filters.map.size(), Fold(filters.map)) == 0) &&
        (!filters.nonempty || static_cast<int64_t>(row.players) - row.bots > 0) && (!filters.notFull || row.players < row.maxPlayers) &&
        (filters.maxPing <= 0 || (row.ping >= 0 && row.ping <= filters.maxPing)) &&
        (filters.password == Requirement::Any || row.password == (filters.password == Requirement::Yes)) &&
        (filters.secure == Requirement::Any || row.secure == (filters.secure == Requirement::Yes)) &&
        (!filters.validSteamAccount || row.hasSteamAccount);
}
struct RowLess {
    Sort sort;
    bool operator()(const Row& left, const Row& right) const {
        int comparison = 0;
        const auto compare = [](const auto& a, const auto& b) {
            return a < b ? -1 : (b < a ? 1 : 0);
        };
        // Unknown fields sort after known fields regardless of direction.
        if (left.responded != right.responded) {
            return left.responded;
        }
        switch (sort.column) {
            case Column::Password:
                comparison = compare(left.password, right.password);
                break;
            case Column::Secure:
                comparison = compare(left.secure, right.secure);
                break;
            case Column::Name:
                comparison = compare(Fold(left.name), Fold(right.name));
                break;
            case Column::Game:
                comparison = compare(Fold(left.gameDescription), Fold(right.gameDescription));
                break;
            case Column::Map:
                comparison = compare(Fold(left.map), Fold(right.map));
                break;
            case Column::Players:
                comparison = compare(std::max<int64_t>(static_cast<int64_t>(left.players) - left.bots, 0),
                                     std::max<int64_t>(static_cast<int64_t>(right.players) - right.bots, 0));
                if (comparison == 0) {
                    comparison = compare(std::max<int64_t>(static_cast<int64_t>(left.maxPlayers) - left.bots, 0),
                                         std::max<int64_t>(static_cast<int64_t>(right.maxPlayers) - right.bots, 0));
                }
                break;
            case Column::Ping:
                comparison = compare(left.ping, right.ping);
                break;
            case Column::Bots:
                comparison = compare(left.bots, right.bots);
                break;
            case Column::LastPlayed:
                comparison = compare(left.lastPlayed, right.lastPlayed);
                break;
            case Column::Address:
                comparison = compare(std::tie(left.key.ip, left.key.queryPort),
                                     std::tie(right.key.ip, right.key.queryPort));
                break;
        }
        if (comparison == 0) {
            return left.key < right.key;
        }
        return sort.ascending ? comparison < 0 : comparison > 0;
    }
};
Row CopyRow(const gameserveritem_t& item, bool failed) {
    Row row;
    row.key = {item.m_NetAdr.GetIP(), item.m_NetAdr.GetConnectionPort(), item.m_NetAdr.GetQueryPort()};
    row.app = item.m_nAppID;
    row.name = CopyText(item.GetName(), k_cbMaxGameServerName);
    row.gameDirectory = CopyText(item.m_szGameDir, sizeof(item.m_szGameDir));
    row.gameDescription = CopyText(item.m_szGameDescription, sizeof(item.m_szGameDescription));
    row.map = CopyText(item.m_szMap, sizeof(item.m_szMap));
    row.tags = CopyText(item.m_szGameTags, sizeof(item.m_szGameTags));
    row.ping = item.m_nPing;
    row.players = item.m_nPlayers;
    row.maxPlayers = item.m_nMaxPlayers;
    row.bots = item.m_nBotPlayers;
    row.lastPlayed = item.m_ulTimeLastPlayed;
    // Original CheckSecondaryFilters tests the account component, not IsValid() or VAC.
    row.hasSteamAccount = item.m_steamID.GetAccountID() != 0;
    row.password = item.m_bPassword;
    row.secure = item.m_bSecure;
    row.responded = item.m_bHadSuccessfulResponse;
    row.failed = failed;
    return row;
}
}

bool RowPrecedes(const Row& left, const Row& right, Sort sort) {
    return RowLess{sort}(left, right);
}

bool ParseAddress(std::string_view text, Address& address) {
    Address parsed;
    size_t offset = 0;
    for (int part = 0; part < 5; ++part) {
        const size_t start = offset;
        uint32_t value = 0;
        const uint32_t maximum = part == 4 ? MaximumPort : MaximumOctet;
        while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9') {
            value = value * 10 + static_cast<uint32_t>(text[offset] - '0');
            if (value > maximum) {
                return false;
            }
            ++offset;
        }
        if (offset == start) {
            return false;
        }
        if (part == 4) {
            if (offset != text.size() || value == 0) {
                return false;
            }
            parsed.connectionPort = static_cast<uint16_t>(value);
            parsed.queryPort = parsed.connectionPort;
        } else {
            parsed.ip = (parsed.ip << BitsPerOctet) | value;
            if (offset == text.size() || text[offset++] != (part == 3 ? ':' : '.')) {
                return false;
            }
        }
    }
    if (!ValidAddress(parsed)) {
        return false;
    }
    address = parsed;
    return true;
}
std::string ConnectionAddress(Address address) {
    std::string result;
    for (int shift = 3 * BitsPerOctet; shift >= 0; shift -= BitsPerOctet) {
        if (!result.empty()) {
            result += '.';
        }
        result += std::to_string((address.ip >> shift) & MaximumOctet);
    }
    return result + ':' + std::to_string(address.connectionPort);
}

struct SteamFavoriteStore::State {
    ISteamMatchmaking* service;
    std::thread::id thread = std::this_thread::get_id();
};
SteamFavoriteStore::SteamFavoriteStore(ISteamMatchmaking* service) : state_(new State{service}) {}
SteamFavoriteStore::~SteamFavoriteStore() = default;
FavoriteReadResult SteamFavoriteStore::ReadAll() {
    RequireThread(state_->thread);
    if (!state_->service) {
        return {false, {}, "Steam favorites unavailable"};
    }
    FavoriteReadResult result{true, {}, {}};
    const int count = state_->service->GetFavoriteGameCount();
    if (count < 0) {
        return {false, {}, "Invalid favorite count"};
    }
    for (int index = 0; index < count; ++index) {
        FavoriteRecord record;
        if (!state_->service->GetFavoriteGame(index, &record.app, &record.address.ip,
                &record.address.connectionPort, &record.address.queryPort, &record.flags, &record.lastPlayed)) {
            return {false, {}, "Could not read Steam favorites"};
        }
        result.records.push_back(record);
    }
    return result;
}
FavoriteWriteResult SteamFavoriteStore::Add(const FavoriteRecord& record) {
    RequireThread(state_->thread);
    if (!state_->service) {
        return {false, "Steam favorites unavailable"};
    }
    // Valve declares an int but does not document a success sentinel. Confirm the effect instead.
    state_->service->AddFavoriteGame(record.app, record.address.ip, record.address.connectionPort,
        record.address.queryPort, record.flags, record.lastPlayed);
    const auto read = ReadAll();
    if (!read.ok) {
        return {false, read.error};
    }
    for (const auto& stored : read.records) {
        if (stored.app == record.app && stored.address == record.address &&
            (stored.flags & record.flags) == record.flags && stored.lastPlayed == record.lastPlayed) {
            return {true, {}};
        }
    }
    return {false, "Steam favorite/history write was not confirmed"};
}
FavoriteWriteResult SteamFavoriteStore::Remove(const FavoriteRecord& record) {
    RequireThread(state_->thread);
    const bool ok = state_->service && state_->service->RemoveFavoriteGame(record.app, record.address.ip,
        record.address.connectionPort, record.address.queryPort, record.flags);
    return {ok, ok ? "" : "Could not remove Steam favorite flag"};
}

struct ServerBrowserModel::State {
    using Index = std::set<Row, RowLess>;
    using Rows = std::map<RowKey, Row>;
    struct Ping final : ISteamMatchmakingPingResponse {
        State& owner;
        Row original;
        HServerQuery handle = HSERVERQUERY_INVALID;
        std::optional<Row> result;
        bool complete = false;
        bool accepting = true;
        Ping(State& state, const Row& row) : owner(state), original(row) {}
        void ServerResponded(gameserveritem_t& item) override {
            owner.Check();
            ++owner.callbackDepth;
            if (accepting && !owner.closing && !complete) {
                Row row = CopyRow(item, false);
                if (row.key.ip == original.key.ip && row.key.connectionPort == original.key.connectionPort) {
                    row.key = original.key;
                    row.lastPlayed = original.lastPlayed;
                    result = std::move(row);
                } else {
                    result = original;
                    result->failed = true;
                }
                complete = true;
            }
            --owner.callbackDepth;
        }
        void ServerFailedToRespond() override {
            owner.Check();
            ++owner.callbackDepth;
            if (accepting && !owner.closing && !complete) {
                result = original;
                result->failed = true;
                complete = true;
            }
            --owner.callbackDepth;
        }
    };
    struct Request final : ISteamMatchmakingServerListResponse {
        State& owner;
        uint64_t generation;
        HServerListRequest handle = nullptr;
        bool accepting = true;
        bool completed = false;
        std::set<int> quickPending;
        int enumerate = 0;
        int count = 0;
        std::map<int, bool> dirty;
        std::set<int> seen;
        std::set<RowKey> membership;
        bool prunePending = true;
        Rows::iterator pruneCursor;
        std::vector<MatchMakingKeyValuePair_t> filters;
        std::vector<MatchMakingKeyValuePair_t*> filterPointers;
        Request(State& state, uint64_t value) : owner(state), generation(value) {}
        bool Accept(HServerListRequest incoming) {
            owner.Check();
            if (!accepting || owner.closing || generation != owner.generation) {
                return false;
            }
            if (!handle) {
                handle = incoming;
            }
            return incoming && incoming == handle;
        }
        void Response(HServerListRequest incoming, int index, bool failed) {
            owner.Check();
            ++owner.callbackDepth;
            if (Accept(incoming) && index >= 0) {
                dirty[index] = failed;
                if (quickPending.erase(index) && quickPending.empty()) {
                    completed = true;
                    owner.status.phase = Phase::Complete;
                }
            }
            --owner.callbackDepth;
        }
        void ServerResponded(HServerListRequest incoming, int index) override {
            Response(incoming, index, false);
        }
        void ServerFailedToRespond(HServerListRequest incoming, int index) override {
            Response(incoming, index, true);
        }
        void RefreshComplete(HServerListRequest incoming, EMatchMakingServerResponse result) override {
            owner.Check();
            ++owner.callbackDepth;
            if (Accept(incoming)) {
                completed = true;
                owner.status.completion = result;
                owner.status.error.clear();
                owner.status.phase = result == eServerFailedToRespond ? Phase::Failed : Phase::Complete;
                if (result == eServerFailedToRespond) {
                    owner.status.error = "Server query failed; partial results remain available";
                }
            }
            --owner.callbackDepth;
        }
    };
    SteamServices services;
    std::thread::id thread = std::this_thread::get_id();
    uint64_t generation = 0;
    size_t providerDepth = 0;
    size_t callbackDepth = 0;
    bool closing = false;
    bool retiring = false;
    bool retirePending = false;
    ListKind kind = ListKind::Internet;
    Filters filters;
    Sort sort;
    Status status;
    std::unique_ptr<Request> request;
    std::unique_ptr<Request> retiredRequest;
    std::set<RowKey> pingPending;
    std::map<RowKey, std::unique_ptr<Ping>> pings;
    bool pingRefresh = false;
    bool refreshPending = false;
    bool resetPending = false;
    bool rebuildPending = false;
    size_t lastReclamationWork = 0;
    Rows rows;
    std::map<RowKey, int> indexes;
    std::unique_ptr<Index> ordered{new Index(RowLess{})};
    std::unique_ptr<Index> rebuilding;
    Rows::iterator rebuildCursor;
    std::unique_ptr<Index> retiredIndex;
    std::map<RowKey, RowUpdate> updates;
    bool resetUpdate = false;
    std::optional<RowKey> selection;

    explicit State(SteamServices value) : services(value) {
        status.phase = services.servers ? Phase::Idle : Phase::Unavailable;
    }
    void Check() const {
        RequireThread(thread);
    }
    template<class Function> auto Provider(Function function) {
        Check();
        struct Guard {
            size_t& depth;
            explicit Guard(size_t& value) : depth(value) {
                ++depth;
            }
            ~Guard() {
                --depth;
            }
        } guard(providerDepth);
        return function();
    }
    size_t StorageNodes() const {
        const auto requestNodes = [](const std::unique_ptr<Request>& owner) {
            return owner ? owner->dirty.size() + owner->seen.size() + owner->membership.size() +
                owner->filters.size() + owner->filterPointers.size() + owner->quickPending.size() : size_t{0};
        };
        return rows.size() + indexes.size() + updates.size() + (ordered ? ordered->size() : 0) +
            (rebuilding ? rebuilding->size() : 0) + (retiredIndex ? retiredIndex->size() : 0) +
            requestNodes(request) + requestNodes(retiredRequest) + pingPending.size() + pings.size();
    }
    void Retire() {
        if (!retirePending || providerDepth || callbackDepth || retiring) {
            return;
        }
        retiring = true;
        for (auto& entry : pings) {
            auto& ping = *entry.second;
            ping.accepting = false;
            if (ping.handle != HSERVERQUERY_INVALID) {
                Provider([&] {
                    services.servers->CancelServerQuery(ping.handle);
                });
            }
        }
        pings.clear();
        if (request && request->handle) {
            request->accepting = false;
            const auto handle = request->handle;
            Provider([&] {
                services.servers->CancelQuery(handle);
            });
            Provider([&] {
                services.servers->ReleaseRequest(handle);
            });
            request->handle = nullptr;
        }
        if (request) {
            // Replacement waits for this single retired generation; never accumulate callback owners.
            if (retiredRequest) {
                std::abort();
            }
            retiredRequest = std::move(request);
        }
        retirePending = false;
        retiring = false;
        if (closing) {
            status.phase = Phase::Closed;
        }
    }
    void Invalidate() {
        ++generation;
        pingRefresh = false;
        for (auto& entry : pings) {
            entry.second->accepting = false;
        }
        if (request) {
            request->accepting = false;
        }
        retirePending = true;
        Retire();
    }
    void ResetRows() {
        resetPending = true;
        rebuildPending = true;
        selection.reset();
        resetUpdate = true;
        status.failed = 0;
    }
    void Rebuild() {
        rebuildPending = true;
    }
    void StartRebuild() {
        if (!rebuildPending || rebuilding || retiredIndex || resetPending || closing) {
            return;
        }
        rebuilding.reset(new Index(RowLess{sort}));
        rebuildCursor = rows.begin();
        rebuildPending = false;
    }
    void NormalizeEmptyRoots() {
        if (retiredRequest && retiredRequest->dirty.empty() && retiredRequest->seen.empty() &&
            retiredRequest->membership.empty() && retiredRequest->filters.empty() &&
            retiredRequest->filterPointers.empty() && retiredRequest->quickPending.empty() && indexes.empty()) {
            retiredRequest.reset();
        }
        if (retiredIndex && retiredIndex->empty()) {
            retiredIndex.reset();
        }
        if ((closing || resetPending) && indexes.empty() && updates.empty() &&
            (!ordered || ordered->empty()) && (!rebuilding || rebuilding->empty()) && rows.empty()) {
            rebuilding.reset();
            resetPending = false;
        }
    }
    size_t Reclaim(size_t budget) {
        lastReclamationWork = 0;
        if (providerDepth || callbackDepth || retiring) {
            return 0;
        }
        const auto eraseOne = [&](auto& container) {
            if (container.empty()) {
                return false;
            }
            container.erase(container.begin());
            ++lastReclamationWork;
            return true;
        };
        while (lastReclamationWork < budget) {
            if (!pingRefresh && eraseOne(pingPending)) {
                continue;
            }
            if (retiredRequest) {
                auto& owner = *retiredRequest;
                if (eraseOne(owner.dirty) || eraseOne(owner.seen) || eraseOne(owner.membership) ||
                    eraseOne(owner.quickPending)) {
                    continue;
                }
                if (!owner.filterPointers.empty()) {
                    owner.filterPointers.pop_back();
                    ++lastReclamationWork;
                    continue;
                }
                if (!owner.filters.empty()) {
                    owner.filters.pop_back();
                    ++lastReclamationWork;
                    continue;
                }
                if (eraseOne(indexes)) {
                    continue;
                }
                retiredRequest.reset();
            }
            if (retiredIndex) {
                if (eraseOne(*retiredIndex)) {
                    continue;
                }
                retiredIndex.reset();
            }
            if (closing || resetPending) {
                // Index/value copies are retired before their backing row map.
                if (eraseOne(indexes) || eraseOne(updates) || (ordered && eraseOne(*ordered)) ||
                    (rebuilding && eraseOne(*rebuilding)) || eraseOne(rows)) {
                    continue;
                }
                rebuilding.reset();
                resetPending = false;
                if (!closing) {
                    ordered.reset(new Index(RowLess{sort}));
                }
            }
            break;
        }
        NormalizeEmptyRoots();
        return lastReclamationWork;
    }
    bool CanStartRequest() const {
        return refreshPending && !request && !retiredRequest && indexes.empty() && !resetPending && !closing;
    }
    bool StartRequest();
    void Queue(const Row& row) {
        updates[row.key] = {row, Matches(row, filters), false, generation};
    }
    void Apply(Row row, std::optional<int> index = std::nullopt) {
        if (!ValidAddress(row.key)) {
            return;
        }
        auto old = rows.find(row.key);
        if (old != rows.end()) {
            ordered->erase(old->second);
            if (rebuilding) {
                rebuilding->erase(old->second);
            }
            if (old->second.failed) {
                --status.failed;
            }
            if (!row.responded && old->second.responded) {
                const bool failed = row.failed;
                row = old->second;
                row.failed = failed;
            }
        }
        rows[row.key] = row;
        if (index && request) {
            indexes[row.key] = *index;
            request->membership.insert(row.key);
        }
        if (row.failed) {
            ++status.failed;
        }
        if (Matches(row, filters)) {
            ordered->insert(row);
            if (rebuilding) {
                rebuilding->insert(row);
            }
        }
        Queue(row);
        if (selection == row.key && !Matches(row, filters)) {
            selection.reset();
        }
    }
    size_t PumpPings(size_t budget) {
        size_t work = 0;
        auto entry = pings.begin();
        while (entry != pings.end() && work < budget && pingRefresh && !closing) {
            auto& ping = *entry->second;
            if (!ping.complete) {
                ++entry;
                continue;
            }
            if (ping.result) {
                Apply(std::move(*ping.result));
            }
            ping.accepting = false;
            if (ping.handle != HSERVERQUERY_INVALID) {
                Provider([&] {
                    services.servers->CancelServerQuery(ping.handle);
                });
            }
            entry = pings.erase(entry);
            ++work;
        }
        while (work < budget && pingRefresh && !closing && !pingPending.empty() &&
               pings.size() < MaximumRefreshPings) {
            const RowKey key = *pingPending.begin();
            pingPending.erase(pingPending.begin());
            ++work;
            const auto row = rows.find(key);
            if (row == rows.end()) {
                continue;
            }
            auto query = std::make_unique<Ping>(*this, row->second);
            auto& ping = *query;
            pings.emplace(key, std::move(query));
            ping.handle = Provider([&] {
                return services.servers->PingServer(key.ip, key.queryPort, &ping);
            });
            if (ping.handle == HSERVERQUERY_INVALID && !ping.complete) {
                ping.ServerFailedToRespond();
            }
        }
        if (pingRefresh && pingPending.empty() && pings.empty()) {
            pingRefresh = false;
            status.phase = Phase::Complete;
        }
        return work;
    }
    bool Favorite(AppId_t app, Address address, uint32_t flag, uint32_t timestamp, bool remove) {
        Check();
        if (closing || app == 0 || !ValidAddress(address) || !services.favorites) {
            status.actionError = "Favorite/history service or address unavailable";
            return false;
        }
        const auto read = Provider([&] {
            return services.favorites->ReadAll();
        });
        if (!read.ok) {
            status.actionError = read.error.empty() ? "Favorite/history read failed" : read.error;
            return false;
        }
        if (closing) {
            status.actionError = "Favorite/history action interrupted by shutdown";
            Retire();
            return false;
        }
        FavoriteRecord change{app, address, flag, timestamp};
        bool found = false;
        bool existingRecord = false;
        for (const auto& existing : read.records) {
            if (existing.app == app && existing.address == address) {
                existingRecord = true;
                found = (existing.flags & flag) != 0;
                if (!remove) {
                    change.flags |= existing.flags;
                }
                if (flag == k_unFavoriteFlagFavorite) {
                    change.lastPlayed = existing.lastPlayed;
                }
                break;
            }
        }
        if (!remove && !existingRecord && flag == k_unFavoriteFlagFavorite) {
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            constexpr uint64_t MaximumSteamTimestamp = UINT32_MAX;
            if (now < 0 || static_cast<uint64_t>(now) > MaximumSteamTimestamp) {
                status.actionError = "Current time cannot be represented by Steam favorites";
                return false;
            }
            change.lastPlayed = static_cast<uint32_t>(now);
        }
        if ((remove && !found) || (!remove && found && flag == k_unFavoriteFlagFavorite)) {
            status.actionError.clear();
            return true;
        }
        if (closing) {
            status.actionError = "Favorite/history action interrupted by shutdown";
            Retire();
            return false;
        }
        const auto write = Provider([&] {
            return remove ? services.favorites->Remove(change) : services.favorites->Add(change);
        });
        status.actionError = write.ok ? std::string{} :
            (write.error.empty() ? "Favorite/history write failed" : write.error);
        Retire();
        return write.ok;
    }
};

bool ServerBrowserModel::State::StartRequest() {
    auto& state = *this;
    const ListKind listKind = state.kind;
    const Filters& listFilters = state.filters;
    refreshPending = false;
    state.request.reset(new State::Request(state, state.generation));
    auto& listRequest = *state.request;
    listRequest.pruneCursor = state.rows.begin();
    const auto add = [&](const char* key, const std::string& value) {
        listRequest.filters.emplace_back(key, value.c_str());
    };
    if (!listFilters.gameDirectory.empty()) {
        add("gamedir", listFilters.gameDirectory);
    }
    // Map is a case-insensitive prefix applied locally, not the provider exact-map key.
    if (listFilters.nonempty) {
        add("hasplayers", "1");
    }
    if (listFilters.notFull) {
        add("notfull", "1");
    }
    if (listFilters.secure == Requirement::Yes) {
        add("secure", "1");
    }
    if (listFilters.region != NoRegion) {
        add("region", std::to_string(listFilters.region));
    }
    for (auto& filter : listRequest.filters) {
        listRequest.filterPointers.push_back(&filter);
    }
    const auto count = static_cast<uint32>(listRequest.filterPointers.size());
    auto** pairs = listRequest.filterPointers.empty() ? nullptr : listRequest.filterPointers.data();
    const auto handle = state.Provider([&] {
        switch (listKind) {
            case ListKind::Internet:
                return state.services.servers->RequestInternetServerList(listFilters.app, pairs, count, &listRequest);
            case ListKind::Favorites:
                return state.services.servers->RequestFavoritesServerList(listFilters.app, pairs, count, &listRequest);
            case ListKind::History:
                return state.services.servers->RequestHistoryServerList(listFilters.app, pairs, count, &listRequest);
            case ListKind::LAN:
                return state.services.servers->RequestLANServerList(listFilters.app, &listRequest);
            case ListKind::Friends:
                return state.services.servers->RequestFriendsServerList(listFilters.app, pairs, count, &listRequest);
            case ListKind::Spectate:
                return state.services.servers->RequestSpectatorServerList(listFilters.app, pairs, count, &listRequest);
        }
        return static_cast<HServerListRequest>(nullptr);
    });
    if (listRequest.handle && listRequest.handle != handle) {
        // A synchronous callback with a different handle violates the pinned provider contract.
        std::abort();
    }
    listRequest.handle = handle;
    if (!handle && !state.closing) {
        state.status.phase = Phase::Failed;
        state.status.error = "Steam did not create a server list request";
        state.Invalidate();
    }
    state.Retire();
    return handle && !state.closing;
}

ServerBrowserModel::ServerBrowserModel(SteamServices services) : state_(new State(services)) {}
ServerBrowserModel::~ServerBrowserModel() {
    if (!BeginClose()) {
        std::abort();
    }
    // Convenience for unused/test owners only. Production uses CloseStep before destruction.
    constexpr size_t DestructionBatch = 128;
    while (!CloseStep(DestructionBatch)) {
    }
}
bool ServerBrowserModel::Refresh(ListKind kind, const Filters& filters) {
    auto& state = *state_;
    state.Check();
    if (state.closing || !state.services.servers || state.providerDepth || state.callbackDepth) {
        return false;
    }
    if (filters.region < NoRegion || filters.region > MaximumRegion ||
        (filters.region != NoRegion && kind != ListKind::Internet && kind != ListKind::Spectate)) {
        state.status.error = "Region filtering is unavailable for this list";
        return false;
    }
    state.Invalidate();
    if (state.kind != kind || state.filters.app != filters.app) {
        state.ResetRows();
    }
    state.kind = kind;
    state.filters = filters;
    state.Rebuild();
    state.status.phase = Phase::Refreshing;
    state.status.error.clear();
    state.refreshPending = true;
    state.NormalizeEmptyRoots();
    return !state.CanStartRequest() || state.StartRequest();
}

bool ServerBrowserModel::QuickRefresh() {
    auto& state = *state_;
    state.Check();
    if (state.closing || !state.services.servers || state.refreshPending || state.resetPending ||
        state.providerDepth || state.callbackDepth || state.retiring) {
        return false;
    }
    if (!state.request || !state.request->handle) {
        if (state.rows.empty()) {
            return false;
        }
        if (!state.pingRefresh) {
            state.pingPending.clear();
        }
        for (const auto& entry : state.rows) {
            if (!state.pings.count(entry.first)) {
                state.pingPending.insert(entry.first);
            }
        }
        state.pingRefresh = true;
        state.status.phase = Phase::Refreshing;
        state.status.error.clear();
        return true;
    }
    if (!state.request->completed) {
        return false;
    }
    auto& request = *state.request;
    request.completed = false;
    state.status.phase = Phase::Refreshing;
    state.status.error.clear();
    state.Provider([&] {
        state.services.servers->RefreshQuery(request.handle);
    });
    state.Retire();
    return !state.closing;
}
bool ServerBrowserModel::QuickRefresh(const RowKey& key) {
    auto& state = *state_;
    state.Check();
    const auto found = state.indexes.find(key);
    if (state.closing || !state.services.servers || state.refreshPending || state.resetPending ||
        state.providerDepth || state.callbackDepth || state.retiring) {
        return false;
    }
    if (!state.request || !state.request->handle) {
        if (!Find(key)) {
            return false;
        }
        if (!state.pingRefresh) {
            state.pingPending.clear();
        }
        if (!state.pings.count(key)) {
            state.pingPending.insert(key);
        }
        state.pingRefresh = true;
        state.status.phase = Phase::Refreshing;
        state.status.error.clear();
        return true;
    }
    if ((!state.request->completed && state.request->quickPending.empty()) || found == state.indexes.end()) {
        return false;
    }
    auto& request = *state.request;
    // Selected refreshes share a list handle and may finish in any order. Track each
    // outstanding index so a preceding asynchronous response cannot block the next row.
    if (!request.quickPending.insert(found->second).second) {
        return true;
    }
    request.completed = false;
    state.status.phase = Phase::Refreshing;
    state.status.error.clear();
    state.Provider([&] {
        state.services.servers->RefreshServer(request.handle, found->second);
    });
    state.Retire();
    return !state.closing;
}
void ServerBrowserModel::Stop() {
    auto& state = *state_;
    state.Check();
    if (!state.closing) {
        state.status.phase = Phase::Stopped;
        state.refreshPending = false;
        state.Invalidate();
    }
}
void ServerBrowserModel::SetFilters(const Filters& filters) {
    auto& state = *state_;
    state.Check();
    if (state.providerDepth || state.callbackDepth) {
        state.status.error = "View changes require returning from the provider callback";
        return;
    }
    if (state.closing) {
        return;
    }
    if (filters.app != state.filters.app) {
        Stop();
        state.ResetRows();
    }
    if (filters.region != state.filters.region || filters.gameDirectory != state.filters.gameDirectory ||
        (state.filters.nonempty && !filters.nonempty) ||
        (state.filters.notFull && !filters.notFull) ||
        (state.filters.secure == Requirement::Yes && filters.secure != Requirement::Yes)) {
        state.status.error = "Provider filter changes require a full refresh for complete membership";
    }
    state.filters = filters;
    if (state.selection && !IsVisible(*state.selection)) {
        state.selection.reset();
    }
    state.Rebuild();
}
void ServerBrowserModel::SetSort(Sort sort) {
    state_->Check();
    if (state_->providerDepth || state_->callbackDepth) {
        state_->status.error = "View changes require returning from the provider callback";
        return;
    }
    if (!state_->closing) {
        state_->sort = sort;
        state_->Rebuild();
    }
}
void ServerBrowserModel::Select(std::optional<RowKey> key) {
    state_->Check();
    state_->selection = key && IsVisible(*key) ? key : std::nullopt;
}
std::optional<RowKey> ServerBrowserModel::Selection() const {
    state_->Check();
    return state_->selection;
}
size_t ServerBrowserModel::Pump(size_t rowBudget) {
    auto& state = *state_;
    state.Check();
    state.Retire();
    if (state.closing || state.providerDepth || state.callbackDepth) {
        return 0;
    }
    size_t work = state.Reclaim(rowBudget);
    work += state.PumpPings(rowBudget - work);
    if (state.CanStartRequest()) {
        state.StartRequest();
    }
    state.StartRebuild();
    if (state.request && state.request->handle) {
        auto& request = *state.request;
        request.count = std::max(0, state.Provider([&] {
            return state.services.servers->GetServerCount(request.handle);
        }));
    }
    while (work < rowBudget && !state.closing && !state.resetPending) {
        if (state.request && (!state.request->dirty.empty() || state.request->enumerate < state.request->count)) {
            auto& request = *state.request;
            int index = 0;
            bool failed = false;
            bool read = true;
            if (!request.dirty.empty()) {
                const auto item = request.dirty.begin();
                index = item->first;
                failed = item->second;
                request.dirty.erase(item);
            } else {
                index = request.enumerate++;
                read = request.seen.count(index) == 0;
            }
            ++work;
            if (read && index < request.count) {
                const gameserveritem_t* item = state.Provider([&] {
                    return state.services.servers->GetServerDetails(request.handle, index);
                });
                if (!state.closing && request.accepting && item) {
                    state.Apply(CopyRow(*item, failed), index);
                    request.seen.insert(index);
                }
            }
        } else if (state.request && state.request->completed && state.request->prunePending) {
            auto& request = *state.request;
            if (state.status.completion == eServerFailedToRespond || request.pruneCursor == state.rows.end()) {
                request.prunePending = false;
            } else {
                const auto entry = request.pruneCursor++;
                if (!request.membership.count(entry->first)) {
                    state.ordered->erase(entry->second);
                    if (state.rebuilding) {
                        state.rebuilding->erase(entry->second);
                        if (state.rebuildCursor == entry) {
                            ++state.rebuildCursor;
                        }
                    }
                    state.updates[entry->first] = {entry->second, false, false, state.generation};
                    if (state.selection == entry->first) {
                        state.selection.reset();
                    }
                    if (entry->second.failed) {
                        --state.status.failed;
                    }
                    state.indexes.erase(entry->first);
                    state.rows.erase(entry);
                }
                ++work;
            }
        } else if (state.rebuilding) {
            if (state.rebuildCursor != state.rows.end()) {
                const auto& row = (state.rebuildCursor++)->second;
                if (Matches(row, state.filters)) {
                    state.rebuilding->insert(row);
                }
                state.Queue(row);
                ++work;
            } else {
                state.retiredIndex = std::move(state.ordered);
                state.ordered = std::move(state.rebuilding);
                break;
            }
        } else {
            break;
        }
    }
    state.Retire();
    return work;
}
std::vector<RowUpdate> ServerBrowserModel::TakeUpdates(size_t maxUpdates) {
    state_->Check();
    std::vector<RowUpdate> result;
    if (maxUpdates && state_->resetUpdate) {
        result.push_back({{}, false, true, state_->generation});
        state_->resetUpdate = false;
    }
    if (state_->closing || state_->resetPending) {
        return result;
    }
    while (result.size() < maxUpdates && !state_->updates.empty()) {
        const auto entry = state_->updates.begin();
        result.push_back(std::move(entry->second));
        state_->updates.erase(entry);
    }
    return result;
}
const Row* ServerBrowserModel::Find(RowKey key) const {
    state_->Check();
    if (state_->closing || state_->resetPending) {
        return nullptr;
    }
    const auto found = state_->rows.find(key);
    return found == state_->rows.end() ? nullptr : &found->second;
}
bool ServerBrowserModel::IsVisible(RowKey key) const {
    const Row *row = Find(key);
    return row && Matches(*row, state_->filters);
}
std::vector<RowKey> ServerBrowserModel::OrderedKeys() const {
    state_->Check();
    std::vector<RowKey> keys;
    if (state_->closing || state_->resetPending) {
        return keys;
    }
    for (const auto& row : *state_->ordered) {
        keys.push_back(row.key);
    }
    return keys;
}
Status ServerBrowserModel::GetStatus() const {
    state_->Check();
    Status result = state_->status;
    result.kind = state_->kind;
    result.requestRetired = !state_->request && !state_->retiredRequest && !state_->providerDepth &&
        !state_->callbackDepth && !state_->retirePending && !state_->retiring && !state_->refreshPending &&
        state_->pings.empty() && state_->pingPending.empty();
    result.rows = state_->rows.size();
    result.retainedNodes = state_->StorageNodes();
    result.retainedGenerations = (state_->retiredRequest ? 1 : 0) + (state_->retiredIndex ? 1 : 0) +
        (state_->rebuilding ? 1 : 0);
    result.lastReclamationWork = state_->lastReclamationWork;
    result.viewUpdating = state_->refreshPending || state_->resetPending || state_->rebuildPending ||
        state_->rebuilding != nullptr ||
        (state_->request && state_->request->completed && state_->request->prunePending);
    result.pending = state_->updates.size() + (state_->resetUpdate ? 1 : 0);
    result.pending += state_->pingPending.size() + state_->pings.size();
    if (state_->request) {
        result.pending += state_->request->dirty.size();
        result.pending += static_cast<size_t>(std::max(0, state_->request->count - state_->request->enumerate));
    }
    return result;
}
Row CopyServerRow(const gameserveritem_t &item) {
    return CopyRow(item, false);
}

bool ServerBrowserModel::RemoveHistory(AppId_t app, RowKey key) {
    return state_->Favorite(app, key, k_unFavoriteFlagHistory, 0, true);
}

bool ServerBrowserModel::AddFavorite(AppId_t app, Address address) {
    return state_->Favorite(app, address, k_unFavoriteFlagFavorite, 0, false);
}
bool ServerBrowserModel::RemoveFavorite(AppId_t app, RowKey key) {
    return state_->Favorite(app, key, k_unFavoriteFlagFavorite, 0, true);
}
bool ServerBrowserModel::RecordAcceptedConnection(AppId_t app, Address address, uint32_t unixTime) {
    return state_->Favorite(app, address, k_unFavoriteFlagHistory, unixTime, false);
}
bool ServerBrowserModel::BeginClose() {
    auto& state = *state_;
    state.Check();
    if (!state.closing) {
        state.closing = true;
        state.status.phase = Phase::Closing;
        state.refreshPending = false;
        state.rebuildPending = false;
        state.selection.reset();
        state.resetUpdate = false;
        state.Invalidate();
    }
    state.Retire();
    return IsQuiescent();
}
bool ServerBrowserModel::IsQuiescent() const {
    state_->Check();
    return state_->closing && !state_->request && !state_->providerDepth && !state_->callbackDepth &&
        !state_->retirePending && !state_->retiring;
}
bool ServerBrowserModel::CloseStep(size_t nodeBudget) {
    if (!BeginClose()) {
        return false;
    }
    state_->Reclaim(nodeBudget);
    return state_->StorageNodes() == 0 && !state_->retiredRequest && !state_->retiredIndex &&
        !state_->rebuilding && !state_->resetPending;
}

}
