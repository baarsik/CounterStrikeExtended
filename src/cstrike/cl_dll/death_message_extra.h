#pragma once

#include <cstddef>
#include <cstdint>

// ReGameDLL_CS master appends these fields after the stock DeathMsg weapon string.
// Coordinates use GoldSrc WRITE_COORD's signed short in eighths.
namespace regamedll_death {
constexpr std::uint32_t kPosition = 0x001;
constexpr std::uint32_t kAssistant = 0x002;
constexpr std::uint32_t kKillRarity = 0x004;

constexpr std::uint32_t kHeadshot = 0x001;
constexpr std::uint32_t kKillerBlind = 0x002;
constexpr std::uint32_t kNoScope = 0x004;
constexpr std::uint32_t kPenetrated = 0x008;
constexpr std::uint32_t kThroughSmoke = 0x010;
constexpr std::uint32_t kAssistedFlash = 0x020;
constexpr std::uint32_t kDominationBegan = 0x040;
constexpr std::uint32_t kDomination = 0x080;
constexpr std::uint32_t kRevenge = 0x100;
constexpr std::uint32_t kInAir = 0x200;
} // namespace regamedll_death

struct DeathMessageExtra {
    bool present{};
    bool has_position{};
    float position[3]{};
    int assistant{};
    std::uint32_t rarity{};
};

inline DeathMessageExtra ParseDeathMessageExtra(const void* buffer, std::size_t size) {
    DeathMessageExtra result{};
    if (!buffer || size < 4) {
        return result;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    std::size_t offset = 3; // killer, victim, headshot
    while (offset < size && bytes[offset] != 0) {
        ++offset;
    }
    if (offset == size || ++offset == size) {
        return result;
    }

    auto read_long = [&]() -> std::uint32_t {
        const std::uint32_t value = static_cast<std::uint32_t>(bytes[offset]) |
                                    (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                                    (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                                    (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
        offset += 4;
        return value;
    };
    if (size - offset < 4) {
        return {};
    }
    const std::uint32_t flags = read_long();
    const std::size_t required = ((flags & regamedll_death::kPosition) ? 6 : 0) +
                                 ((flags & regamedll_death::kAssistant) ? 1 : 0) +
                                 ((flags & regamedll_death::kKillRarity) ? 4 : 0);
    if (size - offset < required) {
        return {};
    }

    result.present = true;
    if (flags & regamedll_death::kPosition) {
        result.has_position = true;
        for (float& coordinate : result.position) {
            const int raw = bytes[offset] | (bytes[offset + 1] << 8);
            constexpr int kShortRange = 1 << 16;
            constexpr int kShortSign = 1 << 15;
            constexpr float kCoordScale = 1.0f / 8.0f;
            coordinate = static_cast<float>(raw >= kShortSign ? raw - kShortRange : raw) * kCoordScale;
            offset += 2;
        }
    }
    if (flags & regamedll_death::kAssistant) {
        result.assistant = bytes[offset++];
    }
    if (flags & regamedll_death::kKillRarity) {
        result.rarity = read_long();
    }
    return result;
}

inline void FormatDeathRarity(std::uint32_t rarity, char* output, std::size_t capacity) {
    if (capacity == 0) {
        return;
    }
    output[0] = 0;
    struct Badge {
        std::uint32_t bit;
        const char* label;
    };
    // The headshot already has a dedicated sprite in the stock notice.
    constexpr Badge badges[] = {{regamedll_death::kKillerBlind, "BLIND"},
                                {regamedll_death::kNoScope, "NS"},
                                {regamedll_death::kPenetrated, "WALL"},
                                {regamedll_death::kThroughSmoke, "SMOKE"},
                                {regamedll_death::kAssistedFlash, "FLASH"},
                                {regamedll_death::kDominationBegan, "DOM+"},
                                {regamedll_death::kDomination, "DOM"},
                                {regamedll_death::kRevenge, "REV"},
                                {regamedll_death::kInAir, "AIR"}};
    std::size_t length = 0;
    for (const Badge& badge : badges) {
        if (!(rarity & badge.bit)) {
            continue;
        }
        const char* label = badge.label;
        std::size_t label_length = 0;
        while (label[label_length]) {
            ++label_length;
        }
        if (length + label_length + 3 >= capacity) {
            break;
        }
        output[length++] = ' ';
        output[length++] = '[';
        for (std::size_t index = 0; index < label_length; ++index) {
            output[length++] = label[index];
        }
        output[length++] = ']';
        output[length] = 0;
    }
}
