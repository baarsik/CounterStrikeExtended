#ifndef CSE_VGUI1_FONT_PLATFORM_H
#define CSE_VGUI1_FONT_PLATFORM_H

#include "VGUI.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vgui {
class SurfacePlat;

// This private interface matches the existing native platform slots. It adds
// no field or virtual function to the public Font class.
class BaseFontPlat {
public:
    virtual ~BaseFontPlat() = default;
    virtual bool equals(const char*, int, int, float, int, bool, bool, bool, bool) = 0;
    virtual void getCharRGBA(int, int, int, int, int, uchar*) = 0;
    virtual void getCharABCwide(int, int&, int&, int&) = 0;
    virtual int getTall() = 0;
    virtual int getWide() = 0;
    virtual void drawSetTextFont(SurfacePlat*) = 0;
};

// Required renderer-side binding. The font component supplies no empty Surface
// implementation; the standalone fixture records and uses the selected font.
void CseVgui1SelectFontForSurfaceV1(SurfacePlat* surface, BaseFontPlat* font);
}

namespace cse_vgui1 {
constexpr std::uint32_t FontItalic = 1U << 0;
constexpr std::uint32_t FontUnderline = 1U << 1;
constexpr std::uint32_t FontStrikeout = 1U << 2;
constexpr std::uint32_t FontSymbol = 1U << 3;
constexpr std::uint32_t CapabilityAbc = 1U << 0;
constexpr std::uint32_t CapabilityLegacyRgba = 1U << 1;
constexpr std::uint32_t CapabilityPenOffsets = 1U << 2;
constexpr std::uint32_t CapabilityWidth = 1U << 3;
constexpr std::uint32_t CapabilityRotation = 1U << 4;
constexpr std::uint32_t CapabilityWeight = 1U << 5;
constexpr std::uint32_t CapabilityItalic = 1U << 6;
constexpr std::uint32_t CapabilityUnderline = 1U << 7;
constexpr std::uint32_t CapabilityStrikeout = 1U << 8;
constexpr std::uint32_t CapabilitySymbol = 1U << 9;
constexpr std::uint32_t LegacyAnsiCodePage = 1252;

struct FontRequest {
    std::string requestedName;
    std::string lookupIdentity;
    std::string resolutionProfile;
    int tall = 0;
    int wide = 0;
    float rotation = 0;
    int rotationTenths = 0;
    int weight = 0;
    std::uint32_t flags = 0;
    std::uint32_t ansiCodePage = LegacyAnsiCodePage;
};

struct FontMetrics {
    int height = 0;
    int maximumWidth = 0;
    std::uint32_t capabilities = 0;
};

class RealizedFace {
public:
    virtual ~RealizedFace() = default;
    virtual FontMetrics Metrics() const noexcept = 0;
    virtual bool RawAbc(unsigned int byte, int& a, int& b, int& c) noexcept = 0;
    virtual bool Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept = 0;
};

// Private provider seam, not a public plugin API. Its context is owned by the
// module state: faces die first, leases release next, then DestroyContext.
// Register/Release return zero on success, otherwise a retained native error.
// Realize must resolve lookupIdentity under resolutionProfile without another
// implicit family substitution. It must own all raster data for the face's
// lifetime and report unsupported legacy features through its capabilities.
// Ordinary missing resources use status/errors. Bookkeeping uses the inherited
// NoEH allocation policy: allocation exhaustion is process-fatal. No exception
// may cross these noexcept provider or exported service boundaries.
struct FontProviderV1 {
    void* (*CreateContext)() noexcept;
    void (*DestroyContext)(void*) noexcept;
    int (*RegisterFile)(void*, const void* owner, const char* path) noexcept;
    int (*ReleaseOwner)(void*, const void* owner) noexcept;
    std::unique_ptr<RealizedFace> (*Realize)(void*, const FontRequest&,
        const std::vector<std::string>& committedFiles, int& error, std::string& diagnostic) noexcept;
};

// Required qualified-provider binding. This initial owner component deliberately
// contains no fake/default-success native provider. Fixture definitions are not
// production sources; native provider selection needs its own reviewed gate.
#if defined(__APPLE__)
__attribute__((visibility("hidden")))
#endif
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept;

char* OwnFontName(const char* name);
vgui::BaseFontPlat* FindCachedPlatform(const char* name, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol);
vgui::BaseFontPlat* CreatePlatform(const char* name, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol);
void AdoptBitmapPlatform(vgui::BaseFontPlat* platform, const char* name);
void RecordBitmapFailure(const char* name);
void RequireReady(vgui::BaseFontPlat* platform);
[[noreturn]] void InvalidLegacyUse(const char* message);
int NextFontId();
void ResetLookup();
}

#endif
