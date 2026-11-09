#if !defined(CSE_VGUI1_ATLAS_FIXTURE_PROVIDER)
#error Controlled fixture provider must never be selected as a production source
#endif
#include "VGui1FontPlatform.h"
#include <cstdlib>

namespace {
int failures = 0;
void (*reenter)() = nullptr;
class Face final : public cse_vgui1::RealizedFace {
public:
    explicit Face(const cse_vgui1::FontRequest& request) : height_(request.tall), width_(request.wide) {}
    cse_vgui1::FontMetrics Metrics() const noexcept override {
        return {height_, width_ > 0 ? width_ : 9, cse_vgui1::CapabilityAbc | cse_vgui1::CapabilityLegacyRgba
            | cse_vgui1::CapabilityPenOffsets | cse_vgui1::CapabilityWidth | cse_vgui1::CapabilityWeight};
    }
    bool RawAbc(unsigned int byte, int& a, int& b, int& c) noexcept override {
        a = width_ > 0 ? 0 : 1;
        b = width_ > 0 ? width_ : byte == 'W' ? 9 : byte == 'i' ? 2 : 4;
        c = 0;
        return true;
    }
    bool Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept override {
        if (reenter) {
            reenter();
        }
        if (!rgba || x < 0 || y < 0 || x >= wide || y >= tall) {
            return false;
        }
        constexpr std::size_t Channels = 4;
        const auto offset = (static_cast<std::size_t>(y) * wide + x) * Channels;
        rgba[offset] = static_cast<unsigned char>(byte);
        rgba[offset + 3] = 255;
        return true;
    }
private:
    int height_;
    int width_;
};
void* Create() noexcept {
    return new int(0);
}
void Destroy(void* context) noexcept {
    delete static_cast<int*>(context);
}
int Register(void*, const void*, const char*) noexcept {
    return 0;
}
int Release(void*, const void*) noexcept {
    if (failures) {
        --failures;
        return 71;
    }
    return 0;
}
std::unique_ptr<cse_vgui1::RealizedFace> Realize(void*, const cse_vgui1::FontRequest& request,
    const std::vector<std::string>&, int& error, std::string& diagnostic) noexcept {
    error = 0;
    diagnostic.clear();
    return std::unique_ptr<cse_vgui1::RealizedFace>(new Face(request));
}
}
namespace cse_vgui1 {
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    // FIXTURE ONLY. Never a production native provider selection.
    static const FontProviderV1 provider{Create, Destroy, Register, Release, Realize};
    return provider;
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) {
    std::abort();
}
}
extern "C" void FixtureFailFontRelease(int count) {
    failures = count;
}
extern "C" void FixtureSetRasterCallback(void (*callback)()) {
    reenter = callback;
}
