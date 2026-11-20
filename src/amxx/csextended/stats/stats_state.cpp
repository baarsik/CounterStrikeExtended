#include <utility>
#include "stats_state.h"

namespace cse::extstats {

ServerState::ServerState(ServerHost& host) : host_(host) {
}

void ServerState::Reset() {
    slots_ = {};
    invalid_ = {};
    nextSnapshot_ = 0;
    automatic_ = false;
}

bool ServerState::Valid(int index) const {
    return index > 0 && index <= host_.MaxPlayers() && index <= MaxPlayerIndex;
}

void ServerState::Connect(int index, double now) {
    if (!Valid(index)) {
        return;
    }
    const Player player = host_.ReadPlayer(index);
    if (!player.connected || player.proxy) {
        return;
    }
    invalid_[index] = true;
    Slot& slot = slots_[index];
    // AMXX's entry forward can call a native before this module's post-entry hook.
    // Disconnect already discarded operations belonging to the preceding connection.
    std::vector<Operation> pending;
    if (!slot.connected) {
        pending = std::move(slot.pending);
    }
    slot = {};
    slot.pending = std::move(pending);
    slot.connected = true;
    slot.generation = ++nextConnection_;
    slot.alive = player.alive;
    slot.target = player.target;
    slot.refresh = true;
    slot.deadline = now + QueryTimeout;
    PublishStats(index, true);
    if (!player.bot) {
        // Reserve a distinct request range and never recycle IDs on map reset.
        constexpr std::uint32_t RequestMask = 0x3fffffffu;
        slot.identityRequest = static_cast<int>((++nextRequest_ & RequestMask) | 0x40000000u);
        slot.protocolRequest = static_cast<int>((++nextRequest_ & RequestMask) | 0x40000000u);
        host_.Query(index, IdentityCvar, slot.identityRequest);
        host_.Query(index, ProtocolCvar, slot.protocolRequest);
    }
}

void ServerState::Disconnect(int index) {
    if (Valid(index)) {
        invalid_[index] = slots_[index].connected;
        slots_[index] = {};
    }
}

void ServerState::Reply(int index, int request, const char* name, const char* value, double now) {
    if (request == 0 || !Valid(index) || !name || !value) {
        return;
    }
    Slot& slot = slots_[index];
    const Player player = host_.ReadPlayer(index);
    if (!slot.connected || !player.connected || player.bot || player.proxy || now >= slot.deadline) {
        return;
    }
    const bool supported = std::strcmp(value, "1") == 0;
    if (request == slot.identityRequest && std::strcmp(name, IdentityCvar) == 0) {
        slot.identity = supported;
        slot.identityRequest = 0;
    } else if (request == slot.protocolRequest && std::strcmp(name, ProtocolCvar) == 0) {
        slot.protocol = supported;
        slot.protocolRequest = 0;
        slot.refresh = true;
    }
}

bool ServerState::IsCseClient(int index) const {
    return Valid(index) && slots_[index].connected && host_.ReadPlayer(index).connected && slots_[index].identity;
}

bool ServerState::HasProtocol(int index) const {
    return Valid(index) && slots_[index].connected && slots_[index].protocol;
}

bool ServerState::SetHealth(int index, std::int32_t amount, bool defer) {
    if (!Valid(index) || !host_.ReadPlayer(index).connected || host_.ReadPlayer(index).proxy) {
        return false;
    }
    if (defer || emitting_ || !slots_[index].pending.empty()) {
        slots_[index].pending.push_back({Stat::Health, amount});
        return true;
    }
    if (amount > 0) {
        host_.AssignHealth(index, static_cast<float>(amount));
    } else {
        host_.Kill(index);
    }
    return PublishStats(index, true);
}

bool ServerState::SetArmor(int index, std::int32_t amount, bool defer) {
    if (amount < 0 || !Valid(index) || !host_.ReadPlayer(index).connected || host_.ReadPlayer(index).proxy) {
        return false;
    }
    if (defer || emitting_ || !slots_[index].pending.empty()) {
        slots_[index].pending.push_back({Stat::Armor, amount});
        return true;
    }
    host_.AssignArmor(index, static_cast<float>(amount));
    return PublishStats(index, true);
}

void ServerState::QueueLegacy(int index) {
    if (Valid(index) && host_.ReadPlayer(index).connected) {
        slots_[index].legacy = true;
    }
}

bool ServerState::PublishStats(int index, bool force) {
    if (!Valid(index)) {
        return false;
    }
    const Player player = host_.ReadPlayer(index);
    if (!player.connected || player.proxy) {
        return false;
    }
    Slot& slot = slots_[index];
    const std::int32_t health = NormalizeHealth(player.health, player.alive);
    const std::int32_t armor = NormalizeArmor(player.armor);
    slot.dirty = slot.dirty || force || slot.health != health || slot.armor != armor;
    slot.health = health;
    slot.armor = armor;
    return true;
}

void ServerState::SendPair(int recipient, int subject, std::int32_t health, std::int32_t armor) {
    host_.Emit(recipient, subject, Stat::Health, health);
    host_.Emit(recipient, subject, Stat::Armor, armor);
}

void ServerState::Frame(double now, bool automatic, bool messageOpen) {
    if (!messageOpen) {
        for (int index = 1; index <= host_.MaxPlayers() && index <= MaxPlayerIndex; ++index) {
            Slot& slot = slots_[index];
            if (slot.pending.empty() || !slot.connected || !host_.ReadPlayer(index).connected) {
                continue;
            }
            const std::uint64_t generation = slot.generation;
            // Move before callbacks so newly queued operations survive until the next safe frame.
            const std::vector<Operation> pending = std::move(slot.pending);
            slot.pending.clear();
            emitting_ = true;
            for (const Operation& operation : pending) {
                if (slot.generation != generation || !slot.connected || !host_.ReadPlayer(index).connected) {
                    break;
                }
                if (operation.stat == Stat::Armor) {
                    host_.AssignArmor(index, static_cast<float>(operation.amount));
                } else if (operation.amount > 0) {
                    host_.AssignHealth(index, static_cast<float>(operation.amount));
                } else {
                    host_.Kill(index);
                }
            }
            emitting_ = false;
            if (slot.generation == generation) {
                PublishStats(index, true);
            }
        }
    }
    const bool enabled = automatic && !automatic_;
    automatic_ = automatic;
    for (int index = 1; index <= host_.MaxPlayers() && index <= MaxPlayerIndex; ++index) {
        const Player player = host_.ReadPlayer(index);
        Slot& slot = slots_[index];
        if (!player.connected || player.proxy) {
            if (slot.connected) {
                Disconnect(index);
            }
            continue;
        }
        if (!slot.connected) {
            Connect(index, now);
        }
        const bool lifecycle = slot.alive != player.alive;
        if (automatic || lifecycle || enabled) {
            PublishStats(index, lifecycle || enabled);
        }
        if (slot.target != player.target) {
            slot.target = player.target;
            slot.refresh = true;
            if (Valid(slot.target)) {
                PublishStats(slot.target, true);
            }
        }
        slot.alive = player.alive;
    }
    if (messageOpen) {
        return;
    }
    const bool snapshot = now >= nextSnapshot_;
    if (snapshot) {
        nextSnapshot_ = now + SnapshotInterval;
    }
    std::array<bool, MaxPlayerIndex + 1> refresh{};
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        refresh[index] = slots_[index].refresh;
        slots_[index].refresh = false;
    }
    emitting_ = true;
    for (int subject = 1; subject <= host_.MaxPlayers() && subject <= MaxPlayerIndex; ++subject) {
        Slot& slot = slots_[subject];
        const bool connected = slot.connected;
        const bool dirty = slot.dirty;
        const bool invalid = invalid_[subject];
        const bool legacy = slot.legacy;
        const std::int32_t health = slot.health;
        const std::int32_t armor = slot.armor;
        // Consume before hookable output. A callback's new request belongs to the next publication.
        slot.dirty = false;
        slot.legacy = false;
        invalid_[subject] = false;
        const Player actual = legacy ? host_.ReadPlayer(subject) : Player{};
        if (invalid) {
            SendPair(0, subject, InvalidAmount, InvalidAmount);
        }
        if (connected && (dirty || snapshot)) {
            SendPair(0, subject, health, armor);
        }
        for (int recipient = 1; recipient <= host_.MaxPlayers() && recipient <= MaxPlayerIndex; ++recipient) {
            const Slot& target = slots_[recipient];
            if (!target.connected || (recipient != subject && target.target != subject)) {
                continue;
            }
            if (invalid && target.protocol) {
                SendPair(recipient, subject, InvalidAmount, InvalidAmount);
            }
            if (connected && (dirty || refresh[recipient])) {
                host_.EmitLegacy(recipient, subject, health, armor);
                if (target.protocol) {
                    SendPair(recipient, subject, health, armor);
                }
            } else if (connected && legacy && actual.connected) {
                host_.EmitLegacy(recipient, subject, NormalizeHealth(actual.health, actual.alive),
                                 NormalizeArmor(actual.armor));
            }
        }
    }
    emitting_ = false;
}

} // namespace cse::extstats
