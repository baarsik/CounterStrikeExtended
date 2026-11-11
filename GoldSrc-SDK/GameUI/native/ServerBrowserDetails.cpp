#include "ServerBrowserDetails.h"
#include <cmath>
#include <deque>
#include <exception>
#include <thread>

namespace cse::server_browser {
namespace {
constexpr size_t DetailTextBytes = 1024;
std::string DisplayText(const char *input) {
    std::string output;
    if (!input) {
        return output;
    }
    for (size_t index = 0; index < DetailTextBytes && input[index]; ++index) {
        const auto character = static_cast<unsigned char>(input[index]);
        output.push_back(character < ' ' || character == 127 ? ' ' : input[index]);
    }
    return output;
}
struct DepthScope {
    size_t &depth;
    explicit DepthScope(size_t &value) : depth(value) {
        ++depth;
    }
    ~DepthScope() {
        --depth;
    }
};
}
struct ServerBrowserDetails::State {
    struct Players final : ISteamMatchmakingPlayersResponse {
        State &owner;
        explicit Players(State &state) : owner(state) {}
        void AddPlayerToList(const char *name, int score, float seconds) override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            if (!owner.status.closing) {
                owner.updates.push_back({DetailKind::Player,
                                         DisplayText(name),
                                         {},
                                         score,
                                         std::isfinite(seconds) && seconds >= 0 ? seconds : 0});
            }
        }
        void PlayersFailedToRespond() override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            owner.status.playersFailed = true;
            owner.status.playersComplete = true;
        }
        void PlayersRefreshComplete() override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            owner.status.playersComplete = true;
        }
    } players{*this};
    struct Rules final : ISteamMatchmakingRulesResponse {
        State &owner;
        explicit Rules(State &state) : owner(state) {}
        void RulesResponded(const char *name, const char *value) override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            if (!owner.status.closing) {
                owner.updates.push_back({DetailKind::Rule, DisplayText(name), DisplayText(value), 0, 0});
            }
        }
        void RulesFailedToRespond() override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            owner.status.rulesFailed = true;
            owner.status.rulesComplete = true;
        }
        void RulesRefreshComplete() override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            owner.status.rulesComplete = true;
        }
    } rules{*this};
    ISteamMatchmakingServers *servers;
    const std::thread::id thread = std::this_thread::get_id();
    HServerQuery playerQuery = HSERVERQUERY_INVALID;
    HServerQuery ruleQuery = HSERVERQUERY_INVALID;
    DetailStatus status;
    std::deque<DetailUpdate> updates;
    size_t providerDepth = 0;
    size_t callbackDepth = 0;
    bool started = false;
    explicit State(ISteamMatchmakingServers *provider) : servers(provider) {}
    void CheckThread() const {
        if (thread != std::this_thread::get_id()) {
            std::terminate();
        }
    }
};

ServerBrowserDetails::ServerBrowserDetails(ISteamMatchmakingServers *servers) : state_(new State(servers)) {}

ServerBrowserDetails::~ServerBrowserDetails() {
    if (!BeginClose()) {
        // Destruction is an invariant, never a cancellation/join substitute.
        std::terminate();
    }
}

bool ServerBrowserDetails::Start(Address address) {
    auto &state = *state_;
    state.CheckThread();
    if (state.started || state.status.closing || !state.servers || !address.ip || !address.queryPort) {
        return false;
    }
    state.started = true;
    DepthScope use(state.providerDepth);
    state.playerQuery = state.servers->PlayerDetails(address.ip, address.queryPort, &state.players);
    if (state.playerQuery == HSERVERQUERY_INVALID) {
        state.status.playersComplete = true;
        state.status.playersFailed = true;
    }
    if (state.status.closing) {
        return false;
    }
    state.ruleQuery = state.servers->ServerRules(address.ip, address.queryPort, &state.rules);
    if (state.ruleQuery == HSERVERQUERY_INVALID) {
        state.status.rulesComplete = true;
        state.status.rulesFailed = true;
    }
    return !state.status.closing;
}

std::vector<DetailUpdate> ServerBrowserDetails::TakeUpdates(size_t budget) {
    auto &state = *state_;
    state.CheckThread();
    std::vector<DetailUpdate> result;
    if (state.status.closing || state.providerDepth || state.callbackDepth) {
        return result;
    }
    while (result.size() < budget && !state.updates.empty()) {
        result.push_back(std::move(state.updates.front()));
        state.updates.pop_front();
    }
    return result;
}

DetailStatus ServerBrowserDetails::Status() const {
    state_->CheckThread();
    return state_->status;
}

bool ServerBrowserDetails::BeginClose() {
    auto &state = *state_;
    state.CheckThread();
    state.status.closing = true;
    if (state.providerDepth || state.callbackDepth) {
        return false;
    }
    // Pinned Valve contract requires canceling in-progress queries before destroying their callback.
    // Both proxy objects survive CancelServerQuery and all synchronous/reentrant callbacks it performs.
    DepthScope use(state.providerDepth);
    if (state.playerQuery != HSERVERQUERY_INVALID) {
        if (!state.status.playersComplete) {
            state.servers->CancelServerQuery(state.playerQuery);
        }
        state.playerQuery = HSERVERQUERY_INVALID;
    }
    if (state.ruleQuery != HSERVERQUERY_INVALID) {
        if (!state.status.rulesComplete) {
            state.servers->CancelServerQuery(state.ruleQuery);
        }
        state.ruleQuery = HSERVERQUERY_INVALID;
    }
    size_t work = 0;
    while (work < DetailFrameBudget && !state.updates.empty()) {
        state.updates.pop_front();
        ++work;
    }
    return state.updates.empty();
}
}
