#ifndef CSE_VGUI1_MAC_FONT_PROVIDER_H
#define CSE_VGUI1_MAC_FONT_PROVIDER_H

#include "../VGui1FontPlatform.h"

struct FT_LibraryRec_;

namespace cse_vgui1 {

struct NativeFontLocation {
    std::string path;
    std::string postScriptName;
    long faceIndex = 0;
};

// Private injected I/O/service boundary. Functions/data are borrowed for the
// complete context lifetime, results own their strings/bytes. Lookup must honor
// the exact request identity/profile and must not invent a fallback. Acquire /
// Release use the existing shared native registration service's 1/0 contract.
struct MacFontServicesV1 {
    void* data;
    bool (*Lookup)(void*, FT_LibraryRec_*, const FontRequest&, NativeFontLocation&) noexcept;
    bool (*Read)(void*, const char* path, std::vector<unsigned char>& bytes) noexcept;
    int (*Acquire)(void*, const void* owner, const char* path) noexcept;
    int (*Release)(void*, const void* owner) noexcept;
};

void* CreateMacFontContextV1(const MacFontServicesV1& services) noexcept;

// Distinctly unqualified. This does not define the owner's required qualified
// factory. Default callbacks use CoreText discovery + the one shared registry;
// the standalone tests only execute explicitly injected owned-data callbacks.
const FontProviderV1& CseVgui1MacFontProviderCandidateV1() noexcept;

}

#endif
