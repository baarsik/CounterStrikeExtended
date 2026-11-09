#ifndef CSE_VGUI1_NATIVE_ATLAS_H
#define CSE_VGUI1_NATIVE_ATLAS_H

#include "VGui1NativeRuntime.h"
#include <array>
#include <cstddef>
#include <memory>

namespace vgui {
    class Font;
}
namespace cse_vgui1_native {
constexpr std::size_t AtlasPageLimit = 8;
constexpr std::size_t AtlasGlyphCount = 256;
constexpr int AtlasSide = 512;
constexpr std::size_t AtlasPageBytes = static_cast<std::size_t>(AtlasSide) * AtlasSide * 4;

struct Quad {
    int x;
    int y;
    int wide;
    int tall;
    int texture;
    float uv[4];
};

// Same-engine synchronous renderer boundary. Upload copies borrowed pixels.
// Retire(false-live) must remove metadata WITHOUT touching a lost device. False
// retains ownership for explicit retry. Draw/pen callbacks cannot throw/fail;
// the complete text geometry is validated before the first draw or pen write.
struct RendererV1 {
    void* context;
    std::uint64_t (*generation)(void*) noexcept;
    bool (*live)(void*, std::uint64_t) noexcept;
    int (*create)(void*) noexcept;
    bool (*upload)(void*, int, const unsigned char*, std::size_t) noexcept;
    bool (*retire)(void*, int, std::uint64_t, bool) noexcept;
    void (*getPen)(void*, int&, int&) noexcept;
    void (*setPen)(void*, int, int) noexcept;
    void (*draw)(void*, const Quad&) noexcept;
};

struct AtlasKey {
    std::uint64_t moduleEpoch = 0;
    const void* platform = nullptr;
    std::uint64_t catalog = 0;
    std::uint64_t renderer = 0;
};

struct AtlasDiagnostics {
    AtlasKey key;
    std::size_t pages;
    std::size_t admitted;
    bool valid;
    bool retirementPending;
};

// One active atlas per embedded surface. A second entry exists only while
// checked replacement/rollback retires; no unbounded Font-history cache. Font*
// is borrowed only by Capture; platform keys live ONLY with a held Selection.
class Atlas {
public:
    Atlas(Runtime&, const RendererV1&) noexcept;
    ~Atlas() noexcept;
    Atlas(const Atlas&) = delete;
    Atlas& operator=(const Atlas&) = delete;
    Result Select(vgui::Font*) noexcept;
    Result Draw(const char*, int) noexcept;
    Result Retire() noexcept;
    Result Invalidate() noexcept;
    AtlasDiagnostics Inspect() const noexcept;

private:
    struct Glyph {
        int a = 0;
        int b = 0;
        int c = 0;
        int x = 0;
        int y = 0;
        std::size_t page = AtlasPageLimit;
        float uv[4]{};
    };
    struct Entry {
        cse_vgui1_selection::Selection selection;
        AtlasKey key;
        std::array<int, AtlasPageLimit> textures{};
        std::array<Glyph, AtlasGlyphCount> glyphs{};
        std::size_t pages = 0;
        std::size_t admitted = 0;
        int height = 0;
        bool valid = false;
    };
    Result Use(Entry&, cse_vgui1_selection::PreparedFont&) noexcept;
    Result Build(Entry&, const cse_vgui1_selection::PreparedFont&) noexcept;
    Result ReleaseTextures(Entry&) noexcept;
    Result ReleaseEntry(std::unique_ptr<Entry>&) noexcept;
    Runtime& runtime_;
    RendererV1 renderer_;
    std::unique_ptr<Entry> entry_;
    std::unique_ptr<Entry> pending_;
    bool active_ = false;
    bool rendererValid_ = false;
};
}
#endif
