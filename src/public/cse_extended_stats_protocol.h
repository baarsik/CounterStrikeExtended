#ifndef CSE_EXTENDED_STATS_PROTOCOL_H
#define CSE_EXTENDED_STATS_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace cse::extstats {

inline constexpr char HealthMessage[] = "HealthEx";
inline constexpr char ArmorMessage[] = "BatteryEx";
inline constexpr char IdentityCvar[] = "cse_client";
inline constexpr char ProtocolCvar[] = "cse_extstats_protocol";
inline constexpr int ProtocolVersion = 1;
inline constexpr int MessageBytes = 5;
inline constexpr int MaxPlayerIndex = 255;
inline constexpr int DisplayMaximum = 99999;
inline constexpr int LegacyHealthMaximum = 255;
inline constexpr int LegacyArmorMaximum = 999;
inline constexpr int BroadcastHealthMaximum = 127;
inline constexpr std::int32_t InvalidAmount = -1;
inline constexpr double QueryTimeout = 2.0;
inline constexpr double SnapshotInterval = 1.0;

enum class Stat {
    Health,
    Armor
};

struct Value {
    std::int32_t amount = 0;
    bool valid = false;
};

inline std::int32_t NormalizeArmor(float amount) {
    if (!(amount > 0.0f)) {
        return 0;
    }
    constexpr auto Maximum = (std::numeric_limits<std::int32_t>::max)();
    if (static_cast<double>(amount) >= static_cast<double>(Maximum)) {
        return Maximum;
    }
    return static_cast<std::int32_t>(amount);
}

inline std::int32_t NormalizeHealth(float health, bool alive) {
    if (!alive || !(health > 0.0f)) {
        return 0;
    }
    const std::int32_t amount = NormalizeArmor(health);
    return amount > 0 ? amount : 1;
}

inline int DisplayAmount(std::int32_t amount) {
    if (amount <= 0) {
        return 0;
    }
    return amount > DisplayMaximum ? DisplayMaximum : amount;
}

inline bool DecodeMessage(const void* buffer, std::size_t size, int maxPlayers, int& player, std::int32_t& amount) {
    if (!buffer || size != MessageBytes) {
        return false;
    }
    const auto* bytes = static_cast<const unsigned char*>(buffer);
    const int index = bytes[0];
    if (index < 1 || index > maxPlayers) {
        return false;
    }
    constexpr int ByteBits = 8;
    const std::uint32_t bits = static_cast<std::uint32_t>(bytes[1]) |
                               (static_cast<std::uint32_t>(bytes[2]) << ByteBits) |
                               (static_cast<std::uint32_t>(bytes[3]) << (ByteBits * 2)) |
                               (static_cast<std::uint32_t>(bytes[4]) << (ByteBits * 3));
    std::int32_t decoded = 0;
    std::memcpy(&decoded, &bits, sizeof(decoded));
    if (decoded < InvalidAmount) {
        return false;
    }
    player = index;
    amount = decoded;
    return true;
}

} // namespace cse::extstats

#endif
