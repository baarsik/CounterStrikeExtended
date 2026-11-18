#pragma once

#include "ServerBrowserModel.h"
#include <array>

namespace cse::server_browser {
constexpr size_t DetailFrameBudget = 32;
constexpr double GameInfoRefreshSeconds = 10.0;
constexpr uint16_t DefaultServerPort = 27015;
constexpr std::array<uint16_t, 14> CommonQueryPorts{
    27015, 26900, 27016, 26901, 27017, 26902, 27018, 26903, 27019, 26904, 27020, 26905, 4242, 27215};

struct ServerAddressInput {
    std::string host;
    uint16_t port = 0;
};
bool ParseServerAddressInput(std::string_view text, ServerAddressInput &input);

// Worker owns only its result. BeginClose waits nonblocking for DNS before joining;
// no worker may outlive the native GameUI image or access a session/panel.
class ServerAddressResolver final {
public:
    ServerAddressResolver();
    ~ServerAddressResolver();
    bool Start(std::string_view text);
    bool Complete() const;
    std::optional<Address> Result() const;
    bool BeginClose();
private:
    struct State;
    std::unique_ptr<State> state_;
};

// One immutable generation. Successors start only after BeginClose retires all callbacks.
class ServerBrowserPing final {
public:
    explicit ServerBrowserPing(ISteamMatchmakingServers *servers);
    ~ServerBrowserPing();
    bool Start(Address address, bool commonPorts = false);
    std::vector<Row> TakeUpdates(size_t budget = DetailFrameBudget);
    bool Complete() const;
    bool Failed() const;
    bool BeginClose();
private:
    struct State;
    std::unique_ptr<State> state_;
};

struct DetailUpdate {
    std::string name;
    int score = 0;
    float seconds = 0;
    uint64_t generation = 0;
};
struct DetailStatus {
    bool playersComplete = false;
    bool playersFailed = false;
    bool closing = false;
};
class ServerBrowserDetails final {
public:
    explicit ServerBrowserDetails(ISteamMatchmakingServers *servers);
    ~ServerBrowserDetails();
    bool Start(Address address);
    bool RefreshMetadata();
    std::vector<Row> TakeMetadata();
    bool MetadataFailed() const;
    bool StartPlayers();
    std::vector<DetailUpdate> TakeUpdates(size_t budget = DetailFrameBudget);
    DetailStatus Status() const;
    bool BeginClose();
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
