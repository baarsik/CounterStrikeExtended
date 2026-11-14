#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if __has_include("stats_state.h")
#include "stats_state.h"
using namespace cse::extstats;

struct Packet {
    int recipient;
    int subject;
    Stat stat;
    std::int32_t amount;
};
struct Probe {
    int player;
    std::string name;
    int request;
};
struct Engine : ServerHost {
    std::array<Player, 5> players{};
    std::vector<Packet> packets;
    std::vector<Probe> probes;
    int kills = 0;
    int MaxPlayers() const override {
        return 4;
    }
    Player ReadPlayer(int i) const override {
        return players[i];
    }
    void AssignHealth(int i, float value) override {
        players[i].health = value;
    }
    void AssignArmor(int i, float value) override {
        players[i].armor = value;
    }
    void Kill(int i) override {
        players[i].alive = false;
        players[i].health = 0;
        ++kills;
    }
    void Query(int i, const char* name, int request) override {
        probes.push_back({i, name, request});
    }
    void EmitLegacy(int, int, std::int32_t, std::int32_t) override {
    }
    void Emit(int recipient, int subject, Stat stat, std::int32_t amount) override {
        packets.push_back({recipient, subject, stat, amount});
    }
    void Add(int i, bool bot = false) {
        players[i] = {true, bot, false, true, 100, 0, 0};
    }
    bool Has(int recipient, int subject, Stat stat, std::int32_t amount) const {
        for (const auto& packet : packets) {
            if (packet.recipient == recipient && packet.subject == subject && packet.stat == stat &&
                packet.amount == amount) {
                return true;
            }
        }
        return false;
    }
};

static void Check(bool condition, const char* why) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}

static void Identify(ServerState& state, Engine& host, int player, double now = 0) {
    const auto probes = host.probes;
    for (const auto& probe : probes) {
        if (probe.player == player) {
            state.Reply(player, probe.request, probe.name.c_str(), "1", now);
        }
    }
}

int main(int, char** argv) {
    Engine host;
    host.Add(1);
    ServerState state(host);
    state.Connect(1, 0);
    const std::string test = argv[1];
    if (test == "native_setters") {
        Check(state.SetHealth(1, 99999), "health native accepted");
        Check(state.SetArmor(1, 100000), "armor native accepted");
        Check(host.players[1].health == 99999 && host.players[1].armor == 100000, "native updates entities");
        Check(!state.SetArmor(1, -1) && !state.SetHealth(0, 10), "native validation");
        Check(state.SetHealth(1, 0) && host.kills == 1, "normal kill path");
        Identify(state, host, 1);
        state.Frame(0.1, false, false);
        Check(host.Has(1, 1, Stat::Health, 0), "death published");
    } else if (test == "identity_vs_protocol") {
        Check(!state.IsCseClient(1), "pending identity false");
        const auto identity = host.probes.front();
        state.Reply(1, identity.request, identity.name.c_str(), "1", 0.1);
        Check(state.IsCseClient(1) && !state.HasProtocol(1), "identity separate from protocol");
        Check(!state.IsCseClient(0) && !state.IsCseClient(5), "invalid identity false");
    } else if (test == "query_timeout") {
        Identify(state, host, 1, 2.1);
        Check(!state.IsCseClient(1) && !state.HasProtocol(1), "late reply ignored");
    } else if (test == "reused_slot_reply") {
        const auto old = host.probes;
        state.Disconnect(1);
        state.Connect(1, 0.5);
        for (const auto& probe : old) {
            state.Reply(1, probe.request, probe.name.c_str(), "1", 0.6);
        }
        Check(!state.IsCseClient(1), "old connection response ignored");
    } else if (test == "nested_publication") {
        Identify(state, host, 1);
        state.SetHealth(1, 65536);
        state.Frame(0.1, true, true);
        Check(host.packets.empty(), "no emission inside a message");
        state.Frame(0.2, true, false);
        Check(host.Has(1, 1, Stat::Health, 65536), "pending update retained");
    } else if (test == "manual_mode") {
        Identify(state, host, 1);
        state.Frame(0.1, false, false);
        host.packets.clear();
        host.players[1].health = 777;
        state.Frame(1.2, false, false);
        Check(!host.Has(0, 1, Stat::Health, 777) && host.Has(0, 1, Stat::Health, 100), "snapshot uses published cache");
        state.PublishStats(1, true);
        state.Frame(1.3, false, false);
        Check(host.Has(1, 1, Stat::Health, 777), "explicit sync works in manual mode");
    } else if (test == "recipient_permissions") {
        host.Add(2);
        host.Add(3);
        host.players[3].alive = false;
        host.players[3].target = 1;
        state.Connect(2, 0);
        state.Connect(3, 0);
        Identify(state, host, 1);
        Identify(state, host, 2);
        Identify(state, host, 3);
        state.SetHealth(1, 99999);
        state.Frame(0.1, true, false);
        Check(host.Has(3, 1, Stat::Health, 99999), "observer receives target");
        Check(!host.Has(2, 1, Stat::Health, 99999), "opponent does not receive stats");
    } else if (test == "hltv_stable_snapshot") {
        state.Frame(0, true, false);
        host.packets.clear();
        state.Frame(0.9, true, false);
        Check(host.packets.empty(), "no frame spam");
        state.Frame(1.0, true, false);
        Check(host.Has(0, 1, Stat::Health, 100) && host.Has(0, 1, Stat::Armor, 0), "stable broadcast refreshed");
    } else if (test == "live_mode_toggle") {
        state.Frame(0, false, false);
        host.packets.clear();
        host.players[1].health = 50000;
        state.Frame(0.1, true, false);
        Check(host.Has(0, 1, Stat::Health, 50000), "enable forces current state");
    } else if (test == "lifecycle_force") {
        Identify(state, host, 1);
        state.Frame(0, false, false);
        host.packets.clear();
        host.players[1].alive = false;
        state.Frame(0.1, false, false);
        Check(host.Has(1, 1, Stat::Health, 0), "manual mode still handles death");
        host.players[1].alive = true;
        host.players[1].health = 2000;
        state.Frame(0.2, false, false);
        Check(host.Has(1, 1, Stat::Health, 2000), "manual mode still handles respawn");
    } else {
        Identify(state, host, 1);
        state.Frame(0, true, false);
        host.packets.clear();
        host.players[1].health = 256;
        host.players[1].armor = 65536;
        state.Frame(0.1, true, false);
        Check(host.Has(1, 1, Stat::Health, 256) && host.Has(1, 1, Stat::Armor, 65536), "automatic entity changes");
    }
    return 0;
}
#else
int main() {
    std::fputs("FAIL: native stats service is not implemented\n", stderr);
    return 1;
}
#endif
