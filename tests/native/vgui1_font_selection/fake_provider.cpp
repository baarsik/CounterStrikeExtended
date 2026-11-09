#include "fake_provider.h"
#include <cstdlib>
#include <cstring>

namespace {
ProviderStats stats{};
int releaseFailures = 0;
bool missingSymbol = false;
struct Context {
    int faces = 0;
};
class Face final : public cse_vgui1::RealizedFace {
public:
    Face(Context& context, int height) : context_(context), height_(height) {
        ++context_.faces;
        ++stats.liveFaces;
        ++stats.createdFaces;
    }
    ~Face() override {
        --context_.faces;
        --stats.liveFaces;
        ++stats.destroyedFaces;
    }
    cse_vgui1::FontMetrics Metrics() const noexcept override {
        using namespace cse_vgui1;
        constexpr std::uint32_t AllCapabilities = CapabilityAbc | CapabilityLegacyRgba | CapabilityPenOffsets
            | CapabilityWidth | CapabilityRotation | CapabilityWeight | CapabilityItalic | CapabilityUnderline
            | CapabilityStrikeout | CapabilitySymbol;
        return {height_, 8, AllCapabilities};
    }
    bool RawAbc(unsigned int, int& a, int& b, int& c) noexcept override {
        a = -1;
        b = 6;
        c = 2;
        return true;
    }
    bool Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept override {
        if (x < 0 || y < 0 || x >= wide || y >= tall || !rgba) {
            return false;
        }
        const std::size_t offset = (static_cast<std::size_t>(y) * wide + x) * 4;
        rgba[offset] = static_cast<unsigned char>(byte);
        rgba[offset + 1] = 21;
        rgba[offset + 2] = 37;
        rgba[offset + 3] = 53;
        return true;
    }
private:
    Context& context_;
    int height_;
};
void* Create() noexcept {
    ++stats.contexts;
    return new Context;
}
void Destroy(void* pointer) noexcept {
    auto* context = static_cast<Context*>(pointer);
    if (context->faces != 0) {
        std::abort();
    }
    delete context;
    --stats.contexts;
}
int Register(void*, const void*, const char*) noexcept {
    return 0; // Controlled resource registration; no filesystem/font API call.
}
int Release(void*, const void*) noexcept {
    ++stats.releases;
    if (releaseFailures > 0) {
        --releaseFailures;
        return 71;
    }
    return 0;
}
std::unique_ptr<cse_vgui1::RealizedFace> Realize(void* pointer, const cse_vgui1::FontRequest& request,
    const std::vector<std::string>&, int& error, std::string& diagnostic) noexcept {
    ++stats.realizations;
    if (missingSymbol && (request.flags & cse_vgui1::FontSymbol) != 0) {
        error = 72;
        diagnostic = "controlled missing symbol face";
        return {};
    }
    return std::unique_ptr<cse_vgui1::RealizedFace>(new Face(*static_cast<Context*>(pointer), request.tall));
}
}
namespace cse_vgui1 {
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    static const FontProviderV1 provider{Create, Destroy, Register, Release, Realize};
    return provider;
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) {
    std::abort(); // No Surface/draw provider is qualified or used in this fixture.
}
}
ProviderStats ProviderSnapshot() {
    return stats;
}
void ProviderFailRelease(int count) {
    releaseFailures = count;
}
void ProviderMissingSymbol(bool missing) {
    missingSymbol = missing;
}
