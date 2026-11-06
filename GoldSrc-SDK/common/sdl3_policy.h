#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cse {
// Keep the exported integer mouse ABI, carrying fractional SDL3 motion between reads.
class FractionalMotion {
public:
    int TakeX(float delta) { return Take(delta, x); }
    int TakeY(float delta) { return Take(delta, y); }
    void Reset() { x = 0.0f; y = 0.0f; }

private:
    static int Take(float delta, float& remainder) {
        remainder += delta;
        const int whole = static_cast<int>(remainder);
        remainder -= static_cast<float>(whole);
        return whole;
    }
    float x = 0.0f;
    float y = 0.0f;
};

inline bool WithinDoubleClickTime(std::uint64_t previous, std::uint64_t current, float milliseconds) {
    constexpr double nanosecondsPerMillisecond = 1000000.0;
    return current >= previous && milliseconds >= 0.0f
        && static_cast<double>(current - previous) <= milliseconds * nanosecondsPerMillisecond;
}

// Return a complete UTF-8 prefix; SDL owns text event storage until the next event read.
inline std::size_t Utf8PrefixBytes(const char* text, std::size_t capacity) {
    std::size_t used = 0;
    while (text && text[used] && used < capacity) {
        const unsigned char lead = static_cast<unsigned char>(text[used]);
        const std::size_t bytes = lead < 0x80 ? 1 : (lead < 0xe0 ? 2 : (lead < 0xf0 ? 3 : 4));
        if (used + bytes > capacity) {
            break;
        }
        bool complete = true;
        for (std::size_t offset = 1; offset < bytes; ++offset) {
            if (!text[used + offset] || (static_cast<unsigned char>(text[used + offset]) & 0xc0) != 0x80) {
                complete = false;
                break;
            }
        }
        if (!complete) {
            break;
        }
        used += bytes;
    }
    return used;
}

inline int ScaleCoordinate(float coordinate, int windowExtent, int pixelExtent) {
    return windowExtent > 0 ? static_cast<int>(coordinate * pixelExtent / windowExtent) : 0;
}

inline int RefreshRateOrDefault(float refreshRate) {
    constexpr int defaultRefreshRate = 60;
    return refreshRate > 1.0f ? static_cast<int>(std::lround(refreshRate)) : defaultRefreshRate;
}

constexpr int LegacyGamepadButtonCount = 21;
}
