#include "ServerBrowserMasterserverQuery.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace cse::server_browser {
namespace {
constexpr size_t MaximumDirectoryServers = 65536;
constexpr size_t MaximumDirectoryPages = 4096;
constexpr size_t MaximumConcurrentPings = 32;
constexpr int DirectoryAttempts = 2;
constexpr auto DirectoryTimeout = std::chrono::milliseconds(1500);
constexpr int CancellationPollMicroseconds = 100000;
constexpr std::array<unsigned char, 6> DirectoryHeader{255, 255, 255, 255, 'f', '\n'};
constexpr size_t AddressBytes = 6;
#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle InvalidSocket = -1;
#endif
struct Socket {
    SocketHandle handle = InvalidSocket;
    ~Socket() {
        if (handle != InvalidSocket) {
#if defined(_WIN32)
            closesocket(handle);
#else
            close(handle);
#endif
        }
    }
};
struct Directory {
    std::thread worker;
    std::atomic<bool> stop{false};
    std::atomic<bool> done{false};
    std::mutex mutex;
    std::deque<Address> pending;
    bool failed = false; // Read only after acquiring done.
    ~Directory() {
        if (worker.joinable()) {
            worker.join();
        }
    }

    bool CompleteAndDrained() {
        // Only inspect emptiness after acquiring the worker's final address publication.
        if (!done.load(std::memory_order_acquire)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex);
        return pending.empty();
    }

    bool Query(const sockaddr* address, size_t addressSize, const std::string& filter,
               unsigned char region, std::set<Address>& seen) {
        Socket socket;
        socket.handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket.handle == InvalidSocket ||
            connect(socket.handle, address, static_cast<int>(addressSize)) != 0) {
            return false;
        }
        std::string cursor = "0.0.0.0:0";
        std::set<std::string> cursors;
        for (size_t page = 0; page < MaximumDirectoryPages && !stop; ++page) {
            if (!cursors.insert(cursor).second) {
                return false;
            }
            std::string request(1, '1');
            request += static_cast<char>(region);
            request += cursor;
            request += '\0';
            request += filter;
            request += '\0';
            bool received = false;
            for (int attempt = 0; attempt < DirectoryAttempts && !received && !stop; ++attempt) {
                if (send(socket.handle, request.data(), static_cast<int>(request.size()), 0) < 0) {
                    break;
                }
                const auto deadline = std::chrono::steady_clock::now() + DirectoryTimeout;
                while (!stop && std::chrono::steady_clock::now() < deadline) {
                    fd_set readable;
                    FD_ZERO(&readable);
                    FD_SET(socket.handle, &readable);
                    timeval timeout{0, CancellationPollMicroseconds};
#if defined(_WIN32)
                    const int ready = select(0, &readable, nullptr, nullptr, &timeout);
#else
                    const int ready = select(socket.handle + 1, &readable, nullptr, nullptr, &timeout);
#endif
                    if (ready < 0) {
                        break;
                    }
                    if (!ready) {
                        continue;
                    }
                    std::array<unsigned char, UINT16_MAX> bytes{};
                    const auto size = recv(socket.handle, reinterpret_cast<char*>(bytes.data()),
                        static_cast<int>(bytes.size()), 0);
                    if (size < 0) {
                        break;
                    }
                    if (static_cast<size_t>(size) < DirectoryHeader.size() + AddressBytes ||
                        !std::equal(DirectoryHeader.begin(), DirectoryHeader.end(), bytes.begin()) ||
                        (static_cast<size_t>(size) - DirectoryHeader.size()) % AddressBytes != 0) {
                        continue;
                    }
                    std::string next;
                    bool terminal = false;
                    for (size_t offset = DirectoryHeader.size(); offset < static_cast<size_t>(size);
                         offset += AddressBytes) {
                        constexpr int ByteBits = 8;
                        uint32_t ip = 0;
                        for (size_t octet = 0; octet < sizeof(ip); ++octet) {
                            ip = (ip << ByteBits) | bytes[offset + octet];
                        }
                        const auto port = static_cast<uint16_t>((bytes[offset + sizeof(ip)] << ByteBits) |
                            bytes[offset + sizeof(ip) + 1]);
                        if (!ip && !port) {
                            terminal = true;
                            break;
                        }
                        if (!ip || !port) {
                            continue;
                        }
                        Address server{ip, port, port};
                        next = ConnectionAddress(server);
                        if (seen.insert(server).second) {
                            std::lock_guard<std::mutex> lock(mutex);
                            pending.push_back(server);
                        }
                        if (seen.size() >= MaximumDirectoryServers) {
                            return false;
                        }
                    }
                    if (terminal) {
                        return true;
                    }
                    // A delayed duplicate of the previous page is not progress.
                    if (next.empty() || cursors.count(next)) {
                        continue;
                    }
                    cursor = std::move(next);
                    received = true;
                    break;
                }
            }
            if (!received) {
                return false;
            }
        }
        return false;
    }
    void Start(std::vector<std::string> endpoints, std::string filter, unsigned char region) {
        worker = std::thread([this, endpoints = std::move(endpoints), filter = std::move(filter), region] {
#if defined(_WIN32)
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                failed = true;
                done.store(true, std::memory_order_release);
                return;
            }
#endif
            std::set<Address> seen;
            bool success = false;
            for (const auto& text : endpoints) {
                if (stop || seen.size() >= MaximumDirectoryServers) {
                    break;
                }
                MasterEndpoint endpoint;
                if (!ParseMasterEndpoint(text, endpoint)) {
                    continue;
                }
                addrinfo hints{};
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                addrinfo* addresses = nullptr;
                const auto port = std::to_string(endpoint.port);
                if (getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &addresses) != 0) {
                    continue;
                }
                for (auto* item = addresses; item && !stop; item = item->ai_next) {
                    if (Query(item->ai_addr, item->ai_addrlen, filter, region, seen)) {
                        success = true;
                        break;
                    }
                }
                freeaddrinfo(addresses);
            }
            failed = !success;
#if defined(_WIN32)
            WSACleanup();
#endif
            done.store(true, std::memory_order_release);
        });
    }
};
}

struct MasterserverQueryProvider::State {
    struct Entry final : ISteamMatchmakingPingResponse {
        gameserveritem_t item;
        HServerQuery query = HSERVERQUERY_INVALID;
        bool complete = false;
        bool accepting = true;
        bool scheduled = false; // Queued or active in the current refresh.
        explicit Entry(Address address, AppId_t app) {
            item.m_NetAdr.Init(address.ip, address.queryPort, address.connectionPort);
            item.m_nAppID = app;
        }
        void ServerResponded(gameserveritem_t& response) override {
            if (complete) {
                return;
            }
            complete = true;
            if (accepting) {
                const auto ip = item.m_NetAdr.GetIP();
                const auto port = item.m_NetAdr.GetQueryPort();
                if (response.m_NetAdr.GetIP() == ip && response.m_NetAdr.GetQueryPort() == port &&
                    response.m_NetAdr.GetConnectionPort()) {
                    item = response;
                    item.m_NetAdr.Init(ip, port, response.m_NetAdr.GetConnectionPort());
                    item.m_bHadSuccessfulResponse = true;
                }
            }
        }
        void ServerFailedToRespond() override {
            complete = true;
        }
    };
    struct Request {
        Directory directory;
        ISteamMatchmakingServerListResponse* callback = nullptr;
        AppId_t app = 0;
        std::vector<std::unique_ptr<Entry>> rows;
        std::deque<int> queued;
        std::vector<int> active;
        bool refreshing = true;
        bool released = false;

        void Queue(int index) {
            auto& row = *rows[static_cast<size_t>(index)];
            if (!row.scheduled) {
                queued.push_back(index);
                row.scheduled = true;
                refreshing = true;
            }
        }
    };
    ISteamMatchmakingServers* steam;
    Masterserver selected = MasterserverSettings{}.Selected();
    std::map<HServerListRequest, std::shared_ptr<Request>> requests;
    std::vector<std::shared_ptr<Request>> retired;
    bool closing = false;
    bool pumping = false;
    size_t cancelDepth = 0;
    explicit State(ISteamMatchmakingServers* service) : steam(service) {}
    bool IsRetired(HServerListRequest handle) const {
        return std::any_of(retired.begin(), retired.end(), [handle](const auto& request) {
            return request.get() == handle;
        });
    }
    void Cancel(Request& request) {
        ++cancelDepth;
        struct CancelScope {
            size_t& depth;
            ~CancelScope() {
                --depth;
            }
        } scope{cancelDepth};
        request.refreshing = false;
        request.directory.stop = true;
        // Revoke presentation callbacks first; detach each handle before its cancellation can reenter release.
        auto active = std::move(request.active);
        request.active.clear();
        for (const int index : request.queued) {
            request.rows[static_cast<size_t>(index)]->scheduled = false;
        }
        request.queued.clear();
        for (const int index : active) {
            auto& row = *request.rows[static_cast<size_t>(index)];
            row.accepting = false;
            row.scheduled = false;
        }
        for (const int index : active) {
            auto& row = *request.rows[static_cast<size_t>(index)];
            const auto query = row.query;
            row.query = HSERVERQUERY_INVALID;
            // Cancelling one query may synchronously finish another. Check liveness at each boundary.
            if (!row.complete && query != HSERVERQUERY_INVALID) {
                steam->CancelServerQuery(query);
            }
        }
    }
};

MasterserverQueryProvider::MasterserverQueryProvider(ISteamMatchmakingServers* steam) : state_(new State(steam)) {}
MasterserverQueryProvider::~MasterserverQueryProvider() {
    if (!BeginClose()) {
        std::terminate();
    }
}
void MasterserverQueryProvider::Select(const Masterserver& provider) {
    state_->selected = provider;
}
HServerListRequest MasterserverQueryProvider::Start(AppId_t app, MatchMakingKeyValuePair_t** filters, uint32 count,
                                                   ISteamMatchmakingServerListResponse* callback, bool spectator) {
    auto& state = *state_;
    if (state.closing || !state.steam || !callback) {
        return nullptr;
    }
    auto request = std::make_shared<State::Request>();
    request->app = app;
    request->callback = callback;
    std::string filter;
    constexpr unsigned char AllRegions = 255;
    unsigned char region = AllRegions;
    for (uint32 index = 0; index < count; ++index) {
        const std::string key = filters[index]->m_szKey;
        const std::string value = filters[index]->m_szValue;
        if (key == "region") {
            if (value.size() == 1 && value[0] >= '0' && value[0] <= '7') {
                region = static_cast<unsigned char>(value[0] - '0');
            }
        } else if (key == "gamedir" && value.find_first_of("\\\r\n") == std::string::npos) {
            filter += "\\gamedir\\" + value;
        }
    }
    if (spectator) {
        filter += "\\proxy\\1";
    }
    const auto handle = static_cast<HServerListRequest>(request.get());
    state.requests.emplace(handle, request);
    request->directory.Start(state.selected.endpoints, std::move(filter), region);
    return handle;
}
void MasterserverQueryProvider::Pump(size_t budget) {
    auto& state = *state_;
    if (state.pumping || state.cancelDepth) {
        return;
    }
    state.pumping = true;
    struct PumpScope {
        bool& pumping;
        ~PumpScope() {
            pumping = false;
        }
    } scope{state.pumping};
    size_t retirementBudget = budget;
    for (auto it = state.retired.begin(); it != state.retired.end();) {
        auto& request = **it;
        if (!request.directory.done.load(std::memory_order_acquire)) {
            ++it;
            continue;
        }
        while (retirementBudget && !request.rows.empty()) {
            request.rows.pop_back();
            --retirementBudget;
        }
        while (retirementBudget && !request.directory.pending.empty()) {
            request.directory.pending.pop_front();
            --retirementBudget;
        }
        if (request.rows.empty() && request.directory.pending.empty()) {
            it = state.retired.erase(it);
        } else {
            ++it;
        }
    }
    std::vector<std::shared_ptr<State::Request>> live;
    for (const auto& item : state.requests) {
        live.push_back(item.second);
    }
    for (const auto& request : live) {
        if (!request->refreshing || request->released) {
            continue;
        }
        size_t work = 0;
        {
            std::lock_guard<std::mutex> lock(request->directory.mutex);
            // Reserve work for metadata starts; a large directory must not starve idle ping slots.
            const size_t discoveryBudget = request->queued.empty() ? std::max(size_t{1}, budget / 2) : 0;
            while (!request->directory.pending.empty() && work < discoveryBudget && work < budget) {
                ++work;
                const auto address = request->directory.pending.front();
                request->directory.pending.pop_front();
                const auto index = static_cast<int>(request->rows.size());
                request->rows.push_back(std::make_unique<State::Entry>(address, request->app));
                request->Queue(index);
            }
        }
        for (size_t slot = 0; slot < request->active.size() && request->refreshing;) {
            const int index = request->active[slot];
            auto& row = *request->rows[static_cast<size_t>(index)];
            if (!row.complete) {
                ++slot;
                continue;
            }
            row.accepting = false;
            row.scheduled = false;
            // A terminal callback has already retired this individual query in the provider.
            row.query = HSERVERQUERY_INVALID;
            request->active.erase(request->active.begin() + static_cast<ptrdiff_t>(slot));
            if (!request->refreshing || request->released) {
                break;
            }
            const auto handle = static_cast<HServerListRequest>(request.get());
            if (row.item.m_bHadSuccessfulResponse) {
                request->callback->ServerResponded(handle, index);
            } else {
                request->callback->ServerFailedToRespond(handle, index);
            }
        }
        while (request->refreshing && !request->queued.empty() &&
               request->active.size() < MaximumConcurrentPings && work++ < budget) {
            const int index = request->queued.front();
            request->queued.pop_front();
            auto& row = *request->rows[static_cast<size_t>(index)];
            request->active.push_back(index);
            row.accepting = true;
            row.complete = false;
            row.item.m_bHadSuccessfulResponse = false;
            row.query = state.steam->PingServer(row.item.m_NetAdr.GetIP(), row.item.m_NetAdr.GetQueryPort(), &row);
            if (!request->refreshing || request->released) {
                row.accepting = false;
                const auto query = row.query;
                row.query = HSERVERQUERY_INVALID;
                if (!row.complete && query != HSERVERQUERY_INVALID) {
                    state.steam->CancelServerQuery(query);
                }
            } else if (row.query == HSERVERQUERY_INVALID) {
                row.complete = true;
            }
        }
        if (request->refreshing && request->directory.CompleteAndDrained() &&
            request->queued.empty() && request->active.empty()) {
            request->refreshing = false;
            request->callback->RefreshComplete(static_cast<HServerListRequest>(request.get()),
                request->directory.failed ? eServerFailedToRespond :
                    request->rows.empty() ? eNoServersListedOnMasterServer : eServerResponded);
        }
    }
}
bool MasterserverQueryProvider::BeginClose(size_t budget) {
    auto& state = *state_;
    state.closing = true;
    while (!state.requests.empty()) {
        ReleaseRequest(state.requests.begin()->first);
    }
    Pump(budget);
    return state.retired.empty();
}
void MasterserverQueryProvider::ReleaseRequest(HServerListRequest handle) {
    auto& state = *state_;
    const auto found = state.requests.find(handle);
    if (found == state.requests.end()) {
        if (state.steam && !state.IsRetired(handle)) {
            state.steam->ReleaseRequest(handle);
        }
        return;
    }
    auto request = found->second;
    request->released = true;
    state.retired.push_back(request);
    state.requests.erase(found);
    state.Cancel(*request);
}
void MasterserverQueryProvider::CancelQuery(HServerListRequest handle) {
    const auto found = state_->requests.find(handle);
    if (found != state_->requests.end()) {
        const auto request = found->second;
        state_->Cancel(*request);
    } else if (state_->steam && !state_->IsRetired(handle)) {
        state_->steam->CancelQuery(handle);
    }
}
gameserveritem_t* MasterserverQueryProvider::GetServerDetails(HServerListRequest handle, int index) {
    const auto found = state_->requests.find(handle);
    if (found == state_->requests.end()) {
        return state_->steam ? state_->steam->GetServerDetails(handle, index) : nullptr;
    }
    const auto& rows = found->second->rows;
    return index >= 0 && static_cast<size_t>(index) < rows.size() ? &rows[static_cast<size_t>(index)]->item : nullptr;
}
int MasterserverQueryProvider::GetServerCount(HServerListRequest handle) {
    const auto found = state_->requests.find(handle);
    return found == state_->requests.end() ? (state_->steam ? state_->steam->GetServerCount(handle) : 0) :
        static_cast<int>(found->second->rows.size());
}
bool MasterserverQueryProvider::IsRefreshing(HServerListRequest handle) {
    const auto found = state_->requests.find(handle);
    return found == state_->requests.end() ? (state_->steam && state_->steam->IsRefreshing(handle)) :
        found->second->refreshing;
}
void MasterserverQueryProvider::RefreshServer(HServerListRequest handle, int index) {
    const auto found = state_->requests.find(handle);
    if (found == state_->requests.end()) {
        if (state_->steam) {
            state_->steam->RefreshServer(handle, index);
        }
        return;
    }
    auto& request = *found->second;
    if (!request.released && index >= 0 && static_cast<size_t>(index) < request.rows.size()) {
        request.Queue(index);
    }
}
void MasterserverQueryProvider::RefreshQuery(HServerListRequest handle) {
    const auto found = state_->requests.find(handle);
    if (found == state_->requests.end()) {
        if (state_->steam) {
            state_->steam->RefreshQuery(handle);
        }
        return;
    }
    // An empty request still needs a completion callback on the next pump.
    auto& request = *found->second;
    request.refreshing = true;
    for (size_t index = 0; index < request.rows.size(); ++index) {
        request.Queue(static_cast<int>(index));
    }
}
HServerListRequest MasterserverQueryProvider::RequestInternetServerList(AppId_t app, MatchMakingKeyValuePair_t** pairs,
    uint32 count, ISteamMatchmakingServerListResponse* response) {
    if (state_->selected.id == "steam") {
        return state_->steam ? state_->steam->RequestInternetServerList(app, pairs, count, response) : nullptr;
    }
    return Start(app, pairs, count, response, false);
}
HServerListRequest MasterserverQueryProvider::RequestSpectatorServerList(AppId_t app, MatchMakingKeyValuePair_t** pairs,
    uint32 count, ISteamMatchmakingServerListResponse* response) {
    if (state_->selected.id == "steam") {
        return state_->steam ? state_->steam->RequestSpectatorServerList(app, pairs, count, response) : nullptr;
    }
    return Start(app, pairs, count, response, true);
}
HServerListRequest MasterserverQueryProvider::RequestLANServerList(AppId_t app,
    ISteamMatchmakingServerListResponse* response) {
    return state_->steam ? state_->steam->RequestLANServerList(app, response) : nullptr;
}
#define CSE_FORWARD_LIST(method) \
HServerListRequest MasterserverQueryProvider::method(AppId_t app, MatchMakingKeyValuePair_t** pairs, \
    uint32 count, ISteamMatchmakingServerListResponse* response) { \
    return state_->steam ? state_->steam->method(app, pairs, count, response) : nullptr; \
}
CSE_FORWARD_LIST(RequestFriendsServerList)
CSE_FORWARD_LIST(RequestFavoritesServerList)
CSE_FORWARD_LIST(RequestHistoryServerList)
#undef CSE_FORWARD_LIST
HServerQuery MasterserverQueryProvider::PingServer(uint32 ip, uint16 port, ISteamMatchmakingPingResponse* response) {
    return state_->steam ? state_->steam->PingServer(ip, port, response) : HSERVERQUERY_INVALID;
}
HServerQuery MasterserverQueryProvider::PlayerDetails(uint32 ip, uint16 port,
    ISteamMatchmakingPlayersResponse* response) {
    return state_->steam ? state_->steam->PlayerDetails(ip, port, response) : HSERVERQUERY_INVALID;
}
HServerQuery MasterserverQueryProvider::ServerRules(uint32 ip, uint16 port, ISteamMatchmakingRulesResponse* response) {
    return state_->steam ? state_->steam->ServerRules(ip, port, response) : HSERVERQUERY_INVALID;
}
void MasterserverQueryProvider::CancelServerQuery(HServerQuery handle) {
    if (state_->steam) {
        state_->steam->CancelServerQuery(handle);
    }
}
}
