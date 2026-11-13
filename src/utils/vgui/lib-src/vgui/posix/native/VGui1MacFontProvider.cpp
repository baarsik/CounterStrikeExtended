#include "VGui1MacFontProvider.h"
#include "VGui1NativeFontFace.h"
#include "VGui1FixedSysFontFace.h"
#include "../../common/VGui1TextEncoding.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
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
    // One immutable snapshot per program and file-binding generation. A TTC's
    // different face indexes/sizes/styles share bytes, never mutable FT state.
    std::map<std::string, std::shared_ptr<const std::vector<unsigned char>>> programs;
};

std::shared_ptr<const std::vector<unsigned char>> Program(Context& context, const std::string& path) {
    std::string identity = path;
#if defined(__APPLE__)
    if (char* canonical = realpath(path.c_str(), nullptr)) {
        identity = canonical;
        std::free(canonical);
    }
#endif
    const auto found = context.programs.find(identity);
    if (found != context.programs.end()) {
        return found->second;
    }
    std::vector<unsigned char> bytes;
    if (!context.services.Read(context.services.data, path.c_str(), bytes) || bytes.empty()) {
        return {};
    }
    auto shared = std::make_shared<const std::vector<unsigned char>>(std::move(bytes));
    context.programs.emplace(std::move(identity), shared);
    return shared;
}

#if defined(__APPLE__)
bool LookupNative(void*, FT_Library library, const FontRequest& request, NativeFontLocation& output) noexcept {
    cse::fonts::FontFile file;
    if (!cse::fonts::FindMacFontFile(library, request.lookupIdentity.c_str(), request.weight, (request.flags & FontItalic) != 0, file)) {
        return false;
    }
    output = {std::move(file.path), std::move(file.postScriptName), file.faceIndex};
    return true;
}

bool LookupCharacterNative(void*, FT_Library library, const FontRequest& request, unsigned int character,
    NativeFontLocation& output) noexcept {
    constexpr unsigned int MaximumBmp = 0xffff;
    constexpr unsigned int FirstSurrogate = 0xd800;
    constexpr unsigned int LastSurrogate = 0xdfff;
    if (character > MaximumBmp || (character >= FirstSurrogate && character <= LastSurrogate)) {
        return false;
    }
    const auto unit = static_cast<std::uint16_t>(character);
    cse::fonts::FontFile file;
    if (!cse::fonts::FindMacFontFallback(library, request.lookupIdentity.c_str(), request.weight,
        (request.flags & FontItalic) != 0, &unit, 1, file)) {
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
    return CreateMacFontContextV1({nullptr, LookupNative, ReadNative, AcquireNative, ReleaseNative, LookupCharacterNative});
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

class UnicodeCascadeFace final : public RealizedFace {
public:
    UnicodeCascadeFace(Context& context, const FontRequest& request,
        std::unique_ptr<legacy::NativeFontFace> primary)
        : context_(context), request_(request), primary_(std::move(primary)) {}

    FontMetrics Metrics() const noexcept override {
        return primary_->Metrics();
    }

    bool RawAbc(unsigned int character, int& a, int& b, int& c) noexcept override {
        if (primary_->RawAbc(character, a, b, c)) {
            return true;
        }
        auto* donor = Fallback(character);
        return donor && donor->RawAbc(character, a, b, c);
    }

    bool Raster(unsigned int character, int x, int y, int wide, int tall, unsigned char* rgba) noexcept override {
        int a = 0;
        int b = 0;
        int c = 0;
        if (primary_->RawAbc(character, a, b, c)) {
            return primary_->Raster(character, x, y, wide, tall, rgba);
        }
        auto* donor = Fallback(character);
        return donor && donor->Raster(character, x, y, wide, tall, rgba);
    }

private:
    legacy::NativeFontFace* Fallback(unsigned int character) {
        constexpr unsigned int LegacyByteCount = 256;
        constexpr unsigned int MaximumBmp = 0xffff;
        constexpr unsigned int FirstSurrogate = 0xd800;
        constexpr unsigned int LastSurrogate = 0xdfff;
        // These are fixed bounds per realized font, independent of language
        // revisions or the amount of user/server text encountered.
        constexpr std::size_t MaximumDonors = 16;
        constexpr std::size_t MaximumMissingScalars = 256;
        if (character < LegacyByteCount || !context_.services.LookupCharacter) {
            return nullptr;
        }
        const auto scalar = static_cast<unsigned int>(text::UnicodeValue(static_cast<int>(character)));
        if (scalar > MaximumBmp || (scalar >= FirstSurrogate && scalar <= LastSurrogate)
            || missing_.count(scalar)) {
            return nullptr;
        }
        for (auto& donor : donors_) {
            if (donor.face->GlyphIndex(character)) {
                return donor.face.get();
            }
        }
        NativeFontLocation location;
        const bool found = donors_.size() < MaximumDonors
            && context_.services.LookupCharacter(context_.services.data, context_.library, request_, scalar, location);
        if (found && !location.path.empty() && !location.postScriptName.empty()) {
            // Repeated characters/scripts sharing one program never allocate
            // another owned face. Old fonts retain their donors until teardown.
            for (auto& donor : donors_) {
                if (donor.location.path == location.path && donor.location.faceIndex == location.faceIndex) {
                    return nullptr;
                }
            }
            const auto bytes = Program(context_, location.path);
            if (bytes) {
                FontRequest fallbackRequest = request_;
                fallbackRequest.tall = Metrics().height;
                legacy::Result result{};
                int error = 0;
                std::string diagnostic;
                auto face = legacy::NativeFontFace::CreateShared(bytes, location.faceIndex,
                    location.postScriptName.c_str(), fallbackRequest, result, error, diagnostic);
                // Match the retained line cell, including donors whose Windows
                // ascent/descent rounding adds one pixel to the requested height.
                while (face && face->Metrics().height > Metrics().height && fallbackRequest.tall > 1) {
                    --fallbackRequest.tall;
                    face = legacy::NativeFontFace::CreateShared(bytes, location.faceIndex,
                        location.postScriptName.c_str(), fallbackRequest, result, error, diagnostic);
                }
                int a = 0;
                int b = 0;
                int c = 0;
                if (face && face->RawAbc(character, a, b, c) && b <= Metrics().maximumWidth) {
                    donors_.push_back({std::move(location), std::move(face)});
                    return donors_.back().face.get();
                }
            }
        }
        if (missing_.size() < MaximumMissingScalars) {
            missing_.insert(scalar);
        }
        return nullptr;
    }

    struct Donor {
        NativeFontLocation location;
        std::unique_ptr<legacy::NativeFontFace> face;
    };
    Context& context_; // Provider contract destroys every face before its context.
    FontRequest request_;
    std::unique_ptr<legacy::NativeFontFace> primary_;
    std::vector<Donor> donors_;
    std::set<unsigned int> missing_;
};

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
    error = kReadFailed;
    diagnostic = "unable to snapshot exact native font file: " + location.path;
    const auto bytes = Program(*context, location.path);
    if (!bytes) {
        return {};
    }
    legacy::Result result = legacy::Result::Ok;
    int nativeError = 0;
    if (fixedSysRole) {
        auto fixed = legacy::FixedSysFontFace::CreateOwned(bytes->data(), bytes->size(), location.faceIndex,
            location.postScriptName.c_str(), request, result, nativeError, diagnostic);
        if (!fixed) {
            error = nativeError != 0 ? nativeError : kInvalidFace;
            return {};
        }
        error = 0;
        diagnostic.clear();
        return std::unique_ptr<RealizedFace>(fixed.release());
    }
    auto face = legacy::NativeFontFace::CreateShared(bytes, location.faceIndex,
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
    if ((request.flags & FontSymbol) == 0 && context->services.LookupCharacter) {
        return std::unique_ptr<RealizedFace>(new UnicodeCascadeFace(*context, request, std::move(face)));
    }
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
