#pragma once

#include "interface.h"
#include "cse_module_abi.h"
#include <cstdint>

namespace cse::radar {
inline constexpr char EngineInterface[] = "CSERadarEngine001";
inline constexpr std::uint32_t EngineApiVersion = 1;
inline constexpr int LightmapCount = 64;

struct Glyph {
    std::uint32_t texture{};
    float uv[4]{};
    int bearing_x{};
    int wide{};
    int tall{};
    int advance{};
};

// Optional, process-local renderer data. Returned GL texture names are
// valid only in the current engine context; no protocol or existing interface changes.
struct EngineApi {
    std::uint32_t size;
    std::uint32_t version;
    const int*(CSE_ABI_CALL* Lightmaps)();
    int(CSE_ABI_CALL* MessageNumber)();
    std::uint64_t(CSE_ABI_CALL* GraphicsGeneration)();
    bool(CSE_ABI_CALL* GetGlyph)(std::uint32_t font, std::uint32_t character, Glyph* out);
};
// Engine-owned invalidation runs before changing or destroying the GL context.
void AdvanceGraphicsGeneration();
} // namespace cse::radar
