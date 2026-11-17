#ifndef CSE_SERVER_BROWSER_MODEL_H
#define CSE_SERVER_BROWSER_MODEL_H

#include "../../external/steamworks/isteammatchmaking.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace cse::server_browser {
enum class ListKind { Internet, Favorites, History, LAN, Friends, Spectate };
enum class Requirement { Any, Yes, No };
enum class Column { Password, Secure, Name, Game, Map, Players, Ping, Address, Bots, LastPlayed };
enum class Phase { Unavailable, Idle, Refreshing, Complete, Stopped, Failed, Closing, Closed };

// All fields are host order: 127.0.0.1 == 0x7f000001. Never conflate the two ports.
struct Address {
    uint32_t ip = 0;
    uint16_t connectionPort = 0;
    uint16_t queryPort = 0;
    bool operator==(const Address& other) const {
        return std::tie(ip, connectionPort, queryPort) ==
            std::tie(other.ip, other.connectionPort, other.queryPort);
    }
    bool operator<(const Address& other) const {
        return std::tie(ip, connectionPort, queryPort) <
            std::tie(other.ip, other.connectionPort, other.queryPort);
    }
};
using RowKey = Address;
bool ParseAddress(std::string_view text, Address& address);
std::string ConnectionAddress(Address address);
bool GameDirectoriesMatch(std::string_view left, std::string_view right);

struct Filters {
    AppId_t app = 0;
    std::string gameDirectory;
    std::string map;
    std::string name;
    bool nonempty = false;
    bool notFull = false;
    Requirement password = Requirement::Any;
    Requirement secure = Requirement::Any;
    int maxPing = 0;
    int region = -1;
    bool validSteamAccount = false;
};
struct Sort {
    Column column = Column::Name;
    bool ascending = true;
    // Changing primary columns retains the previous comparator for this session only.
    std::optional<Column> secondaryColumn = std::nullopt;
    bool secondaryAscending = true;
    bool operator==(const Sort& other) const {
        return std::tie(column, ascending, secondaryColumn, secondaryAscending) ==
            std::tie(other.column, other.ascending, other.secondaryColumn, other.secondaryAscending);
    }
};
struct Row {
    RowKey key;
    AppId_t app = 0;
    std::string name;
    std::string gameDirectory;
    std::string gameDescription;
    std::string map;
    std::string tags;
    int ping = 0;
    int players = 0;
    int maxPlayers = 0;
    int bots = 0;
    bool password = false;
    bool secure = false;
    bool responded = false;
    bool failed = false;
    uint32_t lastPlayed = 0;
    bool hasSteamAccount = false;
};
Row CopyServerRow(const gameserveritem_t &item);

struct RowUpdate {
    Row row;
    bool visible = false;
    bool reset = false;
    uint64_t generation = 0;
};
// Shared canonical ordering for the model and its bounded, viewport-only presentation cache.
bool RowPrecedes(const Row& left, const Row& right, Sort sort);
struct Status {
    Phase phase = Phase::Unavailable;
    ListKind kind = ListKind::Internet;
    // Ordinary Stop also retires request storage; copied rows may remain usable.
    bool requestRetired = true;
    std::optional<EMatchMakingServerResponse> completion;
    size_t rows = 0;
    size_t responding = 0;
    size_t failed = 0;
    size_t pending = 0;
    bool viewUpdating = false;
    size_t retainedNodes = 0;
    size_t retainedGenerations = 0;
    size_t lastReclamationWork = 0;
    std::string error;
    std::string actionError;
};
struct FavoriteRecord {
    AppId_t app = 0;
    Address address;
    uint32_t flags = 0;
    uint32_t lastPlayed = 0;
};
struct FavoriteReadResult {
    bool ok = false;
    std::vector<FavoriteRecord> records;
    std::string error;
};
struct FavoriteWriteResult {
    bool ok = false;
    std::string error;
};
class IFavoriteStore {
public:
    virtual ~IFavoriteStore() = default;
    virtual FavoriteReadResult ReadAll() = 0;
    virtual FavoriteWriteResult Add(const FavoriteRecord& record) = 0;
    virtual FavoriteWriteResult Remove(const FavoriteRecord& record) = 0;
};
class SteamFavoriteStore final : public IFavoriteStore {
public:
    explicit SteamFavoriteStore(ISteamMatchmaking* service);
    ~SteamFavoriteStore() override;
    FavoriteReadResult ReadAll() override;
    FavoriteWriteResult Add(const FavoriteRecord& record) override;
    FavoriteWriteResult Remove(const FavoriteRecord& record) override;
private:
    struct State;
    std::unique_ptr<State> state_;
};
struct SteamServices {
    ISteamMatchmakingServers* servers = nullptr;
    IFavoriteStore* favorites = nullptr;
};

// Private owner. Construct on the main thread; every call and callback is thread-affine.
// Borrowed services and this image remain alive until CloseStep completes.
class ServerBrowserModel final {
public:
    explicit ServerBrowserModel(SteamServices services);
    ~ServerBrowserModel();
    ServerBrowserModel(const ServerBrowserModel&) = delete;
    ServerBrowserModel& operator=(const ServerBrowserModel&) = delete;
    // True accepts the latest intent. Replacements may start later as Pump retires the previous storage.
    bool Refresh(ListKind kind, const Filters& filters, bool clearResults = false);
    bool QuickRefresh();
    bool QuickRefresh(const RowKey& key);
    void Stop();
    void ClearResults();
    void SwapState(ServerBrowserModel& other);
    void SetFilters(const Filters& filters);
    void SetSort(Sort sort);
    void Select(std::optional<RowKey> key);
    std::optional<RowKey> Selection() const;
    size_t Pump(size_t rowBudget = 128);
    std::vector<RowUpdate> TakeUpdates(size_t maxUpdates = 128);
    const Row* Find(RowKey key) const;
    bool IsVisible(RowKey key) const;
    // Explicit O(n) snapshot for user actions/diagnostics, never the frame update interface.
    std::vector<RowKey> OrderedKeys() const;
    Status GetStatus() const;
    bool AddFavorite(AppId_t app, Address address);
    bool RemoveFavorite(AppId_t app, RowKey key);
    bool RemoveHistory(AppId_t app, RowKey key);
    // Caller must have observed an accepted connection, never just a connect attempt.
    bool RecordAcceptedConnection(AppId_t app, Address address, uint32_t unixTime);
    // BeginClose/IsQuiescent report callback/provider safety, not retained data storage destruction.
    bool BeginClose();
    bool IsQuiescent() const;
    // Production must repeatedly call CloseStep and destroy only after complete bounded storage retirement.
    bool CloseStep(size_t nodeBudget = 128);
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
#endif
