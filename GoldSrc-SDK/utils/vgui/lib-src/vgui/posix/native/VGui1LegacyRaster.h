#ifndef CSE_VGUI1_LEGACY_RASTER_H
#define CSE_VGUI1_LEGACY_RASTER_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace cse_vgui1 {
namespace legacy {

enum class Result : std::int32_t {
    Ok = 0,
    InvalidArgument = -1,
    UnsupportedRequest = -2,
    InvalidFont = -3,
    NativeFailure = -4,
    Unrepresentable = -5
};

struct BgrView {
    const unsigned char* data;
    std::size_t capacity;
    int width;
    int height;
    std::ptrdiff_t pitch;
};

struct RgbaCanvas {
    unsigned char* data;
    std::size_t capacity;
    int width;
    int height;
};

// Value/transfer layer only, not a qualified font provider or capability claim.
// Compile the implementation with strict FP and NoEH. Missing resources use
// statuses; ordinary C++ allocation exhaustion in future owned faces is fatal.
Result ConvertLegacyBgr(const unsigned char* bgr, bool tab, unsigned char* output) noexcept;

// data starts at the allocation's lowest address; negative pitch stores the
// last logical row first. Source and destination spans must not overlap.
// Invalid arguments leave destination bytes unchanged. Valid copies clip only
// at the right/bottom canvas edges, preserving every byte outside that region.
Result TransferLegacyBgr(const BgrView& source, int copiedWidth, int copiedHeight,
    int penX, int penY, const RgbaCanvas& destination, bool tab) noexcept;

struct CandidateMetrics {
    int height = 0;
    int ascent = 0;
    int descent = 0;
    int internalLeading = 0;
    int averageWidth = 0;
    int maximumWidth = 0;
};

struct GlyphAbc {
    int a = 0;
    int b = 0;
    int c = 0;
};

std::uint16_t LegacyByteToUnicode(unsigned char byte) noexcept;

// An isolated, unqualified ordinary-height/width0/angle0/regular candidate.
// Creation success means owned FT data is usable for comparison, not that GDI
// metrics/pixels match. This class exposes no provider binding or capabilities.
class HarmonyCandidate {
public:
    static std::unique_ptr<HarmonyCandidate> CreateOwned(const void* bytes, std::size_t length,
        int cellHeight, Result& result, int& nativeError) noexcept;
    ~HarmonyCandidate();
    const CandidateMetrics& GetMetrics() const noexcept;
    unsigned int GetGlyphIndex(unsigned int byte) const noexcept;
    Result GetAbc(unsigned int byte, GlyphAbc& result) const noexcept;
    Result Raster(unsigned int byte, int penX, int penY, const RgbaCanvas& output) noexcept;

    HarmonyCandidate(const HarmonyCandidate&) = delete;
    HarmonyCandidate& operator=(const HarmonyCandidate&) = delete;

private:
    struct Impl;
    explicit HarmonyCandidate(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

}
}

#endif
