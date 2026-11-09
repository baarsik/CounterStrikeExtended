#include "VGUI_Font.h"
#include "VGui1FontServices.h"
#include "VGui1FixedSysFontFace.h"
#include "VGui1MacFontProvider.h"
#include "digest.h"
#include "MacFontDiscovery.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <png.h>

namespace {
using namespace cse_vgui1;
using namespace cse_vgui1::legacy;
using OwnerResult = CseVgui1FontResultV1;
constexpr int kWidth = 8;
constexpr int kHeight = 15;
constexpr int kBaseline = 12;
constexpr int kScratchSide = 24;
constexpr int kPenX = 3;
constexpr int kPenY = 5;
constexpr int kByteMaximum = 255;
constexpr std::size_t kPixelBytes = 4;
constexpr unsigned int kByteCount = 256;
constexpr char kCourierPath[] = "/System/Library/Fonts/Courier.ttc";
constexpr char kCourierHash[] = "76cdad2ba4e8774223ed6184f530570261f9f7fb9beb5c68368e09234c0273ec";
constexpr char kMarlettHash[] = "3b6192eca4451835f4d634b0b6b0841da321ec65c1f3f1aa22d186da45787b9c";

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED owner readability: %s\n", message);
        std::fflush(nullptr);
        std::abort();
    }
}

std::string FileHash(const char* path) {
    FILE* input = std::fopen(path, "rb");
    Require(input != nullptr, "exact pinned resource input");
    Digest digest;
    std::array<unsigned char, 8192> buffer{};
    for (;;) {
        const auto count = std::fread(buffer.data(), 1, buffer.size(), input);
        digest.Update(buffer.data(), count);
        if (count < buffer.size()) {
            Require(!std::ferror(input) && std::fclose(input) == 0, "resource digest read/close");
            return digest.Finish();
        }
    }
}

struct Trace {
    std::string resource;
    FILE* journal = nullptr;
    void* context = nullptr;
    const void* token = nullptr;
    int creates = 0;
    int destroys = 0;
    int registrations = 0;
    int releases = 0;
    std::vector<FontRequest> requests;
    FixedSysRoleInfo info;
    std::array<FixedSysByteRoute, kByteCount> routes{};
    std::array<unsigned int, kByteCount> glyphs{};
};
Trace* g_Trace = nullptr;

void* Create() noexcept {
    Require(g_Trace && !g_Trace->context, "one actual context owned at a time");
    g_Trace->context = CseVgui1MacFontProviderCandidateV1().CreateContext();
    ++g_Trace->creates;
    return g_Trace->context;
}

void Destroy(void* context) noexcept {
    Require(g_Trace && context == g_Trace->context && !g_Trace->token, "destroy only after successful release");
    CseVgui1MacFontProviderCandidateV1().DestroyContext(context);
    ++g_Trace->destroys;
    g_Trace->context = nullptr;
}

int Register(void* context, const void* token, const char* path) noexcept {
    Require(g_Trace && context == g_Trace->context && token && !g_Trace->token
        && path && path == g_Trace->resource, "only one exact game resource token");
    const int result = CseVgui1MacFontProviderCandidateV1().RegisterFile(context, token, path);
    ++g_Trace->registrations;
    if (result == 0) {
        g_Trace->token = token;
    }
    std::fprintf(g_Trace->journal, "register\t%d\tresult=%d\n", g_Trace->registrations, result);
    return result;
}

int Release(void* context, const void* token) noexcept {
    Require(g_Trace && context == g_Trace->context && token == g_Trace->token, "release only actual owned token");
    const int result = CseVgui1MacFontProviderCandidateV1().ReleaseOwner(context, token);
    ++g_Trace->releases;
    if (result == 0) {
        g_Trace->token = nullptr;
    }
    std::fprintf(g_Trace->journal, "release\t%d\tresult=%d\n", g_Trace->releases, result);
    return result;
}

std::unique_ptr<RealizedFace> Realize(void* context, const FontRequest& request,
    const std::vector<std::string>& files, int& error, std::string& diagnostic) noexcept {
    // Check BEFORE forwarding: this fixture cannot query an accidental family.
    Require(g_Trace && context == g_Trace->context && request.requestedName == "FixedSys"
        && FixedSysFontFace::Matches(request) && files == std::vector<std::string>{g_Trace->resource},
        "actual owner preserves requested name and exact role before any native lookup");
    g_Trace->requests.push_back(request);
    FT_Library identityLibrary = nullptr;
    Require(FT_Init_FreeType(&identityLibrary) == 0, "bounded same-family identity library");
    cse::fonts::FontFile input;
    const bool found = cse::fonts::FindMacFontFile(identityLibrary, "Courier", FixedSysRoleWeightV1, false, input);
    FT_Done_FreeType(identityLibrary);
    const bool identity = found && input.path == kCourierPath && input.postScriptName == "Courier"
        && input.faceIndex == 0 && FileHash(kCourierPath) == kCourierHash;
    std::fprintf(g_Trace->journal, "native-input\tpath=%s\tps=%s\tindex=%ld\tidentity=%d\thash=%s\n",
        input.path.c_str(), input.postScriptName.c_str(), input.faceIndex, identity, kCourierHash);
    if (!identity) {
        error = -3101;
        diagnostic = "same-family locator does not match the pinned Courier/index0 program";
        return {};
    }
    auto face = CseVgui1MacFontProviderCandidateV1().Realize(context, request, files, error, diagnostic);
    auto* role = dynamic_cast<FixedSysFontFace*>(face.get());
    if (role) {
        g_Trace->info = role->Info();
        for (unsigned int byte = 0; byte < kByteCount; ++byte) {
            Require(role->Route(byte, g_Trace->routes[byte]), "ready role route snapshot");
            g_Trace->glyphs[byte] = role->GlyphIndex(byte);
        }
    }
    std::fprintf(g_Trace->journal, "realize\t%zu\trequested=%s\tlookup=%s\tprofile=%s\tready=%d\terror=%d\t%s\n",
        g_Trace->requests.size(), request.requestedName.c_str(), request.lookupIdentity.c_str(),
        request.resolutionProfile.c_str(), face != nullptr, error, diagnostic.c_str());
    return face;
}

CseVgui1FontStatusV1 Inspect(vgui::Font& font, bool prepare, OwnerResult expected, const char* label) {
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    std::array<char, 256> diagnostic{};
    const auto result = prepare
        ? CseVgui1FontPrepareV1(font.getPlat(), &status, diagnostic.data(), diagnostic.size())
        : CseVgui1FontQueryV1(font.getPlat(), &status, diagnostic.data(), diagnostic.size());
    std::fprintf(g_Trace->journal, "%s\tresult=%d\tstate=%u\tcatalog=%llu\tcap=%u\terror=%d\t%s\n",
        label, static_cast<int>(result), static_cast<unsigned int>(status.state),
        static_cast<unsigned long long>(status.catalogGeneration), status.capabilities,
        status.nativeError, diagnostic.data());
    Require(result == expected, "ordinary checked owner state");
    return status;
}

void Bind() {
    const char* paths[]{g_Trace->resource.c_str()};
    std::array<char, 256> diagnostic{};
    const auto result = CseVgui1FontsBindFilesV1(paths, 1, diagnostic.data(), diagnostic.size());
    std::fprintf(g_Trace->journal, "bind\tresult=%d\t%s\n", static_cast<int>(result), diagnostic.data());
    Require(result == OwnerResult::FilesBound, "actual scoped resource registration succeeds");
}

void Unbind() {
    std::array<char, 256> diagnostic{};
    const auto result = CseVgui1FontsReleaseFilesV1(diagnostic.data(), diagnostic.size());
    std::fprintf(g_Trace->journal, "unbind\tresult=%d\t%s\n", static_cast<int>(result), diagnostic.data());
    std::fflush(g_Trace->journal);
    if (result != OwnerResult::FilesReleased) {
        // Do not force-unload, destroy retained context, inject a failure or
        // retry implicitly through global destructors. The OS reclaims this
        // process-scoped registration at process exit after this failure.
        std::fputs("Actual release failed; context/token retained; no automatic retry or unload\n", stderr);
        std::fflush(nullptr);
        std::_Exit(79);
    }
    Require(!g_Trace->context && !g_Trace->token, "normal release destroys unowned context");
}

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};
struct Rect {
    int left;
    int top;
    int right;
    int bottom;
};
struct Image {
    int width;
    int height;
    std::vector<unsigned char> pixels;
    Image(int wide, int tall, Color color) : width(wide), height(tall) {
        constexpr int kMaximumSceneSide = 4096;
        Require(wide > 0 && tall > 0 && wide <= kMaximumSceneSide && tall <= kMaximumSceneSide,
            "bounded owned scene geometry");
        pixels.resize(static_cast<std::size_t>(wide) * tall * kPixelBytes);
        Fill({0, 0, width, height}, color);
    }
    unsigned char* Pixel(int x, int y) {
        return pixels.data() + (static_cast<std::size_t>(y) * width + x) * kPixelBytes;
    }
    void Fill(Rect rect, Color color) {
        for (int y = (std::max)(0, rect.top); y < (std::min)(height, rect.bottom); ++y) {
            for (int x = (std::max)(0, rect.left); x < (std::min)(width, rect.right); ++x) {
                auto* p = Pixel(x, y);
                p[0] = color.r;
                p[1] = color.g;
                p[2] = color.b;
                p[3] = kByteMaximum;
            }
        }
    }
    void Save(const std::string& path) const {
        png_image image{};
        image.version = PNG_IMAGE_VERSION;
        image.width = static_cast<png_uint_32>(width);
        image.height = static_cast<png_uint_32>(height);
        image.format = PNG_FORMAT_RGBA;
        Require(png_image_write_to_file(&image, path.c_str(), 0, pixels.data(), 0, nullptr) != 0,
            "owned text-scene PNG write");
        png_image_free(&image);
    }
};

std::vector<unsigned char> Bytes(const char* text) {
    const auto* first = reinterpret_cast<const unsigned char*>(text);
    return {first, first + std::strlen(text)};
}

int CursorPixels(vgui::Font& font, const std::vector<unsigned char>& text, std::size_t count) {
    Require(count <= text.size(), "authored cursor index within text");
    int width = 0;
    for (std::size_t i = 0; i < count; ++i) {
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(text[i], a, b, c);
        width += a + b + c; // Actual TextEntry::cursorToPixelSpace arithmetic.
    }
    return width;
}

void Text(Image& scene, vgui::Font& font, const std::vector<unsigned char>& text,
    int x, int y, Rect clip, Color color, FILE* layouts, const char* name) {
    std::array<unsigned char, kScratchSide * kScratchSide * kPixelBytes> glyph{};
    const int start = x;
    unsigned int covered = 0;
    for (const auto byte : text) {
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(byte, a, b, c);
        Require(a == 0 && b == kWidth && c == 0, "actual owner ABC for scene glyph");
        x += a;
        glyph.fill(0);
        font.getCharRGBA(byte, kPenX, kPenY, kScratchSide, kScratchSide, glyph.data());
        for (int row = 0; row < kHeight; ++row) {
            for (int col = 0; col < kWidth; ++col) {
                const int dx = x + col;
                const int dy = y + row;
                if (dx < 0 || dx >= scene.width || dy < 0 || dy >= scene.height
                    || dx < clip.left || dx >= clip.right || dy < clip.top || dy >= clip.bottom) {
                    continue;
                }
                const auto offset = static_cast<std::size_t>((row + kPenY) * kScratchSide + col + kPenX)
                    * kPixelBytes;
                const auto* source = glyph.data() + offset;
                const unsigned int alpha = source[3];
                covered += alpha != 0;
                auto* destination = scene.Pixel(dx, dy);
                const unsigned int colors[]{color.r, color.g, color.b};
                for (std::size_t channel = 0; channel < kPixelBytes - 1; ++channel) {
                    constexpr unsigned int kNearestByteHalf = 127;
                    // Explicit CPU visualization: nearest-pixel UNORM RGB
                    // modulation then source-alpha over, no gamma/filter claim.
                    const auto modulated = (source[channel] * colors[channel] + kNearestByteHalf) / kByteMaximum;
                    destination[channel] = static_cast<unsigned char>((modulated * alpha
                        + destination[channel] * (kByteMaximum - alpha) + kNearestByteHalf) / kByteMaximum);
                }
            }
        }
        // EngineSurfaceWrap::drawPrintText: x+=A; penX=C+x+wide.
        x = c + x + font.getWide();
    }
    Require(x - start == static_cast<int>(text.size()) * kWidth, "fixed-cell original pen arithmetic");
    std::fprintf(layouts, "%s\t%zu\t%d\t%d\t%d\t%d\t%u\t", name, text.size(), start, y,
        x, font.getTall(), covered);
    for (const auto byte : text) {
        std::fprintf(layouts, "%02x", byte);
    }
    std::fputc('\n', layouts);
    if (std::find(text.begin(), text.end(), 0) == text.end()
        && std::find(text.begin(), text.end(), '\n') == text.end()) {
        const std::string encoded(text.begin(), text.end());
        int measured = 0;
        int tall = 0;
        font.getTextSize(encoded.c_str(), measured, tall);
        Require(measured == x - start && tall == kHeight, "actual getTextSize matches row widths incl signed bytes");
    }
}

void Scenes(vgui::Font& font, const std::string& directory, FILE* layouts) {
    constexpr int kSceneWidth = 720;
    constexpr int kSceneHeight = 600;
    constexpr int kLineSpacing = 22;
    constexpr int kInset = 20;
    constexpr Color kDark{22, 25, 29};
    constexpr Color kLight{240, 240, 235};
    constexpr Color kText{232, 232, 225};
    constexpr Color kGold{230, 195, 40};
    constexpr Color kBlack{0, 0, 0};
    Image scene(kSceneWidth, kSceneHeight, kDark);
    const Rect full{0, 0, kSceneWidth, kSceneHeight};
    int y = kInset;
    const std::array<const char*, 11> lines{{
        "UNSELECTED FixedSys18 role / CPU text scenes / native pixel size",
        "H15 W8 ABC 0/8/0 baseline12/3 - visual review, no acceptance claim",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ  0123456789 +-*/= <>[]{}()",
        "abcdefghijklmnopqrstuvwxyz  Il1 O0 8B S5 Z2 rn m",
        "Caf\xE9 d\xE9j\xE0 vu; fa\xE7" "ade; \xC5" "ngstr\xF6m; Zo\xEB; Stra\xDF" "e",
        "Prices \xA3" "12.50 / \x80" "24.95  Degrees 18\xB0" "C  \xB1" "2  1\xBD + 1\xBC",
        "Punctuation \x91quoted\x92 \x93" "double\x94 \x96 en \x97 em \x85 \x99",
        "Score Name              Ping Loss  Time        Rate",
        "01    ALPHA               42    0  00:12:34   100.00",
        "02    BRAVO                7    1  01:02:03    99.75",
        "03    Caf\xE9                18    0  12:34:56   250.50"}};
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string name = "ordinary-" + std::to_string(i);
        Text(scene, font, Bytes(lines[i]), kInset, y, full, i < 2 ? kGold : kText, layouts, name.c_str());
        y += kLineSpacing;
    }
    Text(scene, font, Bytes("C0 tokens 01/08 use native .notdef; 0C/7F use authored default:"),
        kInset, y, full, kGold, layouts, "control-caption");
    y += kLineSpacing;
    auto controls = Bytes("Tokens: A");
    for (const auto byte : {0x01, 0x08, 0x0c, 0x7f}) {
        controls.push_back(static_cast<unsigned char>(byte));
        controls.push_back('B');
        controls.push_back(' ');
    }
    Text(scene, font, controls, kInset, y, full, kText, layouts, "control-text");
    y += kLineSpacing;
    auto blanks = Bytes("Blank spacing: ");
    for (const auto byte : {0x09, 0x20, 0xa0, 0x00, 0x02}) {
        blanks.push_back('|');
        blanks.push_back('A');
        blanks.push_back(static_cast<unsigned char>(byte));
        blanks.push_back('B');
        blanks.push_back('|');
        blanks.push_back(' ');
    }
    Text(scene, font, blanks, kInset, y, full, kText, layouts, "blank-explicit-length");
    y += kLineSpacing;
    scene.Fill({kInset, y + kBaseline, kSceneWidth - kInset, y + kBaseline + 1}, {95, 55, 55});
    Text(scene, font, Bytes("Baseline guide / gypqj / \xC0\xC9\xD6\xDC / \xE0\xE9\xF6\xFC"),
        kInset, y, full, kText, layouts, "baseline-text");

    constexpr int kLightTop = 370;
    scene.Fill({0, kLightTop, kSceneWidth, kSceneHeight}, kLight);
    Text(scene, font, Bytes("Light panel / original ABC selection and 3px text inset"), kInset, 385,
        full, kBlack, layouts, "selection-caption");
    constexpr Rect kEntry{kInset, 412, kSceneWidth - kInset, 434};
    scene.Fill(kEntry, {255, 255, 255});
    const auto entry = Bytes("0123456789 SELECT THIS TEXT 9876543210");
    constexpr std::size_t kSelectionStart = 11;
    constexpr std::size_t kSelectionEnd = 27;
    constexpr int kTextEntryInset = 3; // Actual TextEntry.cpp constant, no correction of its selection offset.
    const int start = CursorPixels(font, entry, kSelectionStart);
    const int end = CursorPixels(font, entry, kSelectionEnd);
    scene.Fill({kEntry.left + start, kEntry.top, kEntry.left + end, kEntry.top + kHeight + 1}, {0, 0, 200});
    Text(scene, font, entry, kEntry.left + kTextEntryInset, kEntry.top, kEntry, kBlack, layouts, "selection-text");
    scene.Fill({kEntry.left + end + kTextEntryInset, kEntry.top + 2,
        kEntry.left + end + kTextEntryInset + 1, kEntry.top + kHeight - 1}, kBlack);
    std::fprintf(layouts, "selection-range\t%zu\t%zu\t%d\t%d\tinset=%d\n",
        kSelectionStart, kSelectionEnd, start, end, kTextEntryInset);
    Text(scene, font, Bytes("Scissor crop below: partial left/right glyphs; source cells stay complete"),
        kInset, 454, full, kBlack, layouts, "clip-caption");
    constexpr Rect kClip{kInset, 481, 620, 498};
    scene.Fill(kClip, kDark);
    const auto clippedText = Bytes("CLIPPED LEFT / RIGHT: 0123456789 - Caf\xE9 - gypqj - "
        "ALPHA BRAVO CHARLIE DELTA ECHO");
    Image reference(kSceneWidth, kSceneHeight, kDark);
    Image clipped(kSceneWidth, kSceneHeight, kDark);
    Text(reference, font, clippedText, 9, kClip.top, full, kText, layouts, "unclipped-reference");
    Text(clipped, font, clippedText, 9, kClip.top, kClip, kText, layouts, "clipped-control");
    unsigned int clippedLeftInk = 0;
    unsigned int clippedRightInk = 0;
    for (int row = 0; row < kSceneHeight; ++row) {
        for (int col = 0; col < kSceneWidth; ++col) {
            const bool inside = col >= kClip.left && col < kClip.right && row >= kClip.top && row < kClip.bottom;
            auto* actual = clipped.Pixel(col, row);
            if (inside) {
                Require(std::memcmp(actual, reference.Pixel(col, row), kPixelBytes) == 0, "exact CPU scissor interior");
                std::memcpy(scene.Pixel(col, row), actual, kPixelBytes);
            } else {
                Require(actual[0] == kDark.r && actual[1] == kDark.g && actual[2] == kDark.b,
                    "CPU scissor leaves every outside guard unchanged");
                const auto* uncut = reference.Pixel(col, row);
                const bool ink = uncut[0] != kDark.r || uncut[1] != kDark.g || uncut[2] != kDark.b;
                clippedLeftInk += ink && col < kClip.left;
                clippedRightInk += ink && col >= kClip.right;
            }
        }
    }
    Require(clippedLeftInk > 0 && clippedRightInk > 0, "both clip edges actually discard measured ink");
    std::fprintf(layouts, "clip-ink\tleft=%u\tright=%u\n", clippedLeftInk, clippedRightInk);
    Text(scene, font, Bytes("1:1 text. Magnified copy uses integer nearest-neighbor only."), kInset, 527,
        full, kBlack, layouts, "scale-caption");
    Text(scene, font, Bytes("CPU modulation/alpha convention; no atlas, GPU or whole-UI pixel claim."), kInset, 549,
        full, kBlack, layouts, "scope-caption");
    scene.Save(directory + "/contact-sheet.png");
    constexpr int kMagnification = 3;
    Image magnified(kSceneWidth * kMagnification, kSceneHeight * kMagnification, kDark);
    for (int row = 0; row < magnified.height; ++row) {
        for (int col = 0; col < magnified.width; ++col) {
            std::memcpy(magnified.Pixel(col, row), scene.Pixel(col / kMagnification, row / kMagnification),
                kPixelBytes);
        }
    }
    magnified.Save(directory + "/contact-sheet-3x.png");
    Digest digest;
    digest.Update(scene.pixels.data(), scene.pixels.size());
    std::fprintf(g_Trace->journal, "scene\twidth=%d\theight=%d\trgba-sha256=%s\n",
        scene.width, scene.height, digest.Finish().c_str());
}

void Cells(vgui::Font& font, FILE* output) {
    std::array<unsigned char, kScratchSide * kScratchSide * kPixelBytes> pixels{};
    for (unsigned int byte = 0; byte < kByteCount; ++byte) {
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(static_cast<int>(byte), a, b, c);
        Require(a == 0 && b == kWidth && c == 0, "all256 original owner ABC");
        pixels.fill(0x5a);
        font.getCharRGBA(static_cast<int>(byte), kPenX, kPenY, kScratchSide, kScratchSide, pixels.data());
        if (byte >= 128) {
            auto signedPixels = pixels;
            signedPixels.fill(0x5a);
            const int signedByte = static_cast<int>(byte) - static_cast<int>(kByteCount);
            font.getCharRGBA(signedByte, kPenX, kPenY, kScratchSide, kScratchSide, signedPixels.data());
            Require(pixels == signedPixels, "all128 sign-extended byte rasters match");
            font.getCharABCwide(signedByte, a, b, c);
            Require(a == 0 && b == kWidth && c == 0, "all128 sign-extended ABCs match");
        }
        unsigned int count = 0;
        unsigned int sum = 0;
        int left = kWidth;
        int top = kHeight;
        int right = 0;
        int bottom = 0;
        for (int y = 0; y < kScratchSide; ++y) {
            for (int x = 0; x < kScratchSide; ++x) {
                const auto offset = static_cast<std::size_t>(y * kScratchSide + x) * kPixelBytes;
                if (x < kPenX || x >= kPenX + kWidth || y < kPenY || y >= kPenY + kHeight) {
                    Require(pixels[offset] == 0x5a && pixels[offset + 3] == 0x5a, "owner canvas outside guard");
                    continue;
                }
                const auto alpha = pixels[offset + 3];
                sum += alpha;
                if (alpha) {
                    ++count;
                    left = (std::min)(left, x - kPenX);
                    top = (std::min)(top, y - kPenY);
                    right = (std::max)(right, x - kPenX + 1);
                    bottom = (std::max)(bottom, y - kPenY + 1);
                }
            }
        }
        Require((g_Trace->routes[byte] == FixedSysByteRoute::ForcedBlank) == (count == 0),
            "all256 owner blank/nonempty classification");
        Digest digest;
        digest.Update(pixels.data(), pixels.size());
        std::fprintf(output, "%u\t%u\t%u\t%d\t%d\t%d\t%u\t%u\t%d\t%d\t%d\t%d\t%s\n",
            byte, static_cast<unsigned int>(g_Trace->routes[byte]), g_Trace->glyphs[byte], a, b, c,
            count, sum, left, top, right, bottom, digest.Finish().c_str());
    }
}
}

namespace cse_vgui1 {
// Test-only qualified binding; production still has no qualified factory.
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    static const FontProviderV1 provider{Create, Destroy, Register, Release, Realize};
    return provider;
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) {
    std::fputs("Unexpected real Surface path in CPU readability fixture\n", stderr);
    std::abort();
}
}

int main(int argc, char** argv) {
    Require(argc == 3, "exact resource and fresh output directory");
    const std::string directory = argv[2];
    Trace trace;
    trace.resource = argv[1];
    Require(FileHash(kCourierPath) == kCourierHash && FileHash(argv[1]) == kMarlettHash,
        "both exact resource identities before any service action");
    trace.journal = std::fopen((directory + "/owner.tsv").c_str(), "wb");
    FILE* cells = std::fopen((directory + "/owner-cells.tsv").c_str(), "wb");
    FILE* rebound = std::fopen((directory + "/rebound-cells.tsv").c_str(), "wb");
    FILE* layouts = std::fopen((directory + "/layouts.tsv").c_str(), "wb");
    Require(trace.journal && cells && rebound && layouts, "fresh output files");
    g_Trace = &trace;
    char requestedName[] = "FixedSys";
    vgui::Font font(requestedName, 18, 0, 0.0f, 400, false, false, false, false);
    requestedName[0] = 'Z'; // Actual owner must retain the original name independently.
    vgui::Font alias("fIxEdSyS", 18, 0, 0.0f, 400, false, false, false, false);
    Require(font.getPlat() && alias.getPlat() == font.getPlat() && font.getId() == VGUI_FIRST_FONT_ID
        && alias.getId() == -1, "retained requested-name cache and cache-hit ID behavior");
    std::fprintf(trace.journal, "cache\tfirst-id=%d\talias-id=%d\tsame-platform=1\n", font.getId(), alias.getId());
    Inspect(font, false, OwnerResult::PendingResources, "initial-query");
    Require(trace.requests.empty() && trace.creates == 0, "pending construction/query does not execute provider");
    vgui::Font_Reset();
    Bind();
    Inspect(font, false, OwnerResult::PendingRealization, "retired-query");
    Require(CseVgui1FontsPreflightV1(nullptr, 0) == OwnerResult::Ready && trace.requests.empty(),
        "global preflight explicitly covers current lookup generation only");
    const auto ready = Inspect(font, true, OwnerResult::Ready, "retained-prepare");
    Require(ready.capabilities == 39 && font.getTall() == kHeight && font.getWide() == kWidth,
        "actual owner ready metrics/capabilities");
    Bind();
    Require(Inspect(alias, true, OwnerResult::Ready, "cached-prepare").catalogGeneration == ready.catalogGeneration
        && trace.requests.size() == 1 && trace.registrations == 1,
        "duplicate bind/ready/cache keeps identities stable");
    Inspect(font, false, OwnerResult::Ready, "ready-query");
    Cells(font, cells);
    Scenes(font, directory, layouts);
    int wide = -1;
    int tall = -1;
    font.getTextSize("ROW 01\nROW 02", wide, tall);
    Require(wide == 6 * kWidth && tall == 2 * kHeight, "actual multiline getTextSize first-line/height semantics");
    font.getTextSize("", wide, tall);
    Require(wide == 0 && tall == kHeight, "actual empty text first-line height");
    font.getTextSize(nullptr, wide, tall);
    Require(wide == 0 && tall == 0, "actual null text output semantics");
    auto* platform = font.getPlat();
    const int id = font.getId();
    Unbind();
    Inspect(font, false, OwnerResult::PendingResources, "released-query");
    Inspect(font, true, OwnerResult::PendingResources, "released-prepare");
    Require(trace.requests.size() == 1, "released retained borrower cannot realize without a catalog");
    Bind();
    Inspect(font, true, OwnerResult::Ready, "rebound-retained-prepare");
    Require(font.getPlat() == platform && font.getId() == id && trace.requests.size() == 2,
        "normal rebind retains public platform/ID while realizing a new owned face");
    Cells(font, rebound);
    Unbind();
    Require(trace.creates == 2 && trace.destroys == 2 && trace.registrations == 2 && trace.releases == 2,
        "two normal owned registration/release/context lifecycles");
    Require(FileHash(kCourierPath) == kCourierHash && FileHash(argv[1]) == kMarlettHash, "resource programs unchanged");
    const auto info = trace.info;
    std::fprintf(trace.journal, "role-fit\tH=%d\tW=%d\tbaseline=%d/%d\tleading=%d\tppem=%d\torigin=%d\t"
        "bounds=%d/%d/%d/%d\troleWeight=%d\tnumericWeightKnown=%d\tnumericWeight=%u\n",
        kHeight, kWidth, info.ascent, info.descent, info.internalLeading, info.fitPpem, info.originX,
        info.minimumBitmapLeft, info.maximumBitmapRight, info.maximumBitmapTop, info.maximumBitmapBottom,
        info.roleWeight, info.numericWeightKnown, info.numericWeight);
    std::fprintf(trace.journal, "cleanup\tcreates=%d\tdestroys=%d\tregistrations=%d\treleases=%d\t"
        "remaining-context=0\tremaining-token=0\tSurface-calls=0\n",
        trace.creates, trace.destroys, trace.registrations, trace.releases);
    Require(std::fclose(cells) == 0 && std::fclose(rebound) == 0 && std::fclose(layouts) == 0
        && std::fclose(trace.journal) == 0, "measurement output close");
    g_Trace = nullptr; // Owner has no context/token; global teardown needs no fixture callbacks.
    std::puts("PASS actual Font owner/candidate CPU text scenes; visual/UI acceptance remains open");
}
