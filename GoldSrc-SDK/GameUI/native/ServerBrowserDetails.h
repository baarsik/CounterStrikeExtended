#pragma once

#include "ServerBrowserModel.h"

namespace cse::server_browser {
constexpr size_t DetailFrameBudget = 32;
enum class DetailKind { Player, Rule };
struct DetailUpdate {
    DetailKind kind = DetailKind::Player;
    std::string name;
    std::string value;
    int score = 0;
    float seconds = 0;
};
struct DetailStatus {
    bool playersComplete = false;
    bool rulesComplete = false;
    bool playersFailed = false;
    bool rulesFailed = false;
    bool closing = false;
};
class ServerBrowserDetails final {
public:
    explicit ServerBrowserDetails(ISteamMatchmakingServers *servers);
    ~ServerBrowserDetails();
    // One generation per owner; construct a successor only after the previous owner closes.
    bool Start(Address address);
    std::vector<DetailUpdate> TakeUpdates(size_t budget = DetailFrameBudget);
    DetailStatus Status() const;
    bool BeginClose();

private:
    struct State;
    std::unique_ptr<State> state_;
};
}
