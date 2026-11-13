#include "MacFontDiscovery.h"

#include <CoreText/CoreText.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstring>
#include <utility>

namespace cse::fonts {

namespace {

template <typename T> class OwnedCF {
public:
    explicit OwnedCF(T value = nullptr) : value_(value) {}
    ~OwnedCF() {
        if (value_) {
            CFRelease(value_);
        }
    }
    OwnedCF(const OwnedCF&) = delete;
    OwnedCF& operator=(const OwnedCF&) = delete;
    T get() const {
        return value_;
    }

private:
    T value_;
};

std::string CopyUtf8(CFStringRef value) {
    if (!value) {
        return {};
    }
    const CFIndex capacity = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1;
    if (capacity <= 0) {
        return {};
    }
    std::string text(static_cast<std::size_t>(capacity), '\0');
    if (!CFStringGetCString(value, text.data(), capacity, kCFStringEncodingUTF8)) {
        return {};
    }
    text.resize(std::strlen(text.c_str()));
    return text;
}

CTFontDescriptorRef MatchName(CFStringRef name, CFStringRef attribute) {
    const void* keys[] = {attribute};
    const void* values[] = {name};
    OwnedCF<CFDictionaryRef> attributes(CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    OwnedCF<CFSetRef> mandatory(CFSetCreate(kCFAllocatorDefault, keys, 1, &kCFTypeSetCallBacks));
    if (!attributes.get() || !mandatory.get()) {
        return nullptr;
    }
    OwnedCF<CTFontDescriptorRef> query(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return query.get() ? CTFontDescriptorCreateMatchingFontDescriptor(query.get(), mandatory.get()) : nullptr;
}

CTFontRef CreateFont(const char* name, int weight, bool italic) {
    constexpr std::size_t maxNameBytes = 1024;
    if (!name || !name[0] || strnlen(name, maxNameBytes + 1) > maxNameBytes) {
        return nullptr;
    }
    OwnedCF<CFStringRef> requested(CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8));
    if (!requested.get()) {
        return nullptr;
    }
    // Family matching preserves localized Unicode names; PostScript matching also
    // accepts the stable face identities returned by fallback discovery.
    CTFontDescriptorRef matched = MatchName(requested.get(), kCTFontFamilyNameAttribute);
    bool exactPostScriptName = false;
    if (!matched) {
        matched = MatchName(requested.get(), kCTFontNameAttribute);
        exactPostScriptName = true;
    }
    OwnedCF<CTFontDescriptorRef> descriptor(matched);
    if (!descriptor.get()) {
        return nullptr;
    }
    // Point size is irrelevant to file discovery; FreeType keeps the existing pixel-size policy.
    constexpr CGFloat discoveryPointSize = 12.0;
    OwnedCF<CTFontRef> base(CTFontCreateWithFontDescriptorAndOptions(descriptor.get(), discoveryPointSize, nullptr,
        kCTFontOptionsPreventAutoActivation));
    if (!base.get()) {
        return nullptr;
    }
    if (exactPostScriptName) {
        // A PostScript name identifies an already selected face. Reapplying the
        // primary font's italic traits can discard a valid native CJK fallback.
        return static_cast<CTFontRef>(CFRetain(base.get()));
    }
    constexpr int boldWeight = 700;
    CTFontSymbolicTraits traits = weight >= boldWeight ? kCTFontBoldTrait : 0;
    if (italic) {
        traits |= kCTFontItalicTrait;
    }
    constexpr CTFontSymbolicTraits styleMask = kCTFontBoldTrait | kCTFontItalicTrait;
    return CTFontCreateCopyWithSymbolicTraits(base.get(), discoveryPointSize, nullptr, traits, styleMask);
}

bool FindFaceIndex(FT_Library library, FontFile& file) {
    FT_Face metadata = nullptr;
    if (FT_New_Face(library, file.path.c_str(), -1, &metadata)) {
        return false;
    }
    const FT_Long faceCount = metadata->num_faces;
    FT_Done_Face(metadata);
    // FreeType encodes the collection face in the low 16 bits and a named
    // variable-font instance in the following 15 bits of face_index.
    constexpr FT_Long instanceShift = 16;
    constexpr FT_Long maxCollectionFaces = 1L << instanceShift;
    constexpr FT_Long namedInstanceMask = 0x7fff;
    if (faceCount <= 0 || faceCount > maxCollectionFaces) {
        return false;
    }
    for (FT_Long faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
        FT_Long instanceCount = 0;
        for (FT_Long instance = 0; instance <= instanceCount; ++instance) {
            const FT_Long index = faceIndex | (instance << instanceShift);
            FT_Face face = nullptr;
            if (FT_New_Face(library, file.path.c_str(), index, &face)) {
                continue;
            }
            if (instance == 0) {
                instanceCount = (face->style_flags >> instanceShift) & namedInstanceMask;
            }
            const char* name = FT_Get_Postscript_Name(face);
            const bool matches = name && file.postScriptName == name;
            FT_Done_Face(face);
            if (matches) {
                file.faceIndex = index;
                return true;
            }
        }
    }
    return false;
}

bool CopyFontFile(FT_Library library, CTFontRef font, FontFile& result) {
    OwnedCF<CFTypeRef> attribute(CTFontCopyAttribute(font, kCTFontURLAttribute));
    if (!attribute.get() || CFGetTypeID(attribute.get()) != CFURLGetTypeID()) {
        return false;
    }
    const auto url = static_cast<CFURLRef>(attribute.get());
    OwnedCF<CFStringRef> scheme(CFURLCopyScheme(url));
    if (!scheme.get() || !CFEqual(scheme.get(), CFSTR("file"))) {
        return false;
    }
    OwnedCF<CFStringRef> path(CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle));
    OwnedCF<CFStringRef> name(CTFontCopyPostScriptName(font));
    FontFile file;
    file.path = CopyUtf8(path.get());
    file.postScriptName = CopyUtf8(name.get());
    if (file.path.empty() || file.postScriptName.empty() || file.postScriptName == "LastResort"
        || !FindFaceIndex(library, file)) {
        return false;
    }
    result = std::move(file);
    return true;
}

bool IsSingleUtf16Scalar(const std::uint16_t* text, std::size_t length) {
    constexpr std::size_t maxUtf16ScalarUnits = 2;
    constexpr std::uint16_t firstHighSurrogate = 0xd800;
    constexpr std::uint16_t lastHighSurrogate = 0xdbff;
    constexpr std::uint16_t firstLowSurrogate = 0xdc00;
    constexpr std::uint16_t lastLowSurrogate = 0xdfff;
    if (!text || length == 0 || length > maxUtf16ScalarUnits) {
        return false;
    }
    if (length == 1) {
        return text[0] < firstHighSurrogate || text[0] > lastLowSurrogate;
    }
    return text[0] >= firstHighSurrogate && text[0] <= lastHighSurrogate
        && text[1] >= firstLowSurrogate && text[1] <= lastLowSurrogate;
}

}

bool FindMacFontFile(FT_Library library, const char* name, int weight, bool italic, FontFile& result) {
    result = {};
    if (!library) {
        return false;
    }
    OwnedCF<CTFontRef> font(CreateFont(name, weight, italic));
    return font.get() && CopyFontFile(library, font.get(), result);
}

bool FindMacFontFallback(FT_Library library, const char* name, int weight, bool italic,
    const std::uint16_t* text, std::size_t length, FontFile& result) {
    result = {};
    if (!library || !IsSingleUtf16Scalar(text, length)) {
        return false;
    }
    OwnedCF<CTFontRef> base(CreateFont(name, weight, italic));
    OwnedCF<CFStringRef> characters(CFStringCreateWithCharacters(kCFAllocatorDefault, text,
        static_cast<CFIndex>(length)));
    if (!base.get() || !characters.get()) {
        return false;
    }
    OwnedCF<CTFontRef> fallback(CTFontCreateForString(base.get(), characters.get(),
        CFRangeMake(0, static_cast<CFIndex>(length))));
    if (!fallback.get()) {
        return false;
    }
    // CoreText may return LastResort for an unsupported scalar; do not present
    // such a placeholder as a covering font to the existing FreeType renderer.
    constexpr std::size_t maxUtf16ScalarUnits = 2;
    CGGlyph glyphs[maxUtf16ScalarUnits] = {};
    if (CTFontGetGlyphsForCharacters(fallback.get(), text, glyphs, static_cast<CFIndex>(length))
        && CopyFontFile(library, fallback.get(), result)) {
        return true;
    }
    // Some system releases expose CoreText-only font data. Stay with the
    // ordered native cascade and require a covering, FreeType-readable face.
    OwnedCF<CFArrayRef> cascade(CTFontCopyDefaultCascadeListForLanguages(base.get(), nullptr));
    if (!cascade.get()) {
        return false;
    }
    constexpr CGFloat discoveryPointSize = 12.0;
    for (CFIndex index = 0; index < CFArrayGetCount(cascade.get()); ++index) {
        const auto descriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(cascade.get(), index));
        OwnedCF<CTFontRef> candidate(CTFontCreateWithFontDescriptorAndOptions(descriptor, discoveryPointSize, nullptr,
            kCTFontOptionsPreventAutoActivation));
        if (candidate.get()
            && CTFontGetGlyphsForCharacters(candidate.get(), text, glyphs, static_cast<CFIndex>(length))
            && CopyFontFile(library, candidate.get(), result)) {
            return true;
        }
    }
    return false;
}

}
