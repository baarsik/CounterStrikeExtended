#include "VGui1NativeAtlas.h"
#include "VGUI_Font.h"
#include "../utils/vgui/lib-src/vgui/posix/VGui1FontPlatform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace cse_vgui1_native {
namespace {
namespace selection = cse_vgui1_selection;
Result Translate(selection::Result value) noexcept {
    switch (value) {
    case selection::Result::Ready:
    case selection::Result::Captured:
    case selection::Result::Retired:
        return Result::Ready;
    case selection::Result::ReleaseFailed:
        return Result::ReleaseFailed;
    case selection::Result::Busy:
        return Result::Busy;
    default:
        return Result::Unavailable;
    }
}

class Active {
public:
    explicit Active(bool& active) noexcept : active_(active) {
        active_ = true;
    }
    ~Active() noexcept {
        active_ = false;
    }
private:
    bool& active_;
};

bool IntFits(std::int64_t value) noexcept {
    return value >= (std::numeric_limits<int>::min)() && value <= (std::numeric_limits<int>::max)();
}
}

Atlas::Atlas(Runtime& runtime, const RendererV1& renderer) noexcept : runtime_(runtime), renderer_(renderer) {
    rendererValid_ = renderer_.context && renderer_.generation && renderer_.live && renderer_.create
        && renderer_.upload && renderer_.retire && renderer_.getPen && renderer_.setPen && renderer_.draw;
}

Atlas::~Atlas() noexcept {
    if (!entries_.empty() || pending_) {
        std::fputs("VGUI1 native atlas destroyed before successful explicit retirement\n", stderr);
        std::abort();
    }
}

AtlasDiagnostics Atlas::Inspect() const noexcept {
    return entry_ ? AtlasDiagnostics{entry_->key, entry_->pages, entry_->admitted, entry_->valid,
        static_cast<bool>(pending_) || retirementPending_} : AtlasDiagnostics{{}, 0, 0, false,
        static_cast<bool>(pending_) || retirementPending_};
}

Result Atlas::ReleaseTextures(Entry& entry) noexcept {
    entry.valid = false;
    Result result = Result::Ready;
    for (auto& texture : entry.textures) {
        if (texture != 0) {
            const bool live = renderer_.live(renderer_.context, entry.key.renderer);
            if (renderer_.retire(renderer_.context, texture, entry.key.renderer, live)) {
                texture = 0;
            } else {
                result = Result::ReleaseFailed;
            }
        }
    }
    return result;
}

Result Atlas::ReleaseEntry(std::unique_ptr<Entry>& entry) noexcept {
    if (!entry) {
        return Result::Ready;
    }
    const auto released = ReleaseTextures(*entry);
    if (released != Result::Ready) {
        return released;
    }
    const auto retired = Translate(entry->selection.Retire());
    if (retired == Result::Ready) {
        entry.reset();
    }
    return retired;
}

Result Atlas::Retire() noexcept {
    if (runtime_.IsOpen() && !runtime_.OnOwnerThread()) {
        return Result::Unavailable;
    }
    if (active_) {
        return Result::Busy;
    }
    Active active(active_);
    Result result = ReleaseEntry(pending_);
    for (auto iterator = entries_.begin(); iterator != entries_.end();) {
        const bool selected = iterator->get() == entry_;
        const auto released = ReleaseEntry(*iterator);
        if (released == Result::Ready) {
            if (selected) {
                entry_ = nullptr;
            }
            iterator = entries_.erase(iterator);
        } else {
            result = released;
            ++iterator;
        }
    }
    retirementPending_ = result != Result::Ready;
    return result;
}

Result Atlas::Invalidate() noexcept {
    if (runtime_.IsOpen() && !runtime_.OnOwnerThread()) {
        return Result::Unavailable;
    }
    if (active_) {
        return Result::Busy;
    }
    Active active(active_);
    Result result = pending_ ? ReleaseTextures(*pending_) : Result::Ready;
    for (auto& entry : entries_) {
        const auto released = ReleaseTextures(*entry);
        if (released != Result::Ready) {
            result = released;
        }
    }
    return result;
}

Result Atlas::Use(Entry& entry, selection::PreparedFont& prepared) noexcept {
    const auto generation = renderer_.generation(renderer_.context);
    if (!generation || !renderer_.live(renderer_.context, generation)) {
        entry.valid = false;
        return Result::Unavailable;
    }
    const auto status = entry.selection.BeginUse(generation, prepared);
    if (status != selection::Result::Ready) {
        entry.valid = false;
        return Translate(status);
    }
    constexpr std::uint32_t Required = cse_vgui1::CapabilityAbc | cse_vgui1::CapabilityLegacyRgba
        | cse_vgui1::CapabilityPenOffsets;
    if ((prepared.capabilities & Required) != Required) {
        entry.valid = false;
        entry.selection.EndUse(prepared.useSerial);
        return Result::Unavailable;
    }
    if (prepared.atlasInvalidated || entry.key.catalog != prepared.catalogGeneration
        || entry.key.renderer != generation || !entry.valid) {
        const auto built = Build(entry, prepared);
        if (built != Result::Ready) {
            entry.selection.EndUse(prepared.useSerial);
            return built;
        }
    }
    return Result::Ready;
}

Result Atlas::Build(Entry& entry, const selection::PreparedFont& prepared) noexcept {
    // The ONLY platform dereference lane: actual private declaration, held
    // BeginUse guard and genuine root/provider image leases. Never a Font cast.
    entry.valid = false;
    auto* face = static_cast<vgui::BaseFontPlat*>(const_cast<void*>(prepared.platform));
    std::array<Glyph, AtlasGlyphCount> glyphs{};
    const int height = face->getTall();
    constexpr int LastCoordinate = AtlasSide - 1;
    if (height <= 0 || height > LastCoordinate) {
        return Result::Capacity;
    }
    int x = 0;
    int y = 0;
    std::size_t page = 0;
    for (std::size_t byte = 0; byte < glyphs.size(); ++byte) {
        auto& glyph = glyphs[byte];
        face->getCharABCwide(static_cast<int>(byte), glyph.a, glyph.b, glyph.c);
        if (glyph.a < 0 || glyph.b < 0 || glyph.b > LastCoordinate) {
            return Result::Capacity;
        }
        if (glyph.b + x > LastCoordinate) {
            x = 0;
            y += height + 1;
        }
        if (height + y > LastCoordinate) {
            ++page;
            if (page >= AtlasPageLimit) {
                return Result::Capacity; // No slot read/upload/publication yet.
            }
            x = 0;
            y = 0;
        }
        glyph.x = x;
        glyph.y = y;
        glyph.page = page;
        constexpr float Scale = 1.0f / AtlasSide;
        glyph.uv[0] = static_cast<float>(x) * Scale;
        glyph.uv[1] = static_cast<float>(y) * Scale;
        glyph.uv[2] = static_cast<float>(x + glyph.b) * Scale;
        glyph.uv[3] = static_cast<float>(y + height) * Scale;
        x += glyph.b + 1;
    }
    const auto released = ReleaseTextures(entry);
    if (released != Result::Ready) {
        return released;
    }
    entry.key = {runtime_.ModuleEpoch(), prepared.platform, prepared.catalogGeneration,
        prepared.rendererGeneration};
    std::unique_ptr<unsigned char[]> pixels(new unsigned char[AtlasPageBytes]);
    const std::size_t pages = page + 1;
    for (std::size_t index = 0; index < pages; ++index) {
        std::memset(pixels.get(), 0, AtlasPageBytes);
        for (std::size_t byte = 0; byte < glyphs.size(); ++byte) {
            const auto& glyph = glyphs[byte];
            if (glyph.page == index) {
                face->getCharRGBA(static_cast<int>(byte), glyph.x, glyph.y, AtlasSide, AtlasSide, pixels.get());
            }
        }
        if (!renderer_.live(renderer_.context, prepared.rendererGeneration)) {
            const auto rollback = ReleaseTextures(entry);
            return rollback == Result::Ready ? Result::Unavailable : rollback;
        }
        const int texture = renderer_.create(renderer_.context);
        if (texture <= 0) {
            const auto rollback = ReleaseTextures(entry);
            return rollback == Result::Ready ? Result::UploadFailed : rollback;
        }
        entry.textures[index] = texture;
        const bool uploaded = renderer_.upload(renderer_.context, texture, pixels.get(), AtlasPageBytes);
        const bool live = renderer_.live(renderer_.context, prepared.rendererGeneration);
        if (!uploaded || !live) {
            const auto rollback = ReleaseTextures(entry);
            return rollback == Result::Ready ? (live ? Result::UploadFailed : Result::Unavailable) : rollback;
        }
    }
    entry.glyphs = glyphs;
    entry.pages = pages;
    entry.admitted = glyphs.size();
    entry.height = height;
    entry.valid = true;
    if (entry.selection.MarkAtlasReady(prepared.useSerial) != selection::Result::Ready) {
        const auto rollback = ReleaseTextures(entry);
        return rollback == Result::Ready ? Result::Unavailable : rollback;
    }
    return Result::Ready;
}

Result Atlas::Select(vgui::Font* font) noexcept {
    if (runtime_.IsOpen() && !runtime_.OnOwnerThread()) {
        return Result::Unavailable;
    }
    if (active_) {
        return Result::Busy;
    }
    if (!rendererValid_ || !runtime_.IsOpen() || !font) {
        return Result::InvalidArgument;
    }
    Active active(active_);
    if (pending_ || retirementPending_) {
        return Result::ReleaseFailed;
    }
    // Runtime owns the genuine baseline images throughout this scope. getPlat
    // is a nonvirtual facade accessor; a cache hit needs no candidate Selection,
    // allocation or loader reference. Use still prepares the held platform.
    Scope scope(runtime_);
    if (scope.Status() != Result::Ready) {
        return scope.Status();
    }
    const auto* platform = font->getPlat();
    Entry* found = nullptr;
    for (const auto& entry : entries_) {
        if (entry->key.moduleEpoch == runtime_.ModuleEpoch()
            && entry->selection.Inspect().platform == platform) {
            found = entry.get();
            break;
        }
    }
    if (!found) {
        pending_.reset(new Entry);
        const auto captured = pending_->selection.Capture(font, runtime_.Owner());
        if (captured != selection::Result::Captured) {
            const auto released = ReleaseEntry(pending_);
            return released == Result::Ready ? Translate(captured) : released;
        }
        pending_->key.moduleEpoch = runtime_.ModuleEpoch();
        found = pending_.get();
        entries_.push_back(std::move(pending_));
    }
    entry_ = found;
    selection::PreparedFont prepared{};
    const auto used = Use(*entry_, prepared);
    if (used == Result::Ready) {
        entry_->selection.EndUse(prepared.useSerial);
    }
    return used;
}

Result Atlas::Draw(const char* text, int length) noexcept {
    if (runtime_.IsOpen() && !runtime_.OnOwnerThread()) {
        return Result::Unavailable;
    }
    if (active_) {
        return Result::Busy;
    }
    if (!rendererValid_) {
        return Result::InvalidArgument;
    }
    if (pending_ || retirementPending_) {
        return Result::ReleaseFailed;
    }
    if (!text || !entry_) {
        return Result::Ready; // Retained null/no-selected-font no-op, not face readiness.
    }
    Active active(active_);
    selection::PreparedFont prepared{};
    const auto used = Use(*entry_, prepared);
    if (used != Result::Ready) {
        return used;
    }
    int x = 0;
    int y = 0;
    renderer_.getPen(renderer_.context, x, y);
    std::int64_t end = x;
    for (int index = 0; index < length; ++index) {
        const auto& glyph = entry_->glyphs[static_cast<unsigned char>(text[index])];
        if (glyph.page >= entry_->pages || glyph.page >= entry_->textures.size()
            || entry_->textures[glyph.page] <= 0 || !IntFits(end + glyph.a)
            || !IntFits(end + glyph.a + glyph.b) || !IntFits(static_cast<std::int64_t>(y) + entry_->height)
            || !IntFits(end + glyph.a + glyph.b + glyph.c)) {
            entry_->selection.EndUse(prepared.useSerial);
            return Result::Capacity;
        }
        end += glyph.a + static_cast<std::int64_t>(glyph.b) + glyph.c;
    }
    for (int index = 0; index < length; ++index) {
        if (!renderer_.live(renderer_.context, prepared.rendererGeneration)) {
            entry_->valid = false;
            entry_->selection.EndUse(prepared.useSerial);
            return Result::Unavailable;
        }
        const auto& glyph = entry_->glyphs[static_cast<unsigned char>(text[index])];
        x += glyph.a;
        Quad quad{x, y, glyph.b, entry_->height, entry_->textures[glyph.page], {}};
        std::memcpy(quad.uv, glyph.uv, sizeof(quad.uv));
        renderer_.draw(renderer_.context, quad);
        x = static_cast<int>(static_cast<std::int64_t>(x) + glyph.b + glyph.c);
    }
    renderer_.setPen(renderer_.context, static_cast<int>(end), y);
    entry_->selection.EndUse(prepared.useSerial);
    return Result::Ready;
}
}
