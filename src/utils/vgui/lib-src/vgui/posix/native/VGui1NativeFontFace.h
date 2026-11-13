#ifndef CSE_VGUI1_NATIVE_FONT_FACE_H
#define CSE_VGUI1_NATIVE_FONT_FACE_H

#include "../VGui1FontPlatform.h"
#include "VGui1LegacyRaster.h"

namespace cse_vgui1 {
namespace legacy {

// Unselected native metric/raster candidate. Success describes the owned face,
// not GDI metric/pixel equivalence or UI layout acceptance. Calls are serialized
// by the existing font owner; this object retains no borrowed input/FT slot.
class NativeFontFace final : public RealizedFace {
public:
    static std::unique_ptr<NativeFontFace> CreateOwned(const void* bytes, std::size_t length,
        long faceIndex, const char* expectedPostScriptName, const FontRequest& request,
        Result& result, int& nativeError, std::string& diagnostic) noexcept;
    ~NativeFontFace() override;

    FontMetrics Metrics() const noexcept override;
    bool RawAbc(unsigned int byte, int& a, int& b, int& c) noexcept override;
    bool Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept override;
    const CandidateMetrics& DetailedMetrics() const noexcept;
    unsigned int GlyphIndex(unsigned int byte) const noexcept;
    long FaceIndex() const noexcept;
    const std::string& PostScriptName() const noexcept;
    unsigned int IntrinsicWeight() const noexcept;

    NativeFontFace(const NativeFontFace&) = delete;
    NativeFontFace& operator=(const NativeFontFace&) = delete;

private:
    struct Impl;
    explicit NativeFontFace(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

}
}

#endif
