#ifndef CSE_EXTSTATS_STATE_H
#define CSE_EXTSTATS_STATE_H

#include <array>
#include <vector>
#include "cse_extended_stats_protocol.h"

namespace cse::extstats {

struct Player {
    bool connected = false;
    bool bot = false;
    bool proxy = false;
    bool alive = false;
    float health = 0;
    float armor = 0;
    int target = 0;
};

class ServerHost {
  public:
    virtual ~ServerHost() = default;
    virtual int MaxPlayers() const = 0;
    virtual Player ReadPlayer(int index) const = 0;
    virtual void AssignHealth(int index, float amount) = 0;
    virtual void AssignArmor(int index, float amount) = 0;
    virtual void Kill(int index) = 0;
    virtual void Query(int index, const char* name, int request) = 0;
    // Recipient zero is the HLTV spectator stream, never a client broadcast.
    virtual void Emit(int recipient, int subject, Stat stat, std::int32_t amount) = 0;
    virtual void EmitLegacy(int recipient, int subject, std::int32_t health, std::int32_t armor) = 0;
};

class ServerState {
  public:
    explicit ServerState(ServerHost& host);
    void Reset();
    void Connect(int index, double now);
    void Disconnect(int index);
    void Reply(int index, int request, const char* name, const char* value, double now);
    bool IsCseClient(int index) const;
    bool HasProtocol(int index) const;
    bool SetHealth(int index, std::int32_t amount, bool defer = false);
    bool SetArmor(int index, std::int32_t amount, bool defer = false);
    void QueueLegacy(int index);
    bool PublishStats(int index, bool force);
    void Frame(double now, bool automatic, bool messageOpen);

  private:
    struct Operation {
        Stat stat;
        std::int32_t amount;
    };
    struct Slot {
        std::vector<Operation> pending;
        std::uint64_t generation = 0;
        bool legacy = false;
        bool connected = false;
        bool identity = false;
        bool protocol = false;
        bool alive = false;
        bool dirty = false;
        bool refresh = false;
        int target = 0;
        int identityRequest = 0;
        int protocolRequest = 0;
        double deadline = 0;
        std::int32_t health = 0;
        std::int32_t armor = 0;
    };
    bool Valid(int index) const;
    void SendPair(int recipient, int subject, std::int32_t health, std::int32_t armor);
    ServerHost& host_;
    std::array<Slot, MaxPlayerIndex + 1> slots_{};
    std::array<bool, MaxPlayerIndex + 1> invalid_{};
    // Keep request IDs across maps so late replies cannot match a new lifetime.
    std::uint32_t nextRequest_ = 0x43530000u;
    double nextSnapshot_ = 0;
    bool automatic_ = false;
    bool emitting_ = false;
    std::uint64_t nextConnection_ = 0;
};

} // namespace cse::extstats
#endif
