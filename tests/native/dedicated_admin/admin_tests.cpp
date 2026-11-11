#include "admin_actual.inc"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <string>
#include <type_traits>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                       \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
struct Provider : IGameServerData {
    std::deque<std::vector<char>> responses;
    std::vector<std::vector<char>> requests;
    int reads = 0;
    void WriteDataRequest(const void* data, int size) override {
        requests.emplace_back(static_cast<const char*>(data), static_cast<const char*>(data) + size);
    }
    int ReadDataResponse(void* data, int size) override {
        ++reads;
        if (responses.empty()) {
            return 0;
        }
        auto packet = responses.front();
        responses.pop_front();
        CHECK(static_cast<int>(packet.size()) <= size);
        std::memcpy(data, packet.data(), packet.size());
        return static_cast<int>(packet.size());
    }
};
struct Listener : IServerDataResponse {
    int calls = 0;
    std::function<void()> action;
    std::string value;
    void OnServerDataResponse(const char*, const char* response) override {
        ++calls;
        value = response;
        auto callback = action;
        if (callback) {
            callback();
        }
    }
};
// Missing API is an explicit behavioral RED while still compiling the baseline source.
template <class T, class = void> struct Binding {
    static bool Bind(T&, IGameServerData*, std::uint64_t) {
        return false;
    }
    static void Shutdown(T&) {
    }
};
template <class T> struct Binding<T, std::void_t<decltype(std::declval<T>().Bind(nullptr, 1))>> {
    static bool Bind(T& remote, IGameServerData* provider, std::uint64_t generation) {
        return remote.Bind(provider, generation);
    }
    static void Shutdown(T& remote) {
        remote.Shutdown();
    }
};
void AppendInt(std::vector<char>& packet, std::int32_t value) {
    auto bytes = reinterpret_cast<const char*>(&value);
    packet.insert(packet.end(), bytes, bytes + sizeof(value));
}
std::vector<char> Packet(std::int32_t id, std::int32_t type, const char* name, const char* value = "") {
    std::vector<char> packet;
    AppendInt(packet, id);
    AppendInt(packet, type);
    packet.insert(packet.end(), name, name + std::strlen(name) + 1);
    if (type == SERVERDATA_RESPONSE_VALUE) {
        AppendInt(packet, static_cast<std::int32_t>(std::strlen(value) + 1));
        packet.insert(packet.end(), value, value + std::strlen(value) + 1);
    }
    return packet;
}
int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::string mode = argv[1];
    CRemoteServer remote;
    Provider provider;
    if (mode == "admin_bind_failure_retry") {
        CHECK(!Binding<CRemoteServer>::Bind(remote, nullptr, 1));
        remote.SendCommand("status");
        CHECK(provider.requests.empty());
        CHECK(Binding<CRemoteServer>::Bind(remote, &provider, 2));
        remote.SendCommand("status");
        CHECK(provider.requests.size() == 1);
        Binding<CRemoteServer>::Shutdown(remote);
        Binding<CRemoteServer>::Shutdown(remote);
        const int reads = provider.reads;
        remote.SendCommand("quit");
        remote.ProcessServerResponse();
        CHECK(provider.requests.size() == 1 && provider.reads == reads);
        CHECK(g_pGameServerData == nullptr);
        return 0;
    }
    // Exercise pre-existing methods independently of the missing Bind API for diagnostic REDs.
    g_pGameServerData = &provider;
    remote.Initialize();
    Listener first;
    Listener second;
    Listener third;
    if (mode == "response_self_next_removal") {
        remote.AddServerMessageHandler(&first, "UpdatePlayers");
        remote.AddServerMessageHandler(&second, "UpdatePlayers");
        remote.AddServerMessageHandler(&third, "UpdatePlayers");
        first.action = [&] {
            remote.RemoveServerDataResponseTarget(&first);
            remote.RemoveServerDataResponseTarget(&second);
        };
        provider.responses.push_back(Packet(0, SERVERDATA_UPDATE, "UpdatePlayers"));
        remote.ProcessServerResponse();
        CHECK(first.calls == 1 && second.calls == 0 && third.calls == 1);
    } else if (mode == "old_generation_revoked") {
        remote.RequestValue(&first, "hostname");
        std::int32_t oldID;
        std::memcpy(&oldID, provider.requests.back().data(), sizeof(oldID));
        provider.responses.push_back(Packet(0, 1, "old-update"));
        CHECK(Binding<CRemoteServer>::Bind(remote, &provider, 2));
        remote.AddServerMessageHandler(&third, "old-update");
        remote.RequestValue(&second, "hostname");
        provider.responses.push_back(Packet(oldID, 0, "hostname", "stale"));
        remote.ProcessServerResponse();
        CHECK(first.calls == 0 && second.calls == 0 && third.calls == 0);
        first.action = [&] { Binding<CRemoteServer>::Shutdown(remote); };
        remote.AddServerMessageHandler(&first, "close");
        provider.responses.push_back(Packet(0, 1, "close"));
        const int reads = provider.reads;
        remote.ProcessServerResponse();
        CHECK(first.calls == 1 && provider.reads == reads + 1);
    } else if (mode == "admin_command_roundtrip") {
        remote.SendCommand("say \"zażółć test\"");
        CHECK(provider.requests.size() == 1);
        CHECK(std::string(provider.requests.back().data() + 8) == "say \"zażółć test\"");
        remote.SendCommand(std::string(512, 'x').c_str());
        CHECK(provider.requests.size() == 1); // reject, never execute a truncated prefix
        remote.SetValue("hostname", std::string(512, 'y').c_str());
        CHECK(provider.requests.size() == 1);
    } else if (mode == "close_during_response") {
        auto* closing = new Listener;
        auto* deletedNext = new Listener;
        remote.AddServerMessageHandler(closing, "close");
        remote.AddServerMessageHandler(deletedNext, "close");
        closing->action = [&] {
            remote.Shutdown();
            delete deletedNext;
            delete closing;
        };
        provider.responses.push_back(Packet(0, 1, "close"));
        provider.responses.push_back(Packet(0, 1, "close"));
        const int reads = provider.reads;
        CHECK(remote.ProcessServerResponse());
        CHECK(provider.reads == reads + 1);
        CHECK(!remote.ProcessServerResponse());
        CHECK(provider.reads == reads + 1);
    } else if (mode == "malformed_packet_rejected") {
        remote.RequestValue(&first, "hostname");
        std::int32_t id;
        std::memcpy(&id, provider.requests.back().data(), sizeof(id));
        auto malformed = Packet(id, 0, "hostname", "abc");
        malformed.pop_back();
        provider.responses.push_back(malformed);
        remote.ProcessServerResponse();
        CHECK(first.calls == 0);
        const auto valid = Packet(id, 0, "hostname", "valid");
        for (std::int32_t length : {-1, 0, 65536}) {
            auto packet = valid;
            std::memcpy(packet.data() + 8 + std::strlen("hostname") + 1, &length, sizeof(length));
            provider.responses.push_back(packet);
        }
        auto unterminated = Packet(id, 1, "hostname");
        unterminated.pop_back();
        provider.responses.push_back(unterminated);
        provider.responses.push_back(std::vector<char>(7, 0));
        provider.responses.push_back(Packet(id, 2, "hostname", "unknown"));
        remote.ProcessServerResponse();
        CHECK(first.calls == 0);
        provider.responses.push_back(Packet(id, 0, "hostname", "valid"));
        remote.ProcessServerResponse();
        CHECK(first.calls == 1 && first.value == "valid");
    }
    Binding<CRemoteServer>::Shutdown(remote);
}
