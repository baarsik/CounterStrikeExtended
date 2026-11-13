#ifndef CSE_VGUI1_FIXEDSYS_FONT_FACE_H
#define CSE_VGUI1_FIXEDSYS_FONT_FACE_H

#include "../VGui1FontPlatform.h"
#include "VGui1LegacyRaster.h"

namespace cse_vgui1 {
namespace legacy {

constexpr char FixedSysRoleProfileV1[] = "apple-fixedsys18-courier-cell8x15-gray-v1";

constexpr int FixedSysCellWidthV1 = 8;
constexpr int FixedSysCellHeightV1 = 15;
constexpr int FixedSysAscentV1 = 12;
constexpr int FixedSysDescentV1 = 3;
constexpr int FixedSysInternalLeadingV1 = 3;
constexpr int FixedSysRoleWeightV1 = 400;

enum class FixedSysByteRoute : std::uint32_t {
    ForcedBlank,
    NativeNotdef,
    AuthoredDefaultBlock,
    MappedGraphic
};

struct FixedSysRoleInfo {
    int ascent = FixedSysAscentV1;
    int descent = FixedSysDescentV1;
    int internalLeading = FixedSysInternalLeadingV1;
    int roleWeight = FixedSysRoleWeightV1;
    bool numericWeightKnown = false;
    unsigned int numericWeight = 0;
    int fitPpem = 0;
    int originX = 0;
    int minimumBitmapLeft = 0;
    int maximumBitmapRight = 0;
    int maximumBitmapTop = 0;
    int maximumBitmapBottom = 0;
};

// Unselected exact role candidate. Fixed metrics/default classes are measured
// role policy; gray donor/notdef appearance and readability need UI acceptance.
// The existing owner serializes uses and owns the complete module-use lease.
class FixedSysFontFace final : public RealizedFace {
public:
    static bool Matches(const FontRequest& request) noexcept;
    static std::unique_ptr<FixedSysFontFace> CreateOwned(const void* bytes, std::size_t length,
        long faceIndex, const char* expectedPostScriptName, const FontRequest& request,
        Result& result, int& nativeError, std::string& diagnostic) noexcept;
    ~FixedSysFontFace() override;

    FontMetrics Metrics() const noexcept override;
    bool RawAbc(unsigned int byte, int& a, int& b, int& c) noexcept override;
    bool Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept override;
    bool Route(unsigned int byte, FixedSysByteRoute& route) const noexcept;
    unsigned int GlyphIndex(unsigned int byte) const noexcept;
    const FixedSysRoleInfo& Info() const noexcept;

    FixedSysFontFace(const FixedSysFontFace&) = delete;
    FixedSysFontFace& operator=(const FixedSysFontFace&) = delete;

private:
    struct Impl;
    explicit FixedSysFontFace(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

}
}

#endif
