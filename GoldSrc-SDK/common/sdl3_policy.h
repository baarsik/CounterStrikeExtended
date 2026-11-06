#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cse {
// Keep the exported integer mouse ABI, carrying fractional SDL3 motion between reads.
class FractionalMotion {
  public:
    int TakeX(float delta) {
        return Take(delta, x);
    }
    int TakeY(float delta) {
        return Take(delta, y);
    }
    void SetRawInput(bool enabled) {
        if (rawInput != enabled) {
            Reset();
            rawInput = enabled;
        }
    }
    void Reset() {
        x = 0.0f;
        y = 0.0f;
    }

  private:
    static int Take(float delta, float &remainder) {
        remainder += delta;
        const int whole = static_cast<int>(remainder);
        remainder -= static_cast<float>(whole);
        return whole;
    }
    bool rawInput = false;
    float x = 0.0f;
    float y = 0.0f;
};

inline bool WithinDoubleClickTime(std::uint64_t previous, std::uint64_t current, float milliseconds) {
    constexpr std::uint64_t nanosecondsPerMillisecond = 1000000;
    // SDL2 stamped each event in whole milliseconds. Quantize each timestamp, not the difference.
    return current >= previous && milliseconds >= 0.0f &&
           static_cast<double>(current / nanosecondsPerMillisecond - previous / nanosecondsPerMillisecond) <=
               milliseconds;
}

// Return a complete UTF-8 prefix; SDL owns text event storage until the next event read.
inline std::size_t Utf8PrefixBytes(const char *text, std::size_t capacity) {
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
    return windowExtent > 0
               ? static_cast<int>(coordinate * static_cast<float>(pixelExtent) / static_cast<float>(windowExtent))
               : 0;
}

inline int WindowToRenderCoordinate(float coordinate, int windowExtent, int renderExtent, float aspect) {
    const int scaled = ScaleCoordinate(coordinate, windowExtent, renderExtent);
    return static_cast<int>(scaled + (static_cast<double>(aspect) - 1.0) * (scaled - renderExtent / 2));
}

inline float RenderToWindowCoordinate(int coordinate, int windowExtent, int renderExtent, float aspect) {
    if (renderExtent <= 0 || aspect <= 0.0f) {
        return 0.0f;
    }
    const float center = static_cast<float>(renderExtent / 2);
    const float scaled = center + (static_cast<float>(coordinate) - center) / aspect;
    return scaled * static_cast<float>(windowExtent) / static_cast<float>(renderExtent);
}

inline int RefreshRateOrDefault(float refreshRate) {
    constexpr int defaultRefreshRate = 60;
    return refreshRate > 1.0f ? static_cast<int>(std::lround(refreshRate)) : defaultRefreshRate;
}

inline int EngineWheelPulse(int detents) {
    // The engine historically emits one key pulse per whole wheel event, regardless of magnitude.
    return detents > 0 ? 1 : (detents < 0 ? -1 : 0);
}

struct Utf16Character {
    std::uint16_t first;
    std::uint16_t second;
    int count;
};

inline Utf16Character EncodeUtf16(std::uint32_t codepoint) {
    constexpr std::uint32_t supplementaryStart = 0x10000;
    constexpr std::uint32_t unicodeMaximum = 0x10ffff;
    constexpr std::uint32_t highSurrogateStart = 0xd800;
    constexpr std::uint32_t lowSurrogateStart = 0xdc00;
    constexpr std::uint32_t surrogateEnd = 0xdfff;
    constexpr std::uint32_t replacementCharacter = 0xfffd;
    constexpr unsigned surrogateShift = 10;
    constexpr std::uint32_t surrogateMask = 0x3ff;
    if (codepoint > unicodeMaximum || (codepoint >= highSurrogateStart && codepoint <= surrogateEnd)) {
        codepoint = replacementCharacter;
    }
    if (codepoint < supplementaryStart) {
        return {static_cast<std::uint16_t>(codepoint), 0, 1};
    }
    codepoint -= supplementaryStart;
    return {static_cast<std::uint16_t>(highSurrogateStart + (codepoint >> surrogateShift)),
            static_cast<std::uint16_t>(lowSurrogateStart + (codepoint & surrogateMask)), 2};
}

constexpr int LegacyGamepadButtonCount = 21;
} // namespace cse
