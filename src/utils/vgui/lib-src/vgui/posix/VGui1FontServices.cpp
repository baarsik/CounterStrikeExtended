#if !defined(__APPLE__)
#error This private VGUI1 font owner must be selected only for Apple targets
#endif

#include "VGui1FontServices.h"
#include "VGui1FontPlatform.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <type_traits>

namespace {
using cse_vgui1::FontMetrics;
using cse_vgui1::FontProviderV1;
using cse_vgui1::FontRequest;
using cse_vgui1::RealizedFace;
using State = CseVgui1FontStateV1;
using Result = CseVgui1FontResultV1;
constexpr std::size_t kMaximumNameBytes = 1024;
constexpr int kInitialFontId = 100;
constexpr int kInvalidRequest = -1001;
constexpr int kInvalidMetrics = -1002;
constexpr int kMissingCapabilities = -1003;
constexpr int kMissingFace = -1004;
constexpr int kInvalidBitmap = -1005;
constexpr unsigned int kLegacyByteCount = 256;
constexpr unsigned int kFloatFractionBits = 23;
constexpr unsigned int kFloatExponentMask = 0xff;
constexpr unsigned int kFloatExponentBias = 127;
constexpr std::uint32_t kFloatFractionMask = 0x7fffff;
constexpr std::uint32_t kFloatSignMask = 0x80000000;
constexpr std::uint32_t kUtf8AsciiLimit = 0x80;
constexpr std::uint32_t kUtf8ContinuationMaximum = 0xbf;
constexpr std::uint32_t kUtf8TwoByteFirst = 0xc2;
constexpr std::uint32_t kUtf8TwoByteLast = 0xdf;
constexpr std::uint32_t kUtf8ThreeByteFirst = 0xe0;
constexpr std::uint32_t kUtf8ThreeByteLast = 0xef;
constexpr std::uint32_t kUtf8FourByteFirst = 0xf0;
constexpr std::uint32_t kUtf8FourByteLast = 0xf4;
constexpr std::uint32_t kUtf8TwoByteMask = 0x1f;
constexpr std::uint32_t kUtf8ThreeByteMask = 0x0f;
constexpr std::uint32_t kUtf8FourByteMask = 0x07;
constexpr std::uint32_t kUtf8ContinuationMask = 0x3f;
constexpr unsigned int kUtf8ContinuationBits = 6;
constexpr std::uint32_t kFirstThreeByteScalar = 0x800;
constexpr std::uint32_t kFirstFourByteScalar = 0x10000;
constexpr std::uint32_t kLastUnicodeScalar = 0x10ffff;
constexpr std::uint32_t kFirstSurrogate = 0xd800;
constexpr std::uint32_t kLastSurrogate = 0xdfff;

static_assert(sizeof(CseVgui1FontStatusV1) == 24 && std::is_standard_layout<CseVgui1FontStatusV1>::value,
    "Private V1 status layout");

void WriteDiagnostic(char* output, std::size_t capacity, const std::string& text) {
    if (output && capacity > 0) {
        const std::size_t copied = (std::min)(text.size(), capacity - 1);
        std::memcpy(output, text.data(), copied);
        output[copied] = 0;
    }
}

bool ValidOutput(char* output, std::size_t capacity) {
    return output || capacity == 0;
}

unsigned char FoldAscii(unsigned char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value - 'A' + 'a') : value;
}

bool EqualName(const char* left, const std::string& right) {
    if (!left) {
        return false;
    }
    std::size_t index = 0;
    while (left[index] && index < right.size()) {
        if (FoldAscii(static_cast<unsigned char>(left[index]))
            != FoldAscii(static_cast<unsigned char>(right[index]))) {
            return false;
        }
        ++index;
    }
    return index == right.size() && left[index] == 0;
}

bool ValidUtf8(const char* text, std::size_t maximum) {
    if (!text || !text[0]) {
        return false;
    }
    std::size_t index = 0;
    while (text[index]) {
        if (index >= maximum) {
            return false;
        }
        const auto first = static_cast<unsigned char>(text[index++]);
        if (first < kUtf8AsciiLimit) {
            continue;
        }
        unsigned int remaining;
        std::uint32_t value;
        std::uint32_t minimum;
        if (first >= kUtf8TwoByteFirst && first <= kUtf8TwoByteLast) {
            remaining = 1;
            value = first & kUtf8TwoByteMask;
            minimum = kUtf8AsciiLimit;
        } else if (first >= kUtf8ThreeByteFirst && first <= kUtf8ThreeByteLast) {
            remaining = 2;
            value = first & kUtf8ThreeByteMask;
            minimum = kFirstThreeByteScalar;
        } else if (first >= kUtf8FourByteFirst && first <= kUtf8FourByteLast) {
            remaining = 3;
            value = first & kUtf8FourByteMask;
            minimum = kFirstFourByteScalar;
        } else {
            return false;
        }
        while (remaining > 0) {
            if (index >= maximum) {
                return false;
            }
            const auto next = static_cast<unsigned char>(text[index]);
            if (next < kUtf8AsciiLimit || next > kUtf8ContinuationMaximum) {
                return false;
            }
            value = (value << kUtf8ContinuationBits) | (next & kUtf8ContinuationMask);
            ++index;
            --remaining;
        }
        if (value < minimum || value > kLastUnicodeScalar || (value >= kFirstSurrogate && value <= kLastSurrogate)) {
            return false;
        }
    }
    return true;
}

bool RotationTenths(float rotation, int& result) {
    // Exact truncation of the retained float -> double * 10 -> int operation.
    // Integer decoding also remains valid under inherited fast-FP compilation.
    static_assert(sizeof(rotation) == sizeof(std::uint32_t)
        && std::numeric_limits<float>::is_iec559, "Selected rotation representation");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &rotation, sizeof(bits));
    const auto exponent = (bits >> kFloatFractionBits) & kFloatExponentMask;
    if (exponent == kFloatExponentMask) {
        return false;
    }
    std::uint64_t mantissa = bits & kFloatFractionMask;
    int shift = 1 - static_cast<int>(kFloatExponentBias) - static_cast<int>(kFloatFractionBits);
    if (exponent != 0) {
        mantissa |= std::uint64_t{1} << kFloatFractionBits;
        shift = static_cast<int>(exponent) - static_cast<int>(kFloatExponentBias)
            - static_cast<int>(kFloatFractionBits);
    }
    constexpr std::uint64_t kTenthsPerDegree = 10;
    std::uint64_t magnitude = mantissa * kTenthsPerDegree;
    if (shift >= 0) {
        if (shift >= 63 || magnitude > (std::numeric_limits<std::uint64_t>::max)() >> shift) {
            return false;
        }
        magnitude <<= static_cast<unsigned int>(shift);
    } else {
        magnitude = shift <= -64 ? 0 : magnitude >> static_cast<unsigned int>(-shift);
    }
    const bool negative = (bits & kFloatSignMask) != 0;
    const std::uint64_t limit = static_cast<std::uint64_t>(INT_MAX) + static_cast<unsigned int>(negative);
    if (magnitude > limit) {
        return false;
    }
    if (negative && magnitude == static_cast<std::uint64_t>(INT_MAX) + 1) {
        result = INT_MIN;
    } else {
        result = negative ? -static_cast<int>(magnitude) : static_cast<int>(magnitude);
    }
    return true;
}

std::uint32_t RequiredCapabilities(const FontRequest& request) {
    using namespace cse_vgui1;
    std::uint32_t required = CapabilityAbc | CapabilityLegacyRgba | CapabilityPenOffsets | CapabilityWeight;
    if (request.wide != 0) {
        required |= CapabilityWidth;
    }
    if (request.rotationTenths != 0) {
        required |= CapabilityRotation;
    }
    if (request.flags & FontItalic) {
        required |= CapabilityItalic;
    }
    if (request.flags & FontUnderline) {
        required |= CapabilityUnderline;
    }
    if (request.flags & FontStrikeout) {
        required |= CapabilityStrikeout;
    }
    if (request.flags & FontSymbol) {
        required |= CapabilitySymbol;
    }
    return required;
}

void ApplyPinnedResolution(FontRequest& request) {
    request.lookupIdentity = request.requestedName;
    request.resolutionProfile = "exact-request-v1";
    // Exact tuples from the sealed VM1 real-GDI V3 packet; no general unknown-
    // family fallback is inferred from them. Width/angle/style neighbors remain
    // exact requests until their own measured resolution entry is added.
    constexpr int kReferenceHeight = 20;
    constexpr int kReferenceRegularWeight = 400;
    constexpr int kReferenceVerdanaWeight = 500;
#if defined(__APPLE__)
    // Preserve the measured FixedSys cell role through one explicit native
    // profile. Neighboring requests retain their own identity and capabilities.
    constexpr int kFixedSysHeight = 18;
    if (request.tall == kFixedSysHeight && request.wide == 0 && request.rotation == 0.0f
        && request.rotationTenths == 0 && request.weight == kReferenceRegularWeight && request.flags == 0
        && request.ansiCodePage == cse_vgui1::LegacyAnsiCodePage && EqualName("FixedSys", request.requestedName)) {
        request.lookupIdentity = "Courier";
        request.resolutionProfile = "apple-fixedsys18-courier-cell8x15-gray-v1";
        return;
    }
#endif
    const bool referenceGeometry = request.tall == kReferenceHeight && request.wide == 0 && request.rotation == 0.0f;
    if (referenceGeometry && request.weight == kReferenceRegularWeight && request.flags == 0
        && EqualName("Arial Narrow Bold", request.requestedName)) {
        request.lookupIdentity = "ArialMT";
        request.resolutionProfile = "vm1-gdi-20261109-v3-ArialNarrowBold-20-400";
    } else if (referenceGeometry && request.weight == kReferenceRegularWeight && request.flags == cse_vgui1::FontSymbol
        && EqualName("Arial", request.requestedName)) {
        request.lookupIdentity = "Wingdings-Regular";
        request.resolutionProfile = "vm1-gdi-20261109-v3-Arial-symbol-20-400";
    } else if (referenceGeometry && request.weight == kReferenceVerdanaWeight && request.flags == 0
        && EqualName("Verdana Bold", request.requestedName)) {
        request.lookupIdentity = "Verdana-Bold";
        request.resolutionProfile = "vm1-gdi-20261109-v3-VerdanaBold-20-500";
    }
}

struct Record {
    FontRequest request;
    std::unique_ptr<vgui::BaseFontPlat> platform;
    std::unique_ptr<RealizedFace> face;
    State state = State::PendingResources;
    std::uint64_t lookupGeneration = 0;
    std::uint64_t attemptedGeneration = 0;
    int nativeError = 0;
    std::string diagnostic;
    FontMetrics metrics;
    bool bitmap = false;
    bool validRequest = true;
};

struct RegistrationBatch {
    std::vector<std::string> paths;
};

struct OwnedName {
    void operator()(char* value) const {
        std::free(value);
    }
};

struct ModuleData {
    std::recursive_mutex mutex;
    std::vector<std::unique_ptr<Record>> records;
    std::vector<std::unique_ptr<char, OwnedName>> names;
    std::vector<std::unique_ptr<RegistrationBatch>> batches;
    std::vector<std::string> files;
    const FontProviderV1* provider = nullptr;
    void* context = nullptr;
    std::uint64_t catalogGeneration = 0;
    std::uint64_t lookupGeneration = 1;
    int nextId = kInitialFontId;
    bool bound = false;
    bool releasePending = false;
};

Result ReleaseFiles(ModuleData& data, std::string& diagnostic);

class ModuleState {
public:
    ModuleState() : data_(new ModuleData) {}
    ~ModuleState() {
        std::string diagnostic;
        if (ReleaseFiles(*data_, diagnostic) != Result::FilesReleased) {
            // The integration must refuse dlclose after ReleaseFailed. At
            // process exit it may leave process-owned registrations to the OS;
            // preserve their context/token addresses without aborting. This
            // does not make an explicit permanent-failure unload safe.
            std::fprintf(stderr, "VGUI1 font shutdown could not release owned files: %s\n", diagnostic.c_str());
            data_.release();
        }
    }
    ModuleData& data() {
        return *data_;
    }

private:
    std::unique_ptr<ModuleData> data_;
};

// Eager empty owner precedes every request in a dependent consumer. Its provider
// context is lazy and belongs to this owner, avoiding late-singleton teardown.
ModuleState g_State;

Record* FindRecord(ModuleData& data, const void* platform) {
    if (!platform) {
        return nullptr;
    }
    for (const auto& record : data.records) {
        if (record->platform.get() == platform) {
            return record.get();
        }
    }
    return nullptr;
}

Result ResultForState(State state) {
    switch (state) {
    case State::Ready:
        return Result::Ready;
    case State::PendingResources:
        return Result::PendingResources;
    case State::PendingRealization:
        return Result::PendingRealization;
    default:
        return Result::Unavailable;
    }
}

void Attempt(ModuleData& data, Record& record) {
    if (record.bitmap || record.state == State::Ready || !record.validRequest) {
        return;
    }
    if (!data.bound) {
        record.state = State::PendingResources;
        record.diagnostic = "font resources are not bound: " + record.request.requestedName;
        return;
    }
    if (record.attemptedGeneration == data.catalogGeneration && record.state == State::Unavailable) {
        return;
    }
    record.attemptedGeneration = data.catalogGeneration;
    record.nativeError = 0;
    record.diagnostic.clear();
    record.face = data.provider->Realize(data.context, record.request, data.files,
        record.nativeError, record.diagnostic);
    if (record.face) {
        record.metrics = record.face->Metrics();
        if (record.metrics.height <= 0 || record.metrics.maximumWidth <= 0) {
            record.nativeError = kInvalidMetrics;
            record.diagnostic = "provider supplied unusable font metrics";
        } else if ((record.metrics.capabilities & RequiredCapabilities(record.request))
            != RequiredCapabilities(record.request)) {
            record.nativeError = kMissingCapabilities;
            record.diagnostic = "provider does not preserve every requested legacy font feature";
        } else {
            record.state = State::Ready;
            return;
        }
        record.face.reset();
    }
    record.metrics = {};
    record.state = State::Unavailable;
    if (record.nativeError == 0) {
        record.nativeError = kMissingFace;
    }
    if (record.diagnostic.empty()) {
        record.diagnostic = "exact font identity unavailable: " + record.request.lookupIdentity;
    }
}

Record& Usable(ModuleData& data, vgui::BaseFontPlat* platform) {
    Record* record = FindRecord(data, platform);
    if (!record) {
        cse_vgui1::InvalidLegacyUse("unknown/null font platform");
    }
    Attempt(data, *record);
    if (record->state != State::Ready) {
        cse_vgui1::InvalidLegacyUse(record->diagnostic.c_str());
    }
    return *record;
}

unsigned int LegacyByte(int ch) {
    // Retained callers sometimes sign-extend a byte char. Normalize only that
    // explicit byte domain; arbitrary Unicode/int values are not truncated.
    constexpr int kMinimumSignedByte = -128;
    if (ch < kMinimumSignedByte || ch >= static_cast<int>(kLegacyByteCount)) {
        cse_vgui1::InvalidLegacyUse("glyph index is outside the legacy byte domain");
    }
    return static_cast<unsigned int>(ch < 0 ? ch + static_cast<int>(kLegacyByteCount) : ch);
}

class SystemFontPlat final : public vgui::BaseFontPlat {
public:
    explicit SystemFontPlat(Record& record) : record_(record) {}

    bool equals(const char* name, int tall, int wide, float rotation, int weight,
        bool italic, bool underline, bool strikeout, bool symbol) override {
        const auto flags = (italic ? cse_vgui1::FontItalic : 0U) | (underline ? cse_vgui1::FontUnderline : 0U)
            | (strikeout ? cse_vgui1::FontStrikeout : 0U) | (symbol ? cse_vgui1::FontSymbol : 0U);
        return EqualName(name, record_.request.requestedName) && tall == record_.request.tall
            && wide == record_.request.wide && rotation == record_.request.rotation
            && weight == record_.request.weight && flags == record_.request.flags;
    }

    void getCharRGBA(int ch, int x, int y, int wide, int tall, uchar* rgba) override {
        auto& data = g_State.data();
        std::lock_guard<std::recursive_mutex> guard(data.mutex);
        Record& record = Usable(data, this);
        if (x < 0 || y < 0 || wide < 0 || tall < 0 || !rgba
            || static_cast<std::uint64_t>(wide) * static_cast<std::uint64_t>(tall)
                > static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)()) / VGUI_RGBA_BYTES) {
            cse_vgui1::InvalidLegacyUse("invalid legacy raster canvas");
        }
        if (!record.face->Raster(LegacyByte(ch), x, y, wide, tall, rgba)) {
            cse_vgui1::InvalidLegacyUse("provider failed the requested legacy raster operation");
        }
    }

    void getCharABCwide(int ch, int& a, int& b, int& c) override {
        auto& data = g_State.data();
        std::lock_guard<std::recursive_mutex> guard(data.mutex);
        Record& record = Usable(data, this);
        if (!record.face->RawAbc(LegacyByte(ch), a, b, c) || b < 0) {
            cse_vgui1::InvalidLegacyUse("provider failed legacy ABC metrics");
        }
        if (a < 0) {
            a = 0;
        }
    }

    int getTall() override {
        auto& data = g_State.data();
        std::lock_guard<std::recursive_mutex> guard(data.mutex);
        return Usable(data, this).metrics.height;
    }

    int getWide() override {
        auto& data = g_State.data();
        std::lock_guard<std::recursive_mutex> guard(data.mutex);
        return Usable(data, this).metrics.maximumWidth;
    }

    void drawSetTextFont(vgui::SurfacePlat* surface) override {
        cse_vgui1::RequireReady(this);
        vgui::CseVgui1SelectFontForSurfaceV1(surface, this);
    }

private:
    Record& record_;
};

bool EnsureContext(ModuleData& data, std::string& diagnostic) {
    if (data.context) {
        return true;
    }
    const auto& provider = cse_vgui1::CseVgui1QualifiedFontProviderV1();
    if (!provider.CreateContext || !provider.DestroyContext || !provider.RegisterFile
        || !provider.ReleaseOwner || !provider.Realize) {
        diagnostic = "the qualified font provider binding is incomplete";
        return false;
    }
    data.context = provider.CreateContext();
    if (!data.context) {
        diagnostic = "the qualified font provider context is unavailable";
        return false;
    }
    data.provider = &provider;
    return true;
}

Result ReleaseFiles(ModuleData& data, std::string& diagnostic) {
    for (auto& record : data.records) {
        if (!record->bitmap) {
            record->face.reset();
            record->metrics = {};
            if (record->validRequest) {
                record->state = State::PendingResources;
                record->attemptedGeneration = 0;
                record->nativeError = 0;
                record->diagnostic = "font resources were released: " + record->request.requestedName;
            }
        }
    }
    data.bound = false;
    bool failed = false;
    for (auto iterator = data.batches.begin(); iterator != data.batches.end();) {
        const int error = data.provider->ReleaseOwner(data.context, iterator->get());
        if (error != 0) {
            failed = true;
            diagnostic = "owned font-file release failed; retain the module and retry";
            ++iterator;
        } else {
            iterator = data.batches.erase(iterator);
        }
    }
    data.releasePending = failed;
    if (failed) {
        return Result::ReleaseFailed;
    }
    data.files.clear();
    if (data.context) {
        data.provider->DestroyContext(data.context);
        data.context = nullptr;
        data.provider = nullptr;
    }
    diagnostic.clear();
    return Result::FilesReleased;
}
}

namespace cse_vgui1 {
char* OwnFontName(const char* name) {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    const char* text = name ? name : "";
    const std::size_t length = strnlen(text, kMaximumNameBytes + 1);
    if (length > kMaximumNameBytes) {
        text = "";
    }
    const std::size_t bytes = std::strlen(text) + 1;
    auto* copy = static_cast<char*>(std::malloc(bytes));
    if (!copy) {
        InvalidLegacyUse("unable to retain the font request name");
    }
    std::memcpy(copy, text, bytes);
    data.names.emplace_back(copy);
    return copy;
}

vgui::BaseFontPlat* FindCachedPlatform(const char* name, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol) {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    int checkedRotation = 0;
    if (!ValidUtf8(name, kMaximumNameBytes) || !RotationTenths(rotation, checkedRotation)) {
        return nullptr;
    }
    for (const auto& record : data.records) {
        if (!record->bitmap && record->validRequest && record->platform
            && record->lookupGeneration == data.lookupGeneration
            && record->platform->equals(name, tall, wide, rotation, weight, italic, underline, strikeout, symbol)) {
            return record->platform.get();
        }
    }
    return nullptr;
}

vgui::BaseFontPlat* CreatePlatform(const char* name, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol) {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    std::unique_ptr<Record> record(new Record);
    record->lookupGeneration = data.lookupGeneration;
    record->request.requestedName = ValidUtf8(name, kMaximumNameBytes) ? name : "";
    record->request.tall = tall;
    record->request.wide = wide;
    record->request.rotation = rotation;
    record->request.weight = weight;
    record->request.flags = (italic ? FontItalic : 0U) | (underline ? FontUnderline : 0U)
        | (strikeout ? FontStrikeout : 0U) | (symbol ? FontSymbol : 0U);
    record->validRequest = !record->request.requestedName.empty()
        && RotationTenths(rotation, record->request.rotationTenths);
    if (!record->validRequest) {
        record->state = State::Unavailable;
        record->nativeError = kInvalidRequest;
        record->diagnostic = "invalid UTF8 name or unrepresentable font rotation";
    }
    ApplyPinnedResolution(record->request);
    record->platform.reset(new SystemFontPlat(*record));
    vgui::BaseFontPlat* platform = record->platform.get();
    data.records.push_back(std::move(record));
    Attempt(data, *data.records.back());
    return platform;
}

void AdoptBitmapPlatform(vgui::BaseFontPlat* platform, const char* name) {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    std::unique_ptr<Record> record(new Record);
    record->platform.reset(platform);
    record->request.requestedName = name ? name : "";
    record->lookupGeneration = data.lookupGeneration;
    record->bitmap = true;
    record->state = State::Ready;
    record->metrics.capabilities = CapabilityAbc | CapabilityLegacyRgba | CapabilityPenOffsets;
    data.records.push_back(std::move(record));
}

void RecordBitmapFailure(const char* name) {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    std::unique_ptr<Record> record(new Record);
    record->request.requestedName = name ? name : "";
    record->lookupGeneration = data.lookupGeneration;
    record->bitmap = true;
    record->validRequest = false;
    record->state = State::Unavailable;
    record->nativeError = kInvalidBitmap;
    record->diagnostic = "invalid bitmap font: " + record->request.requestedName;
    data.records.push_back(std::move(record));
}

void RequireReady(vgui::BaseFontPlat* platform) {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    Usable(data, platform);
}

[[noreturn]] void InvalidLegacyUse(const char* message) {
    std::fprintf(stderr, "VGUI1 font legacy API used without a ready face: %s\n", message);
    std::abort();
}

int NextFontId() {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    if (data.nextId == INT_MAX) {
        InvalidLegacyUse("font ID domain exhausted");
    }
    return data.nextId++;
}

void ResetLookup() {
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    if (data.lookupGeneration == (std::numeric_limits<std::uint64_t>::max)()) {
        InvalidLegacyUse("font lookup generation exhausted");
    }
    ++data.lookupGeneration;
}
}

extern "C" CseVgui1FontResultV1 CseVgui1FontsBindFilesV1(const char* const* paths, std::uint32_t count,
    char* diagnostic, std::size_t capacity) noexcept {
    if ((!paths && count != 0) || !ValidOutput(diagnostic, capacity)) {
        return Result::InvalidArgument;
    }
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    std::string detail;
    if (data.releasePending) {
        WriteDiagnostic(diagnostic, capacity, "retry the failed font-file release before binding");
        return Result::ReleaseFailed;
    }
    std::unique_ptr<RegistrationBatch> batch(new RegistrationBatch);
    for (std::uint32_t index = 0; index < count; ++index) {
        constexpr std::size_t kMaximumPathBytes = INT_MAX;
        if (!paths[index] || paths[index][0] != '/' || !ValidUtf8(paths[index], kMaximumPathBytes)) {
            WriteDiagnostic(diagnostic, capacity, "font paths must be complete resolved UTF8 paths");
            return Result::InvalidArgument;
        }
        const std::string path(paths[index]);
        if (std::find(data.files.begin(), data.files.end(), path) == data.files.end()
            && std::find(batch->paths.begin(), batch->paths.end(), path) == batch->paths.end()) {
            batch->paths.push_back(path);
        }
    }
    if (data.bound && batch->paths.empty()) {
        WriteDiagnostic(diagnostic, capacity, "files already bound; font readiness still requires preflight");
        return Result::FilesBound;
    }
    if (data.catalogGeneration == (std::numeric_limits<std::uint64_t>::max)()) {
        WriteDiagnostic(diagnostic, capacity, "font catalog generation exhausted");
        return Result::InvalidArgument;
    }
    if (!EnsureContext(data, detail)) {
        WriteDiagnostic(diagnostic, capacity, detail);
        return Result::RegistrationFailed;
    }
    std::size_t acquired = 0;
    for (const auto& path : batch->paths) {
        const int error = data.provider->RegisterFile(data.context, batch.get(), path.c_str());
        if (error != 0) {
            if (acquired != 0 && data.provider->ReleaseOwner(data.context, batch.get()) != 0) {
                data.batches.push_back(std::move(batch));
                data.releasePending = true;
            }
            WriteDiagnostic(diagnostic, capacity, "font registration failed; new catalog was not published");
            return Result::RegistrationFailed;
        }
        ++acquired;
    }
    if (!batch->paths.empty()) {
        data.files.insert(data.files.end(), batch->paths.begin(), batch->paths.end());
        data.batches.push_back(std::move(batch));
    }
    data.bound = true;
    ++data.catalogGeneration;
    for (auto& record : data.records) {
        if (!record->bitmap && record->validRequest && record->state != State::Ready) {
            record->state = State::PendingRealization;
            record->diagnostic = "font request awaits catalog preflight: " + record->request.requestedName;
            record->nativeError = 0;
        }
    }
    WriteDiagnostic(diagnostic, capacity, "files bound; font readiness still requires preflight");
    return Result::FilesBound;
}

extern "C" CseVgui1FontResultV1 CseVgui1FontsPreflightV1(char* diagnostic, std::size_t capacity) noexcept {
    if (!ValidOutput(diagnostic, capacity)) {
        return Result::InvalidArgument;
    }
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    for (auto& record : data.records) {
        if (record->lookupGeneration != data.lookupGeneration) {
            continue;
        }
        Attempt(data, *record);
        if (record->state != State::Ready) {
            WriteDiagnostic(diagnostic, capacity, record->diagnostic);
            return ResultForState(record->state);
        }
    }
    WriteDiagnostic(diagnostic, capacity, "all current font requests are ready");
    return Result::Ready;
}

extern "C" CseVgui1FontResultV1 CseVgui1FontQueryV1(const void* platform, CseVgui1FontStatusV1* status,
    char* diagnostic, std::size_t capacity) noexcept {
    if (!status || status->structSize != sizeof(*status) || !ValidOutput(diagnostic, capacity)) {
        return Result::InvalidArgument;
    }
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    Record* record = FindRecord(data, platform);
    if (!record) {
        WriteDiagnostic(diagnostic, capacity, "font platform does not belong to this module");
        return Result::InvalidHandle;
    }
    status->state = record->state;
    status->catalogGeneration = data.catalogGeneration;
    status->nativeError = record->nativeError;
    status->capabilities = record->metrics.capabilities;
    WriteDiagnostic(diagnostic, capacity, record->diagnostic);
    return ResultForState(record->state);
}

extern "C" CseVgui1FontResultV1 CseVgui1FontPrepareV1(const void* platform, CseVgui1FontStatusV1* status,
    char* diagnostic, std::size_t capacity) noexcept {
    if (!status || status->structSize != sizeof(*status) || !ValidOutput(diagnostic, capacity)) {
        return Result::InvalidArgument;
    }
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    Record* record = FindRecord(data, platform);
    if (!record) {
        WriteDiagnostic(diagnostic, capacity, "font platform does not belong to this module");
        return Result::InvalidHandle;
    }
    if (!data.releasePending) {
        Attempt(data, *record);
    }
    status->state = record->state;
    status->catalogGeneration = data.catalogGeneration;
    status->nativeError = record->nativeError;
    status->capabilities = record->metrics.capabilities;
    if (data.releasePending) {
        WriteDiagnostic(diagnostic, capacity, "retry failed font-file release before preparing");
        return Result::ReleaseFailed;
    }
    WriteDiagnostic(diagnostic, capacity, record->diagnostic);
    return ResultForState(record->state);
}

extern "C" CseVgui1FontResultV1 CseVgui1FontsReleaseFilesV1(char* diagnostic, std::size_t capacity) noexcept {
    if (!ValidOutput(diagnostic, capacity)) {
        return Result::InvalidArgument;
    }
    auto& data = g_State.data();
    std::lock_guard<std::recursive_mutex> guard(data.mutex);
    std::string detail;
    const Result result = ReleaseFiles(data, detail);
    WriteDiagnostic(diagnostic, capacity, detail);
    return result;
}
