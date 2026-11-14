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
    slot = {};
    slot.connected = true;
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
    if (!Valid(index) || !name || !value) {
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

bool ServerState::SetHealth(int index, std::int32_t amount) {
    if (!Valid(index) || !host_.ReadPlayer(index).connected || host_.ReadPlayer(index).proxy) {
        return false;
    }
    if (amount > 0) {
        host_.AssignHealth(index, static_cast<float>(amount));
    } else {
        host_.Kill(index);
    }
    return PublishStats(index, true);
}

bool ServerState::SetArmor(int index, std::int32_t amount) {
    if (amount < 0 || !Valid(index) || !host_.ReadPlayer(index).connected || host_.ReadPlayer(index).proxy) {
        return false;
    }
    host_.AssignArmor(index, static_cast<float>(amount));
    return PublishStats(index, true);
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
    for (int subject = 1; subject <= host_.MaxPlayers() && subject <= MaxPlayerIndex; ++subject) {
        Slot& slot = slots_[subject];
        if (invalid_[subject]) {
            SendPair(0, subject, InvalidAmount, InvalidAmount);
        }
        if (slot.connected && (slot.dirty || snapshot)) {
            SendPair(0, subject, slot.health, slot.armor);
        }
        for (int recipient = 1; recipient <= host_.MaxPlayers() && recipient <= MaxPlayerIndex; ++recipient) {
            const Slot& target = slots_[recipient];
            if (!target.connected || (recipient != subject && target.target != subject)) {
                continue;
            }
            if (invalid_[subject] && target.protocol) {
                SendPair(recipient, subject, InvalidAmount, InvalidAmount);
            }
            if (slot.connected && (slot.dirty || target.refresh)) {
                host_.EmitLegacy(recipient, subject, slot.health, slot.armor);
                if (target.protocol) {
                    SendPair(recipient, subject, slot.health, slot.armor);
                }
            }
        }
        invalid_[subject] = false;
        slot.dirty = false;
    }
    for (Slot& slot : slots_) {
        slot.refresh = false;
    }
}

} // namespace cse::extstats
