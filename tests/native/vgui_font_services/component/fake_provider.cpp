#include "fake_provider.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

namespace {
constexpr std::size_t kModelBytes = 128;
constexpr int kMagic = 0x43534546;
constexpr int kRegistrationError = 2101;
constexpr int kReleaseError = 2102;
constexpr int kLookupError = 2103;
FakeFontStats g_Stats{};
int g_FailRegistration = 0;
int g_ReleaseFailures = 0;
bool g_FailCreate = false;
bool g_InvalidMetrics = false;
std::uint32_t g_DroppedCapabilities = 0;
unsigned int g_LastByte = 0;
int g_LastReleaseLiveFaces = 0;
struct Context {
    int magic = kMagic;
    int liveFaces = 0;
    std::map<const void*, std::set<std::string>> owners;
    cse_vgui1::FontRequest lastRequest;
};
Context* g_CurrentContext = nullptr;

void Require(bool condition) {
    if (!condition) {
        std::fputs("FAILED fixture provider ownership/lifetime\n", stderr);
        std::abort();
    }
}

class Face final : public cse_vgui1::RealizedFace {
public:
    explicit Face(Context& context) : context_(context) {
        ++context_.liveFaces;
        ++g_Stats.liveFaces;
        ++g_Stats.facesCreated;
    }
    ~Face() override {
        Require(context_.magic == kMagic);
        --context_.liveFaces;
        --g_Stats.liveFaces;
        ++g_Stats.facesDestroyed;
    }
    cse_vgui1::FontMetrics Metrics() const noexcept override {
        using namespace cse_vgui1;
        constexpr std::uint32_t kAllCapabilities = CapabilityAbc | CapabilityLegacyRgba | CapabilityPenOffsets
            | CapabilityWidth | CapabilityRotation | CapabilityWeight | CapabilityItalic | CapabilityUnderline
            | CapabilityStrikeout | CapabilitySymbol;
        return {g_InvalidMetrics ? 0 : 5, 6, kAllCapabilities & ~g_DroppedCapabilities};
    }
    bool RawAbc(unsigned int byte, int& a, int& b, int& c) noexcept override {
        Require(context_.magic == kMagic);
        g_LastByte = byte;
        a = -2;
        b = 4;
        c = 1;
        return true;
    }
    bool Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept override {
        Require(context_.magic == kMagic);
        g_LastByte = byte;
        constexpr int kWidth = 4;
        constexpr int kHeight = 5;
        constexpr std::size_t kChannels = 4;
        for (int row = 0; row < kHeight; ++row) {
            for (int col = 0; col < kWidth; ++col) {
                if (x + col >= wide || y + row >= tall) {
                    continue;
                }
                auto* pixel = rgba + (static_cast<std::size_t>(y + row) * static_cast<std::size_t>(wide)
                    + static_cast<std::size_t>(x + col)) * kChannels;
                const unsigned char values[]{17, 91, 203, 78};
                std::memcpy(pixel, values, sizeof(values));
                if (byte == '\t') {
                    std::memset(pixel, 0, kChannels);
                }
            }
        }
        return true;
    }

private:
    Context& context_;
};

void* CreateContext() noexcept {
    if (g_FailCreate) {
        return nullptr;
    }
    auto* context = new Context;
    g_CurrentContext = context;
    ++g_Stats.contextsCreated;
    return context;
}

void DestroyContext(void* value) noexcept {
    auto* context = static_cast<Context*>(value);
    Require(context->magic == kMagic && context->liveFaces == 0 && context->owners.empty());
    context->magic = 0;
    delete context;
    g_CurrentContext = nullptr;
    ++g_Stats.contextsDestroyed;
    if (const char* receipt = std::getenv("CSE_VGUI1_FONT_TEARDOWN_RECEIPT")) {
        if (FILE* output = std::fopen(receipt, "ab")) {
            std::fprintf(output, "context-destroyed faces=%d created=%d destroyed=%d registrations=%d releases=%d\n",
                g_Stats.liveFaces, g_Stats.facesCreated, g_Stats.facesDestroyed, g_Stats.registrations, g_Stats.releases);
            std::fclose(output);
        }
    }
}

int RegisterFile(void* value, const void* owner, const char* path) noexcept {
    auto* context = static_cast<Context*>(value);
    Require(context->magic == kMagic);
    ++g_Stats.registrations;
    if (g_FailRegistration != 0 && g_Stats.registrations == g_FailRegistration) {
        return kRegistrationError;
    }
    context->owners[owner].insert(path);
    return 0;
}

int ReleaseOwner(void* value, const void* owner) noexcept {
    auto* context = static_cast<Context*>(value);
    Require(context->magic == kMagic);
    g_LastReleaseLiveFaces = context->liveFaces;
    ++g_Stats.releases;
    if (g_ReleaseFailures > 0) {
        --g_ReleaseFailures;
        return kReleaseError;
    }
    context->owners.erase(owner);
    return 0;
}

std::unique_ptr<cse_vgui1::RealizedFace> Realize(void* value, const cse_vgui1::FontRequest& request,
    const std::vector<std::string>& files, int& error, std::string& diagnostic) noexcept {
    auto* context = static_cast<Context*>(value);
    Require(context->magic == kMagic);
    context->lastRequest = request;
    ++g_Stats.realizations;
    for (const auto& path : files) {
        FILE* input = std::fopen(path.c_str(), "rb");
        if (!input) {
            continue;
        }
        char model[kModelBytes]{};
        const std::size_t count = std::fread(model, 1, sizeof(model) - 1, input);
        Require(std::fclose(input) == 0);
        model[count] = 0;
        if (request.lookupIdentity == model) {
            return std::unique_ptr<cse_vgui1::RealizedFace>(new Face(*context));
        }
    }
    error = kLookupError;
    diagnostic = "owned model unavailable for exact identity " + request.lookupIdentity;
    return {};
}
}

namespace cse_vgui1 {
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    static const FontProviderV1 provider{CreateContext, DestroyContext, RegisterFile, ReleaseOwner, Realize};
    return provider;
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat* surface, BaseFontPlat* font) {
    Require(surface && font);
    surface->selected = font;
}
}

FakeFontStats FakeFontSnapshot() {
    return g_Stats;
}
void FakeFontFailRegister(int call) {
    g_FailRegistration = call;
}
void FakeFontFailRelease(int count) {
    g_ReleaseFailures = count;
}
void FakeFontDropCapabilities(std::uint32_t capabilities) {
    g_DroppedCapabilities = capabilities;
}
void FakeFontFailCreate(bool fail) {
    g_FailCreate = fail;
}
void FakeFontInvalidMetrics(bool invalid) {
    g_InvalidMetrics = invalid;
}
cse_vgui1::FontRequest FakeFontLastRequest() {
    Require(g_CurrentContext != nullptr);
    return g_CurrentContext->lastRequest;
}
unsigned int FakeFontLastByte() {
    return g_LastByte;
}

int FakeFontLastReleaseLiveFaces() {
    return g_LastReleaseLiveFaces;
}

extern "C" FakeFontStats FakeFontSnapshotC() {
    return FakeFontSnapshot();
}

extern "C" void FakeFontFailReleaseC(int count) {
    FakeFontFailRelease(count);
}
