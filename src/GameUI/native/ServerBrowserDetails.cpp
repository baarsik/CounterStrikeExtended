#include "ServerBrowserDetails.h"
#include <atomic>
#include <cmath>
#include <deque>
#include <exception>
#include <locale>
#include <sstream>
#include <thread>
#if defined(_WIN32)
#include <winsock2.h>
#if defined(_MSC_VER)
// SDK 10.0.26100.0's unused WSAGetIPUserMtu leaves its _Out_ value unset on error (C6101).
// Limit this exclusion to the SDK header; restore analysis diagnostics before browser code.
#pragma warning(push)
#pragma warning(disable : 6101)
#endif
#include <ws2tcpip.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace cse::server_browser {
namespace {
constexpr size_t DetailTextBytes = 1024;
std::string DisplayText(const char *input) {
    std::string output;
    for (size_t index = 0; input && index < DetailTextBytes && input[index]; ++index) {
        const auto character = static_cast<unsigned char>(input[index]);
        output.push_back(character < ' ' || character == 127 ? ' ' : input[index]);
    }
    return output;
}

std::string_view BoundedRuleText(const char *text) {
    constexpr size_t MaximumRuleTextBytes = 64;
    size_t length = 0;
    while (text && length <= MaximumRuleTextBytes && text[length]) {
        ++length;
    }
    return length && length <= MaximumRuleTextBytes ? std::string_view{text, length} : std::string_view{};
}

void StoreAdvertisedRule(DetailRules &rules, const char *key, const char *value) {
    constexpr std::array<std::string_view, static_cast<size_t>(ServerRule::Count)> keys{
        "mp_freeforall", "mp_forcerespawn", "mp_infinite_ammo", "mp_buy_anywhere", "mp_friendlyfire",
        "mp_roundtime", "mp_freezetime", "mp_buytime", "mp_c4timer"
    };
    const auto name = BoundedRuleText(key);
    const auto text = BoundedRuleText(value);
    if (text.empty()) {
        return;
    }
    if (name == "game_version" || name == "sv_version") {
        auto version = DisplayText(value);
        const auto first = version.find_first_not_of(' ');
        if (first != std::string::npos) {
            auto &destination = name == "sv_version" ? rules.versions.engine : rules.versions.gameDll;
            destination = version.substr(first, version.find_last_not_of(' ') - first + 1);
        }
        return;
    }
    for (size_t index = 0; index < keys.size(); ++index) {
        if (name != keys[index]) {
            continue;
        }
        std::istringstream stream{std::string{text}};
        stream.imbue(std::locale::classic());
        double number = 0;
        if ((stream >> std::noskipws >> number) && stream.eof() && std::isfinite(number)) {
            rules.values[index] = number;
        }
        return;
    }
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

DetailVersions ReadVersionTags(std::string_view tags) {
    DetailVersions versions;
    constexpr size_t MaximumVersionTagBytes = 32;
    constexpr std::string_view VersionCharacters =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.+-_";
    while (!tags.empty()) {
        const auto delimiter = tags.find_first_of(",;");
        auto tag = tags.substr(0, delimiter);
        tags.remove_prefix(delimiter == std::string_view::npos ? tags.size() : delimiter + 1);
        const auto first = tag.find_first_not_of(" \t");
        if (first == std::string_view::npos) {
            continue;
        }
        tag = tag.substr(first, tag.find_last_not_of(" \t") - first + 1);
        const auto separator = tag.find(':');
        if (separator == std::string_view::npos) {
            continue;
        }
        const auto version = tag.substr(separator + 1);
        if (version.empty() || version.size() > MaximumVersionTagBytes || version.front() < '0' ||
            version.front() > '9' || version.find_first_not_of(VersionCharacters) != std::string_view::npos) {
            continue;
        }
        const auto product = tag.substr(0, separator);
        if (product == "rehlds" && versions.engine.empty()) {
            versions.engine = "ReHLDS ";
            versions.engine.append(version);
        } else if (product == "regamedll" && versions.gameDll.empty()) {
            versions.gameDll = "ReGameDLL ";
            versions.gameDll.append(version);
        }
    }
    return versions;
}

bool ParseServerAddressInput(std::string_view text, ServerAddressInput &input) {
    constexpr size_t MaximumHostLength = 253;
    const auto colon = text.find(':');
    const auto host = text.substr(0, colon);
    if (host.empty() || host.size() > MaximumHostLength || host.front() == '.' || host.back() == '.') {
        return false;
    }
    bool numeric = true;
    for (const unsigned char character : host) {
        const bool digit = character >= '0' && character <= '9';
        const bool letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
        if (!digit && !letter && character != '-' && character != '.') {
            return false;
        }
        numeric = numeric && (digit || character == '.');
    }
    uint32_t port = 0;
    if (colon != std::string_view::npos) {
        const auto value = text.substr(colon + 1);
        if (value.empty()) {
            return false;
        }
        constexpr uint32_t MaximumPort = UINT16_MAX;
        for (const unsigned char character : value) {
            if (character < '0' || character > '9') {
                return false;
            }
            port = port * 10 + character - '0';
            if (port > MaximumPort) {
                return false;
            }
        }
    }
    if (numeric) {
        Address address;
        if (!ParseAddress(std::string(host) + ":" + std::to_string(port ? port : DefaultServerPort), address)) {
            return false;
        }
    }
    input = {std::string(host), static_cast<uint16_t>(port)};
    return true;
}

struct ServerAddressResolver::State {
    std::thread worker;
    std::atomic<bool> complete{false};
    std::optional<Address> result;
    bool started = false;
    bool closing = false;
};
ServerAddressResolver::ServerAddressResolver() : state_(new State) {}
ServerAddressResolver::~ServerAddressResolver() {
    if (!BeginClose()) {
        std::terminate();
    }
}
bool ServerAddressResolver::Start(std::string_view text) {
    auto &state = *state_;
    if (state.started || state.closing) {
        return false;
    }
    state.started = true;
    ServerAddressInput input;
    if (!ParseServerAddressInput(text, input)) {
        state.complete.store(true, std::memory_order_release);
        return false;
    }
    Address numeric;
    if (ParseAddress(input.host + ":" + std::to_string(input.port ? input.port : DefaultServerPort), numeric)) {
        numeric.connectionPort = input.port;
        numeric.queryPort = input.port;
        state.result = numeric;
        state.complete.store(true, std::memory_order_release);
        return true;
    }
    state.worker = std::thread([&state, input] {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo *addresses = nullptr;
        if (getaddrinfo(input.host.c_str(), nullptr, &hints, &addresses) == 0) {
            for (auto *item = addresses; item; item = item->ai_next) {
                if (item->ai_family == AF_INET && item->ai_addrlen >= sizeof(sockaddr_in)) {
                    const auto *address = reinterpret_cast<const sockaddr_in *>(item->ai_addr);
                    const uint32_t ip = ntohl(address->sin_addr.s_addr);
                    if (ip) {
                        state.result = Address{ip, input.port, input.port};
                    }
                    break;
                }
            }
            freeaddrinfo(addresses);
        }
        state.complete.store(true, std::memory_order_release);
    });
    return true;
}
bool ServerAddressResolver::Complete() const {
    return state_->complete.load(std::memory_order_acquire);
}
std::optional<Address> ServerAddressResolver::Result() const {
    return Complete() && !state_->closing ? state_->result : std::nullopt;
}
bool ServerAddressResolver::BeginClose() {
    state_->closing = true;
    if (state_->worker.joinable()) {
        if (!Complete()) {
            return false;
        }
        state_->worker.join();
    }
    return true;
}

struct ServerBrowserPing::State {
    struct Query final : ISteamMatchmakingPingResponse {
        State &owner;
        Address expected;
        HServerQuery handle = HSERVERQUERY_INVALID;
        bool complete = false;
        Query(State &state, Address address) : owner(state), expected(address) {}
        void ServerResponded(gameserveritem_t &item) override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            if (owner.closing || complete) {
                return;
            }
            complete = true;
            Row row = CopyPingResponse(item);
            if (row.key.ip == expected.ip && (!expected.connectionPort ||
                    row.key.connectionPort == expected.connectionPort)) {
                // The response's query endpoint is not allowed to retarget this generation.
                row.key.queryPort = expected.queryPort;
                owner.updates.push_back(std::move(row));
            } else {
                owner.failed = true;
            }
        }
        void ServerFailedToRespond() override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            if (!owner.closing && !complete) {
                owner.failed = true;
            }
            complete = true;
        }
    };
    ISteamMatchmakingServers *servers;
    const std::thread::id thread = std::this_thread::get_id();
    std::vector<std::unique_ptr<Query>> queries;
    std::deque<Row> updates;
    size_t providerDepth = 0;
    size_t callbackDepth = 0;
    bool started = false;
    bool closing = false;
    bool failed = false;
    explicit State(ISteamMatchmakingServers *provider) : servers(provider) {}
    void CheckThread() const {
        if (thread != std::this_thread::get_id()) {
            std::terminate();
        }
    }
};
ServerBrowserPing::ServerBrowserPing(ISteamMatchmakingServers *servers) : state_(new State(servers)) {}
ServerBrowserPing::~ServerBrowserPing() {
    if (!BeginClose()) {
        std::terminate();
    }
}
bool ServerBrowserPing::Start(Address address, bool commonPorts) {
    auto &state = *state_;
    state.CheckThread();
    if (state.started || state.closing || !state.servers || !address.ip || (!address.queryPort && !commonPorts)) {
        state.failed = true;
        return false;
    }
    state.started = true;
    DepthScope use(state.providerDepth);
    const auto start = [&](Address target) {
        state.queries.emplace_back(new State::Query(state, target));
        auto &query = *state.queries.back();
        query.handle = state.servers->PingServer(target.ip, target.queryPort, &query);
        if (query.handle == HSERVERQUERY_INVALID && !query.complete) {
            query.complete = true;
            state.failed = true;
        }
    };
    if (commonPorts && !address.queryPort) {
        for (const auto port : CommonQueryPorts) {
            if (state.closing) {
                break;
            }
            start({address.ip, 0, port});
        }
    } else {
        start(address);
    }
    return !state.closing;
}
std::vector<Row> ServerBrowserPing::TakeUpdates(size_t budget) {
    auto &state = *state_;
    state.CheckThread();
    std::vector<Row> result;
    if (!state.closing && !state.providerDepth && !state.callbackDepth) {
        while (result.size() < budget && !state.updates.empty()) {
            result.push_back(std::move(state.updates.front()));
            state.updates.pop_front();
        }
    }
    return result;
}
bool ServerBrowserPing::Complete() const {
    state_->CheckThread();
    for (const auto &query : state_->queries) {
        if (!query->complete) {
            return false;
        }
    }
    return true;
}
bool ServerBrowserPing::Failed() const {
    return state_->failed;
}
bool ServerBrowserPing::BeginClose() {
    auto &state = *state_;
    state.CheckThread();
    state.closing = true;
    if (state.providerDepth || state.callbackDepth) {
        return false;
    }
    DepthScope use(state.providerDepth);
    for (auto &query : state.queries) {
        if (!query->complete && query->handle != HSERVERQUERY_INVALID) {
            const auto handle = query->handle;
            query->handle = HSERVERQUERY_INVALID;
            state.servers->CancelServerQuery(handle);
        }
    }
    size_t work = 0;
    while (!state.updates.empty() && work++ < DetailFrameBudget) {
        state.updates.pop_front();
    }
    return state.updates.empty();
}

struct ServerBrowserDetails::State {
    struct Players final : ISteamMatchmakingPlayersResponse {
        State &owner;
        explicit Players(State &state) : owner(state) {}
        void AddPlayerToList(const char *name, int score, float seconds) override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            if (!owner.status.closing && !owner.status.playersComplete) {
                owner.updates.push_back({owner.playerGeneration,
                    {DisplayText(name), score,
                        std::isfinite(seconds) && seconds >= 0 ? seconds : 0, owner.playerGeneration}});
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
    };

    struct Rules final : ISteamMatchmakingRulesResponse {
        State &owner;
        explicit Rules(State &state) : owner(state) {}

        void RulesResponded(const char *key, const char *value) override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            if (!owner.status.closing && owner.rulesPending) {
                StoreAdvertisedRule(owner.pendingRules, key, value);
            }
        }

        void RulesFailedToRespond() override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            owner.CompleteRules(false);
        }

        void RulesRefreshComplete() override {
            owner.CheckThread();
            DepthScope use(owner.callbackDepth);
            owner.CompleteRules(true);
        }
    };

    std::unique_ptr<Players> players;
    std::unique_ptr<Rules> rules;
    ISteamMatchmakingServers *servers;
    std::unique_ptr<ServerBrowserPing> ping;
    const std::thread::id thread = std::this_thread::get_id();
    Address address;
    HServerQuery playerQuery = HSERVERQUERY_INVALID;
    HServerQuery rulesQuery = HSERVERQUERY_INVALID;
    DetailStatus status;
    std::deque<std::pair<uint64_t, DetailUpdate>> updates;
    uint64_t playerGeneration = 0;
    DetailRules pendingRules;
    std::optional<DetailRules> rulesUpdate;
    bool rulesPending = false;
    size_t providerDepth = 0;
    size_t callbackDepth = 0;
    bool started = false;
    explicit State(ISteamMatchmakingServers *provider) : servers(provider) {}
    void CheckThread() const {
        if (thread != std::this_thread::get_id()) {
            std::terminate();
        }
    }

    void CompleteRules(bool responded) {
        if (status.closing || !rulesPending) {
            return;
        }
        rulesPending = false;
        pendingRules.responded = responded;
        rulesUpdate = responded ? std::move(pendingRules) : DetailRules{};
    }

    void CancelRules() {
        const auto handle = rulesQuery;
        const bool cancel = rulesPending && handle != HSERVERQUERY_INVALID;
        rulesQuery = HSERVERQUERY_INVALID;
        rulesPending = false;
        if (cancel) {
            servers->CancelServerQuery(handle);
        }
    }
};
ServerBrowserDetails::ServerBrowserDetails(ISteamMatchmakingServers *servers) : state_(new State(servers)) {}
ServerBrowserDetails::~ServerBrowserDetails() {
    if (!BeginClose()) {
        std::terminate();
    }
}
bool ServerBrowserDetails::Start(Address address) {
    auto &state = *state_;
    state.CheckThread();
    if (state.started || state.status.closing) {
        return false;
    }
    state.started = true;
    state.address = address;
    return RefreshMetadata();
}
bool ServerBrowserDetails::RefreshMetadata() {
    auto &state = *state_;
    state.CheckThread();
    if (!state.started || state.status.closing || state.providerDepth || state.callbackDepth) {
        return false;
    }
    DepthScope use(state.providerDepth);
    if (state.ping && !state.ping->BeginClose()) {
        return false;
    }
    state.ping.reset(new ServerBrowserPing(state.servers));
    return state.ping->Start(state.address) && !state.status.closing;
}
std::vector<Row> ServerBrowserDetails::TakeMetadata() {
    return state_->ping ? state_->ping->TakeUpdates() : std::vector<Row>{};
}
bool ServerBrowserDetails::MetadataFailed() const {
    return state_->ping && state_->ping->Failed();
}
bool ServerBrowserDetails::StartPlayers() {
    auto &state = *state_;
    state.CheckThread();
    if (state.status.closing || !state.servers || !state.started || state.providerDepth || state.callbackDepth) {
        return false;
    }
    DepthScope use(state.providerDepth);
    if (state.playerQuery != HSERVERQUERY_INVALID && !state.status.playersComplete) {
        // Reject synchronous cancellation callbacks before replacing this independent player generation.
        state.status.playersComplete = true;
        state.servers->CancelServerQuery(state.playerQuery);
    }
    if (state.status.closing) {
        return false;
    }
    state.playerQuery = HSERVERQUERY_INVALID;
    state.status.playersComplete = false;
    state.status.playersFailed = false;
    ++state.playerGeneration;
    state.players.reset(new State::Players(state));
    state.playerQuery = state.servers->PlayerDetails(state.address.ip, state.address.queryPort, state.players.get());
    if (state.playerQuery == HSERVERQUERY_INVALID) {
        state.status.playersComplete = true;
        state.status.playersFailed = true;
    }
    return !state.status.closing;
}
std::vector<DetailUpdate> ServerBrowserDetails::TakeUpdates(size_t budget) {
    auto &state = *state_;
    state.CheckThread();
    std::vector<DetailUpdate> result;
    if (!state.status.closing && !state.providerDepth && !state.callbackDepth) {
        size_t work = 0;
        while (work++ < budget && !state.updates.empty()) {
            if (state.updates.front().first == state.playerGeneration) {
                result.push_back(std::move(state.updates.front().second));
            }
            state.updates.pop_front();
        }
    }
    return result;
}

bool ServerBrowserDetails::StartRules() {
    auto &state = *state_;
    state.CheckThread();
    if (state.status.closing || !state.started || state.providerDepth || state.callbackDepth) {
        return false;
    }
    DepthScope use(state.providerDepth);
    state.CancelRules();
    if (state.status.closing) {
        return false;
    }
    state.pendingRules = {};
    state.rulesUpdate.reset();
    state.rulesPending = true;
    if (state.servers && state.address.ip && state.address.queryPort) {
        state.rules.reset(new State::Rules(state));
        state.rulesQuery = state.servers->ServerRules(state.address.ip, state.address.queryPort, state.rules.get());
    }
    if (state.rulesQuery == HSERVERQUERY_INVALID) {
        state.CompleteRules(false);
    }
    return !state.status.closing;
}

std::optional<DetailRules> ServerBrowserDetails::TakeRules() {
    auto &state = *state_;
    state.CheckThread();
    if (state.status.closing || state.providerDepth || state.callbackDepth) {
        return std::nullopt;
    }
    auto result = std::move(state.rulesUpdate);
    state.rulesUpdate.reset();
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
    DepthScope use(state.providerDepth);
    const bool pingClosed = !state.ping || state.ping->BeginClose();
    if (state.playerQuery != HSERVERQUERY_INVALID) {
        const auto handle = state.playerQuery;
        state.playerQuery = HSERVERQUERY_INVALID;
        if (!state.status.playersComplete) {
            state.servers->CancelServerQuery(handle);
        }
    }
    state.CancelRules();
    state.rulesUpdate.reset();
    size_t work = 0;
    while (work < DetailFrameBudget && !state.updates.empty()) {
        state.updates.pop_front();
        ++work;
    }
    return pingClosed && state.updates.empty();
}
}
