#include "VGui1FixedSysFontFace.h"
#include "VGui1MacFontProvider.h"
#include "digest.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

namespace {
using namespace cse_vgui1;
using namespace cse_vgui1::legacy;
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED FixedSys role: %s\n", message);
        std::abort();
    }
}
std::vector<unsigned char> Read(const std::string& path) {
    FILE* input = nullptr;
#if defined(_MSC_VER)
    Require(fopen_s(&input, path.c_str(), "rb") == 0, "authored input open");
#else
    input = std::fopen(path.c_str(), "rb");
#endif
    Require(input != nullptr, "authored input exists");
    std::array<unsigned char, 32768> bytes{};
    const auto count = std::fread(bytes.data(), 1, bytes.size(), input);
    Require(count > 0 && count < bytes.size() && std::fclose(input) == 0, "bounded authored font read");
    return {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(count)};
}
FontRequest Request() {
    FontRequest request;
    request.requestedName = "FixedSys";
    request.lookupIdentity = "Courier";
    request.resolutionProfile = FixedSysRoleProfileV1;
    request.tall = 18;
    request.weight = 400;
    return request;
}
std::unique_ptr<FixedSysFontFace> Make(const std::vector<unsigned char>& bytes, const FontRequest& request,
    bool success, Result* returned = nullptr) {
    Result result = Result::Ok;
    int error = 0;
    std::string diagnostic;
    auto face = FixedSysFontFace::CreateOwned(bytes.data(), bytes.size(), 0, "Courier", request,
        result, error, diagnostic);
    if ((face != nullptr) != success) {
        std::fprintf(stderr, "result=%d FT=%d diagnostic=%s\n", static_cast<int>(result), error, diagnostic.c_str());
    }
    Require((face != nullptr) == success, "creation face/status");
    Require(success ? result == Result::Ok && error == 0 && diagnostic.empty()
        : result != Result::Ok && !diagnostic.empty(), "ordinary success/failure diagnostics");
    if (returned) {
        *returned = result;
    }
    return face;
}
constexpr char kReferenceRoutes[] =
    "0101111110012011111111111111000003333333333333333333333333333333"
    "3333333333333333333333333333333333333333333333333333333333333332"
    "3033333333333030033333333333303303333333333333333333333333333333"
    "3333333333333333333333333333333333333333333333333333333333333333"
;

struct Oracle {
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    explicit Oracle(const std::vector<unsigned char>& bytes) {
        Require(FT_Init_FreeType(&library) == 0, "independent owned FT oracle library");
        FT_UInt interpreter = 40;
        Require(FT_Property_Set(library, "truetype", "interpreter-version", &interpreter) == 0,
            "same pinned interpreter in independent oracle");
        Require(FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face) == 0
            && FT_Select_Charmap(face, FT_ENCODING_UNICODE) == 0, "independent authored-memory face");
    }
    ~Oracle() {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }
    unsigned int Glyph(unsigned int byte) const {
        return kReferenceRoutes[byte] == '1' ? 0 : FT_Get_Char_Index(face,
            LegacyByteToUnicode(static_cast<unsigned char>(byte)));
    }
    void Load(unsigned int byte) {
        Require(FT_Load_Glyph(face, Glyph(byte), FT_LOAD_TARGET_NORMAL) == 0
            && FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) == 0, "independent gray glyph render");
    }
    bool Fits(int ppem, int& left) {
        Require(FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(ppem)) == 0, "oracle size");
        left = 0;
        int right = (640 * ppem + 1023) / 1024; // Authored ASCII advance/units, not a donor-family assumption.
        bool valid = true;
        for (unsigned int byte = 0; byte < 256; ++byte) {
            if (kReferenceRoutes[byte] != '1' && kReferenceRoutes[byte] != '3') {
                continue;
            }
            Load(byte);
            const auto* slot = face->glyph;
            left = (std::min)(left, slot->bitmap_left);
            right = (std::max)(right, slot->bitmap_left + static_cast<int>(slot->bitmap.width));
            valid = valid && slot->bitmap_top <= 12
                && static_cast<int>(slot->bitmap.rows) - slot->bitmap_top <= 3;
            bool nonempty = false;
            for (unsigned int y = 0; y < slot->bitmap.rows; ++y) {
                for (unsigned int x = 0; x < slot->bitmap.width; ++x) {
                    nonempty = nonempty || slot->bitmap.buffer[static_cast<std::size_t>(y)
                        * slot->bitmap.pitch + x] != 0;
                }
            }
            valid = valid && nonempty;
        }
        return valid && right - left <= 8;
    }
};

void OracleChecks(FixedSysFontFace& subject, const std::vector<unsigned char>& bytes) {
    Oracle oracle(bytes);
    const auto info = subject.Info();
    int left = 0;
    Require(oracle.Fits(info.fitPpem, left) && info.originX == -left,
        "chosen global transform equals independent complete ink bounds");
    for (int size = info.fitPpem + 1; size <= 12; ++size) {
        Require(!oracle.Fits(size, left), "every larger uniform scale is rejected by the actual bitmap bounds");
    }
    Require(FT_Set_Pixel_Sizes(oracle.face, 0, static_cast<FT_UInt>(info.fitPpem)) == 0, "oracle final scale");
    for (unsigned int byte = 0; byte < 256; ++byte) {
        if (kReferenceRoutes[byte] != '1' && kReferenceRoutes[byte] != '3') {
            continue;
        }
        Require(subject.GlyphIndex(byte) == oracle.Glyph(byte), "actual complete native mapping/notdef identity");
        oracle.Load(byte);
        std::array<unsigned char, 8 * 15 * 4> bgr{};
        const auto* slot = oracle.face->glyph;
        for (unsigned int y = 0; y < slot->bitmap.rows; ++y) {
            for (unsigned int x = 0; x < slot->bitmap.width; ++x) {
                const auto dx = info.originX + slot->bitmap_left + static_cast<int>(x);
                const auto dy = 12 - slot->bitmap_top + static_cast<int>(y);
                Require(dx >= 0 && dx < 8 && dy >= 0 && dy < 15, "oracle source ink is not clipped");
                const auto offset = static_cast<std::size_t>(dy * 8 + dx) * 4;
                const auto value = slot->bitmap.buffer[static_cast<std::size_t>(y) * slot->bitmap.pitch + x];
                bgr[offset] = value;
                bgr[offset + 1] = value;
                bgr[offset + 2] = value;
            }
        }
        std::array<unsigned char, 8 * 15 * 4> expected{};
        std::array<unsigned char, 8 * 15 * 4> actual{};
        Require(TransferLegacyBgr({bgr.data(), bgr.size(), 8, 15, 32}, 8, 15, 0, 0,
            {expected.data(), expected.size(), 8, 15}, false) == Result::Ok
            && subject.Raster(byte, 0, 0, 8, 15, actual.data()) && expected == actual,
            "cached raster equals independent gray image plus only the shared transfer kernel");
    }
    std::printf("owned-role-fit ppem=%d origin=%d bounds=%d/%d/%d/%d\n", info.fitPpem, info.originX,
        info.minimumBitmapLeft, info.maximumBitmapRight, info.maximumBitmapTop, info.maximumBitmapBottom);
}

void FaceContract(const std::vector<unsigned char>& bytes) {
    auto owned = bytes;
    auto face = Make(owned, Request(), true);
    owned.assign(owned.size(), 0);
    const auto metrics = face->Metrics();
    const auto info = face->Info();
    Require(metrics.height == 15 && metrics.maximumWidth == 8 && metrics.capabilities == 39,
        "exact logical metrics and narrow capabilities");
    Require(info.ascent == 12 && info.descent == 3 && info.internalLeading == 3 && info.roleWeight == 400
        && !info.numericWeightKnown && info.numericWeight == 0 && info.fitPpem > 0 && info.fitPpem <= 12,
        "baseline/global fit/unknown numeric weight");
    std::array<unsigned int, 4> routes{};
    for (unsigned int byte = 0; byte < 256; ++byte) {
        FixedSysByteRoute route = FixedSysByteRoute::ForcedBlank;
        Require(face->Route(byte, route), "all256 routing");
        Require(static_cast<unsigned int>(route) == static_cast<unsigned int>(kReferenceRoutes[byte] - '0'),
            "every byte matches the separately pinned GDI role-routing data");
        Require(static_cast<std::size_t>(route) < routes.size(), "known route enum domain");
        ++routes[static_cast<std::size_t>(route)];
        int a = -91;
        int b = -92;
        int c = -93;
        Require(face->RawAbc(byte, a, b, c) && a == 0 && b == 8 && c == 0, "all256 fixed ABC");
        std::array<unsigned char, 24 * 24 * 4> canvas;
        canvas.fill(0x5a);
        Require(face->Raster(byte, 3, 5, 24, 24, canvas.data()), "nonzero pen raster");
        unsigned int alpha = 0;
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < 24; ++x) {
                const auto offset = static_cast<std::size_t>((y * 24 + x) * 4);
                const bool inside = x >= 3 && x < 11 && y >= 5 && y < 20;
                if (!inside) {
                    Require(canvas[offset] == 0x5a && canvas[offset + 3] == 0x5a, "destination guard bytes");
                } else {
                    alpha += canvas[offset + 3] != 0;
                }
            }
        }
        if (route == FixedSysByteRoute::ForcedBlank) {
            Require(alpha == 0, "blank/space/tab class stays blank");
        } else {
            Require(alpha > 0, "no invisible graphic/notdef/default");
        }
        if (route == FixedSysByteRoute::AuthoredDefaultBlock) {
            Digest digest;
            digest.Update(canvas.data(), canvas.size());
            Require(digest.Finish() == "cfa79e1ed0df70dd52de2bf2030731b125ea07424b53ec0a44f9a8ec296ce0a7",
                "exact authored default rectangle reference digest");
        }
    }
    Require(routes == std::array<unsigned int, 4>{{16, 22, 2, 216}}, "16/22/2/216 route counts");
    Require(face->GlyphIndex(8) == 0 && face->GlyphIndex(0x80) != 0 && face->GlyphIndex(0x95) != 0,
        "08 override and valid C1 mappings are retained");
    std::array<unsigned char, 8 * 15 * 4> first{};
    std::array<unsigned char, 8 * 15 * 4> again{};
    Require(face->Raster('A', 0, 0, 8, 15, first.data()) && face->Raster(0xc, 0, 0, 8, 15, again.data())
        && face->Raster('A', 0, 0, 8, 15, again.data()) && first == again, "immutable cached cells/call order");
    again.fill(0x5a);
    const auto saved = again;
    Require(!face->Raster(256, 0, 0, 8, 15, again.data()) && !face->Raster('A', -1, 0, 8, 15, again.data())
        && !face->Raster('A', 0, 0, INT_MAX, INT_MAX, again.data()) && again == saved,
        "invalid geometry/byte rejects before output writes");
    Require(!face->Raster('A', 0, 0, 8, 15, nullptr)
        && !face->Raster('A', 0, 0, -1, 15, again.data()), "null/negative canvas fails before writing");
    FixedSysByteRoute poisoned = FixedSysByteRoute::MappedGraphic;
    Require(!face->Route(256, poisoned) && poisoned == FixedSysByteRoute::MappedGraphic,
        "invalid route output is unchanged");
    OracleChecks(*face, bytes);
    int a = 91;
    int b = 92;
    int c = 93;
    Require(!face->RawAbc(256, a, b, c) && a == 91 && b == 92 && c == 93, "invalid ABC atomicity");
}
struct ServiceState {
    std::vector<unsigned char> bytes;
    int lookups = 0;
    int reads = 0;
    int releases = 0;
    bool failRelease = false;
    std::map<const void*, std::set<std::string>> owners;
};
bool LookupRole(void* data, FT_LibraryRec_*, const FontRequest& request, NativeFontLocation& location) noexcept {
    auto& state = *static_cast<ServiceState*>(data);
    ++state.lookups;
    Require(request.lookupIdentity == "Courier", "explicit owner lookup reaches service unchanged");
    location = {"/owned/role.ttf", "Courier", 0};
    return true;
}
bool ReadRole(void* data, const char* path, std::vector<unsigned char>& bytes) noexcept {
    auto& state = *static_cast<ServiceState*>(data);
    ++state.reads;
    Require(std::string(path) == "/owned/role.ttf", "owned role path");
    bytes = state.bytes;
    return true;
}
int AcquireRole(void* data, const void* owner, const char* path) noexcept {
    static_cast<ServiceState*>(data)->owners[owner].insert(path);
    return 1;
}
int ReleaseRole(void* data, const void* owner) noexcept {
    auto& state = *static_cast<ServiceState*>(data);
    ++state.releases;
    if (state.failRelease) {
        return 0;
    }
    state.owners.erase(owner);
    return 1;
}
void ProviderContract(const std::vector<unsigned char>& bytes) {
    ServiceState state;
    state.bytes = bytes;
    const MacFontServicesV1 callbacks{&state, LookupRole, ReadRole, AcquireRole, ReleaseRole};
    const auto& provider = CseVgui1MacFontProviderCandidateV1();
    void* context = CreateMacFontContextV1(callbacks);
    Require(context != nullptr, "injected owned context");
    int owner = 0;
    Require(provider.RegisterFile(context, &owner, "/owned/role.ttf") == 0, "injected owned registration");
    auto request = Request();
    const auto original = request;
    int error = 777;
    std::string diagnostic;
    auto face = provider.Realize(context, request, {}, error, diagnostic);
    Require(face && dynamic_cast<FixedSysFontFace*>(face.get()) && error == 0 && diagnostic.empty(),
        "tagged request dispatches actual new face only");
    Require(request.requestedName == original.requestedName && request.lookupIdentity == original.lookupIdentity
        && request.tall == original.tall && request.weight == original.weight,
        "provider never changes requested name/cache identity or original parameters");
    const int lookups = state.lookups;
    auto invalid = request;
    invalid.flags = FontItalic;
    Require(!provider.Realize(context, invalid, {}, error, diagnostic) && state.lookups == lookups,
        "invalid reserved profile arguments reject before native service lookup/read");
    auto ordinary = request;
    ordinary.requestedName = "Courier";
    ordinary.resolutionProfile = "exact-request-v1";
    Require(!provider.Realize(context, ordinary, {}, error, diagnostic) && error != 0,
        "OS2-less ordinary Courier remains unqualified; no hidden alias");
    state.failRelease = true;
    face.reset();
    Require(provider.ReleaseOwner(context, &owner) != 0 && !state.owners.empty(),
        "release failure retains actual provider tokens/context after role face closes");
    state.failRelease = false;
    Require(provider.ReleaseOwner(context, &owner) == 0 && state.owners.empty(), "explicit release retry");
    provider.DestroyContext(context);
}

void Failures(const std::string& directory, const std::vector<unsigned char>& bytes) {
    for (const char* name : {"missing-graphic.ttf", "empty-graphic.ttf", "empty-notdef.ttf", "italic.ttf",
        "bold.ttf", "linegap.ttf", "short-hmtx.ttf", "with-os2.ttf", "unfit.ttf",
        "nonuniform-ascii.ttf", "post-italic.ttf"}) {
        Make(Read(directory + "/" + name), Request(), false);
    }
    Result result = Result::Ok;
    int error = 777;
    std::string diagnostic;
    Require(!FixedSysFontFace::CreateOwned(bytes.data(), (std::numeric_limits<std::size_t>::max)(), 0,
        "Courier", Request(), result, error, diagnostic) && result == Result::Unrepresentable && error == 0,
        "unrepresentable span rejects before copying/dereferencing");
    Require(!FixedSysFontFace::CreateOwned(nullptr, 0, 0, "Courier", Request(), result, error, diagnostic)
        && result == Result::InvalidArgument, "null span rejects ordinarily");
    Require(!FixedSysFontFace::CreateOwned(bytes.data(), bytes.size(), 1, "Courier", Request(),
        result, error, diagnostic) && result == Result::InvalidArgument, "index never truncates to zero");
    Require(!FixedSysFontFace::CreateOwned(bytes.data(), bytes.size(), 0, "UnrelatedPS", Request(),
        result, error, diagnostic) && result == Result::InvalidArgument, "exact selected identity required");
    for (int change = 0; change < 8; ++change) {
        auto request = Request();
        if (change == 0) {
            request.tall = 19;
        }
        if (change == 1) {
            request.wide = 1;
        }
        if (change == 2) {
            request.rotation = 0.01f;
        }
        if (change == 3) {
            request.weight = 700;
        }
        if (change == 4) {
            request.flags = FontItalic;
        }
        if (change == 5) {
            request.ansiCodePage = 65001;
        }
        if (change == 6) {
            request.requestedName = "Courier";
        }
        if (change == 7) {
            request.resolutionProfile = "exact-request-v1";
        }
        Make(bytes, request, false);
    }
}
}
int main(int argc, char** argv) {
    Require(argc == 2, "authored input directory");
    const std::string directory = argv[1];
    const auto bytes = Read(directory + "/role.ttf");
    FaceContract(bytes);
    Failures(directory, bytes);
    ProviderContract(bytes);
    for (int cycle = 0; cycle < 16; ++cycle) {
        Make(bytes, Request(), true);
    }
    std::puts("PASS authored FixedSys role; native appearance/UI/readability acceptance remains open");
}
