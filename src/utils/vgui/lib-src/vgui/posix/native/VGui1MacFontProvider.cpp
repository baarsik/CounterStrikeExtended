#include "VGui1MacFontProvider.h"
#include "VGui1NativeFontFace.h"
#include "VGui1FixedSysFontFace.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <ft2build.h>
#include FT_FREETYPE_H
#if defined(__APPLE__)
#include "MacFontDiscovery.h"
#include "MacFontRegistration.h"
#endif

namespace cse_vgui1 {
namespace {
constexpr int kInvalidArgument = -2101;
constexpr int kRegistrationFailed = -2102;
constexpr int kReleaseFailed = -2103;
constexpr int kUnavailable = -2104;
constexpr int kReadFailed = -2105;
constexpr int kInvalidFace = -2106;

struct Context {
    explicit Context(const MacFontServicesV1& callbacks) : services(callbacks) {}
    ~Context() {
        if (library) {
            FT_Done_FreeType(library);
        }
    }
    MacFontServicesV1 services;
    FT_Library library = nullptr;
    std::map<const void*, std::set<std::string>> owners;
};

#if defined(__APPLE__)
bool LookupNative(void*, FT_Library library, const FontRequest& request, NativeFontLocation& output) noexcept {
    cse::fonts::FontFile file;
    if (!cse::fonts::FindMacFontFile(library, request.lookupIdentity.c_str(), request.weight, (request.flags & FontItalic) != 0, file)) {
        return false;
    }
    output = {std::move(file.path), std::move(file.postScriptName), file.faceIndex};
    return true;
}

bool ReadNative(void*, const char* path, std::vector<unsigned char>& output) noexcept {
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return false;
    }
    bool ok = fseeko(file, 0, SEEK_END) == 0;
    const off_t length = ok ? ftello(file) : -1;
    ok = ok && length > 0
        && static_cast<std::uintmax_t>(length) <= static_cast<std::uintmax_t>((std::numeric_limits<FT_Long>::max)())
        && static_cast<std::uintmax_t>(length)
            <= static_cast<std::uintmax_t>((std::numeric_limits<std::ptrdiff_t>::max)())
        && static_cast<std::uintmax_t>(length) <= output.max_size() && fseeko(file, 0, SEEK_SET) == 0;
    std::vector<unsigned char> bytes;
    if (ok) {
        bytes.resize(static_cast<std::size_t>(length));
        ok = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    }
    const bool closed = std::fclose(file) == 0;
    if (!ok || !closed) {
        return false;
    }
    output = std::move(bytes);
    return true;
}

int AcquireNative(void*, const void* owner, const char* path) noexcept {
    return CseMacFontRegistrationAcquire(owner, path);
}

int ReleaseNative(void*, const void* owner) noexcept {
    return CseMacFontRegistrationReleaseOwner(owner);
}
#endif

void* CreateContext() noexcept {
#if defined(__APPLE__)
    return CreateMacFontContextV1({nullptr, LookupNative, ReadNative, AcquireNative, ReleaseNative});
#else
    // This is an Apple provider. Other platforms may compile its injected data
    // fixture, but never receive a fake native service implementation.
    return nullptr;
#endif
}

void DestroyContext(void* value) noexcept {
    auto* context = static_cast<Context*>(value);
    if (context && !context->owners.empty()) {
        // The frozen owner calls this only after successful release. Retain
        // tokens/context if misused; root must refuse unload/rebind on failure.
        std::fputs("VGUI1 native font context still owns registration tokens; unload is unsafe\n", stderr);
        return;
    }
    delete context;
}

int RegisterFile(void* value, const void* owner, const char* path) noexcept {
    auto* context = static_cast<Context*>(value);
    if (!context || !owner || !path || path[0] != '/') {
        return kInvalidArgument;
    }
    // Allocate bookkeeping before registration, then roll it back if acquire
    // fails. The shared service publishes no new ownership on failure; the
    // common owner need not release a batch whose first acquire failed.
    auto& paths = context->owners[owner];
    const auto inserted = paths.insert(path);
    if (context->services.Acquire(context->services.data, owner, path) == 0) {
        if (inserted.second) {
            paths.erase(inserted.first);
        }
        if (paths.empty()) {
            context->owners.erase(owner);
        }
        return kRegistrationFailed;
    }
    return 0;
}

int ReleaseOwner(void* value, const void* owner) noexcept {
    auto* context = static_cast<Context*>(value);
    if (!context || !owner) {
        return kInvalidArgument;
    }
    if (context->owners.count(owner) == 0) {
        return 0;
    }
    if (context->services.Release(context->services.data, owner) == 0) {
        return kReleaseFailed;
    }
    context->owners.erase(owner);
    return 0;
}

bool EqualAscii(const std::string& left, const char* right) {
    std::size_t index = 0;
    for (; index < left.size() && right[index]; ++index) {
        const auto a = static_cast<unsigned char>(left[index]);
        const auto b = static_cast<unsigned char>(right[index]);
        const auto foldedA = a >= 'A' && a <= 'Z' ? a - 'A' + 'a' : a;
        const auto foldedB = b >= 'A' && b <= 'Z' ? b - 'A' + 'a' : b;
        if (foldedA != foldedB) {
            return false;
        }
    }
    return index == left.size() && right[index] == 0;
}

std::unique_ptr<RealizedFace> Realize(void* value, const FontRequest& request,
    const std::vector<std::string>& files, int& error, std::string& diagnostic) noexcept {
    auto* context = static_cast<Context*>(value);
    error = kInvalidArgument;
    diagnostic = "missing native font context/exact identity";
    if (!context || request.lookupIdentity.empty()) {
        return {};
    }
    const bool fixedSysRole = request.resolutionProfile == legacy::FixedSysRoleProfileV1;
    if (fixedSysRole && !legacy::FixedSysFontFace::Matches(request)) {
        diagnostic = "request does not match the explicitly tagged FixedSys role";
        return {};
    }
    NativeFontLocation location;
    error = kUnavailable;
    diagnostic = "exact native font unavailable: " + request.lookupIdentity;
    bool found = context->services.Lookup(context->services.data, context->library, request, location);
    // A family may have no requested styled face. Keep its regular donor and
    // synthesize the requested effects in the owned FreeType face.
    if (!found && !fixedSysRole && (request.flags & FontSymbol) == 0) {
        auto donor = request;
        donor.flags &= ~FontItalic;
        donor.weight = 0;
        found = context->services.Lookup(context->services.data, context->library, donor, location);
        if (!found) {
            // GDI substitutes absent ordinary names. Keep that policy here;
            // symbol maps and the measured FixedSys role must never substitute.
            constexpr const char* kFallbackFamilies[]{"Arial", "Helvetica"};
            for (const char* family : kFallbackFamilies) {
                donor.lookupIdentity = family;
                found = context->services.Lookup(context->services.data, context->library, donor, location);
                if (found) {
                    break;
                }
            }
        }
    }
    if (!found || location.path.empty() || location.path[0] != '/' || location.postScriptName.empty()) {
        return {};
    }
    // Default Marlett must use the explicitly approved/bound game resource.
    // A same-named installed program is not an implicit symbol substitution.
    if ((request.flags & FontSymbol) != 0 && EqualAscii(request.requestedName, "Marlett")
        && std::find(files.begin(), files.end(), location.path) == files.end()) {
        diagnostic = "Marlett requires its explicitly bound game resource";
        return {};
    }
    std::vector<unsigned char> bytes;
    error = kReadFailed;
    diagnostic = "unable to snapshot exact native font file: " + location.path;
    if (!context->services.Read(context->services.data, location.path.c_str(), bytes)) {
        return {};
    }
    legacy::Result result = legacy::Result::Ok;
    int nativeError = 0;
    if (fixedSysRole) {
        auto fixed = legacy::FixedSysFontFace::CreateOwned(bytes.data(), bytes.size(), location.faceIndex,
            location.postScriptName.c_str(), request, result, nativeError, diagnostic);
        if (!fixed) {
            error = nativeError != 0 ? nativeError : kInvalidFace;
            return {};
        }
        error = 0;
        diagnostic.clear();
        return std::unique_ptr<RealizedFace>(fixed.release());
    }
    auto face = legacy::NativeFontFace::CreateOwned(bytes.data(), bytes.size(), location.faceIndex,
        location.postScriptName.c_str(), request, result, nativeError, diagnostic);
    if (!face) {
        error = nativeError != 0 ? nativeError : kInvalidFace;
        return {};
    }
    if ((request.flags & FontSymbol) != 0 && EqualAscii(request.requestedName, "Marlett")) {
        constexpr unsigned int kWidgetBytes[]{'1', '2', 'a', 'c', 'd', 'e', 'f', 'g',
            'h', 'j', 'k', 'l', 'm', 'n', 'r', 's'};
        for (const unsigned int byte : kWidgetBytes) {
            if (face->GlyphIndex(byte) == 0) {
                error = kInvalidFace;
                diagnostic = "bound Marlett lacks a required widget byte";
                return {};
            }
        }
    }
    error = 0;
    diagnostic.clear();
    return std::unique_ptr<RealizedFace>(face.release());
}
}

void* CreateMacFontContextV1(const MacFontServicesV1& services) noexcept {
    if (!services.Lookup || !services.Read || !services.Acquire || !services.Release) {
        return nullptr;
    }
    std::unique_ptr<Context> context(new Context(services));
    if (FT_Init_FreeType(&context->library) != 0) {
        return nullptr;
    }
    return context.release();
}

const FontProviderV1& CseVgui1MacFontProviderCandidateV1() noexcept {
    static const FontProviderV1 provider{CreateContext, DestroyContext, RegisterFile, ReleaseOwner, Realize};
    return provider;
}

}
