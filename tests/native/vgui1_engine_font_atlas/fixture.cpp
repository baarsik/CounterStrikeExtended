#include "VGUI_EngineSurfaceWrap.h"
#include "IEngineSurface.h"
#include "VGUI_Font.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "atlas_state.inc"
#include "atlas_methods.inc"

namespace {
constexpr int kSide = 512;
constexpr int kByteCount = 256;
constexpr int kChannels = 4;
constexpr int kInitializedPages = 4;
constexpr int kTextureIdBase = 1000;
constexpr int kDiagnosticGlyphWidth = 64;
constexpr int kDiagnosticGlyphHeight = 127;
constexpr int kPaintsBeforeFifthPage = (kSide / (kDiagnosticGlyphWidth + 1))
    * (kSide / (kDiagnosticGlyphHeight + 1)) * kInitializedPages;
constexpr float kUvScale = 1.0f / kSide;
constexpr std::size_t kPageBytes = static_cast<std::size_t>(kSide) * kSide * kChannels;

[[noreturn]] void Fail(const char* message) {
    std::fprintf(stderr, "FAILED actual atlas methods: %s\n", message);
    std::fflush(nullptr);
    std::abort();
}
void Check(bool condition, const char* message) {
    if (!condition) {
        Fail(message);
    }
}

template<class T> class DarStorage : public vgui::Dar<T> {
public:
    static auto Data() {
        return &DarStorage::_data;
    }
    static auto Count() {
        return &DarStorage::_count;
    }
    static auto Capacity() {
        return &DarStorage::_capacity;
    }
    static void Release(vgui::Dar<T>& values) {
        // Legal protected member pointers; no layout copy/downcast. This is
        // fixture-owned cleanup, not a production Dar lifetime claim.
        delete[] (values.*Data());
        values.*Data() = nullptr;
        values.*Count() = 0;
        values.*Capacity() = 0;
    }
};

void ClearAtlas() {
    for (int i = 0; i < staticFontInfoVGUIDar.getCount(); ++i) {
        ::operator delete(staticFontInfoVGUIDar[i]);
    }
    staticFontInfoVGUIDar.setCount(0);
    staticFont = nullptr;
    staticFontInfoVGUI = nullptr;
}

struct Paint {
    int byte;
    int page;
    int x;
    int y;
    int width;
    int height;
};
struct Quad {
    int texture;
    int x;
    int y;
    int width;
    int height;
    std::array<float, 4> uv;
};
class Recorder final : public IEngineSurface {
public:
    struct Upload {
        int texture;
        std::vector<unsigned char> pixels;
    };
    std::vector<Upload> uploads;
    std::vector<Quad> quads;
    std::vector<int> binds;
    std::vector<std::array<int, 2>> positions;
    int nextTexture = kTextureIdBase;
    int selectedTexture = 0;
    int x = 13;
    int y = 19;
    int gets = 0;
    int* paintCount = nullptr;
    bool capacityStop = false;

    int createNewTextureID() override {
        return ++nextTexture;
    }
    void drawSetTextureRGBA(int texture, const char* bytes, int width, int height, int filter, int reload) override {
        Check(bytes && width == kSide && height == kSide && filter == 0 && reload == 0,
            "actual upload dimensions/flags");
        Check(texture > kTextureIdBase && texture <= nextTexture, "initialized texture ownership");
        const auto* first = reinterpret_cast<const unsigned char*>(bytes);
        uploads.push_back({texture, {first, first + kPageBytes}});
        if (capacityStop && uploads.size() == kInitializedPages && paintCount
            && *paintCount == kPaintsBeforeFifthPage) {
            std::puts("DIAGNOSTIC intercepted after page3 upload, before uninitialized bind slot4; no bad-slot read");
            std::fflush(nullptr);
            std::_Exit(78);
        }
    }
    void drawSetTexture(int texture) override {
        Check(texture > kTextureIdBase && texture <= nextTexture, "draw binds an owned texture");
        selectedTexture = texture;
        binds.push_back(texture);
    }
    void drawSetTextPos(int px, int py) override {
        x = px;
        y = py;
        positions.push_back({{px, py}});
    }
    void drawGetTextPos(int& px, int& py) override {
        ++gets;
        px = x;
        py = y;
    }
    void drawPrintChar(int px, int py, int width, int height, float s0, float t0, float s1, float t1) override {
        quads.push_back({selectedTexture, px, py, width, height, {{s0, t0, s1, t1}}});
    }
#define RECORD_ABORT(signature) signature override { Fail(#signature); }
    RECORD_ABORT(void pushMakeCurrent(int*, int*, int*, bool))
    RECORD_ABORT(void popMakeCurrent())
    RECORD_ABORT(void drawFilledRect(int, int, int, int))
    RECORD_ABORT(void drawOutlinedRect(int, int, int, int))
    RECORD_ABORT(void drawLine(int, int, int, int))
    RECORD_ABORT(void drawPolyLine(int*, int*, int))
    RECORD_ABORT(void drawTexturedPolygon(vgui2::VGuiVertex*, int))
    RECORD_ABORT(void drawTexturedRect(int, int, int, int))
    RECORD_ABORT(void drawTexturedRectAdd(int, int, int, int))
    RECORD_ABORT(void drawSetColor(int, int, int, int))
    RECORD_ABORT(void drawSetTextColor(int, int, int, int))
    RECORD_ABORT(void drawPrintCharAdd(int, int, int, int, float, float, float, float))
    RECORD_ABORT(void drawSetTextureFile(int, const char*, int, bool))
    RECORD_ABORT(void drawGetTextureSize(int, int&, int&))
    RECORD_ABORT(int isTextureIdValid(int))
    RECORD_ABORT(void drawSetSubTextureRGBA(int, int, int, const unsigned char*, int, int))
    RECORD_ABORT(void drawFlushText())
    RECORD_ABORT(void resetViewPort())
    RECORD_ABORT(void drawSetTextureBGRA(int, const char*, int, int, int, int))
    RECORD_ABORT(void drawUpdateRegionTextureBGRA(int, int, int, const unsigned char*, int, int))
#undef RECORD_ABORT
};

class ModelFont final : public vgui::Font {
public:
    struct Abc {
        int a;
        int b;
        int c;
    };
    std::array<Abc, kByteCount> abc{};
    std::vector<Paint> paints;
    Recorder& owner;
    int paintCount = 0;
    int maximum = 16;
    int height = 17;
    int id;
    unsigned char tag;

    ModelFont(Recorder& recorder, int identity, unsigned char color) :
        Font("authored model", 17, 0, 0.0f, 400, false, false, false, false),
        owner(recorder), id(identity), tag(color) {
        for (int i = 0; i < kByteCount; ++i) {
            abc[static_cast<std::size_t>(i)] = {0, 1 + i % 11, 0};
        }
        abc['W'] = {1, 11, 0};
        abc['i'] = {0, 2, 1};
        abc['A'] = {1, 6, -1};
    }
    static int Byte(int value) {
        Check(value >= -128 && value < kByteCount, "controlled byte domain");
        return value < 0 ? value + kByteCount : value;
    }
    void Uniform(int width, int tall) {
        maximum = width;
        height = tall;
        for (auto& value : abc) {
            value = {0, width, 0};
        }
    }
    std::array<unsigned char, kChannels> Pixel(int byte, int column) const {
        return {{static_cast<unsigned char>(byte), tag, static_cast<unsigned char>(column + 1), 255}};
    }
    void getCharABCwide(int value, int& a, int& b, int& c) override {
        const auto v = abc[static_cast<std::size_t>(Byte(value))];
        a = v.a;
        b = v.b;
        c = v.c;
    }
    void getCharRGBA(int value, int px, int py, int width, int tall, vgui::uchar* bytes) override {
        const int byte = Byte(value);
        const int inkWidth = abc[static_cast<std::size_t>(byte)].b;
        Check(bytes && px >= 0 && py >= 0 && width == kSide && tall == kSide,
            "actual atlas requests an owned positive canvas");
        Check(px + inkWidth <= width && py + height <= tall, "authored raster fits actual requested atlas cell");
        paints.push_back({byte, static_cast<int>(owner.uploads.size()), px, py, inkWidth, height});
        ++paintCount;
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < inkWidth; ++column) {
                const auto pixel = Pixel(byte, column);
                const auto offset = (static_cast<std::size_t>(py + row) * width + px + column) * kChannels;
                std::memcpy(bytes + offset, pixel.data(), pixel.size());
            }
        }
    }
    int getTall() override {
        return height;
    }
#if !defined(_WIN32)
    int getWide() override {
        return maximum;
    }
#endif
    int getId() override {
        return id;
    }
};

class Access final : public EngineSurfaceWrap {
public:
    explicit Access(IEngineSurface* owner) : EngineSurfaceWrap(nullptr, owner) {}
    using EngineSurfaceWrap::drawSetTextFont;
    using EngineSurfaceWrap::drawPrintText;
    using EngineSurfaceWrap::invalidate;
};

bool Admission(ModelFont& font) {
    std::array<int, kByteCount> count{};
    for (const auto& paint : font.paints) {
        ++count[static_cast<std::size_t>(paint.byte)];
    }
    const int admitted = static_cast<int>(std::count(count.begin(), count.end(), 1));
    std::printf("admission expected=256 actual=%d locale=%s\n", admitted, std::setlocale(LC_CTYPE, nullptr));
    return admitted == kByteCount;
}

void ValidateAtlas(const ModelFont& font, const Recorder& owner) {
    Check(!owner.uploads.empty() && owner.uploads.size() <= kInitializedPages, "core stays in initialized slots0..3");
    Check(staticFontInfoVGUI && staticFontInfoVGUI->pageCount == static_cast<int>(owner.uploads.size()),
        "page count matches complete owned uploads");
    for (const auto& paint : font.paints) {
        const auto index = static_cast<std::size_t>(paint.byte);
        const auto& metrics = font.abc[index];
        Check(paint.width == metrics.b && paint.page >= 0
            && static_cast<std::size_t>(paint.page) < owner.uploads.size(), "per-glyph B and page bounds");
        const auto* uv = staticFontInfoVGUI->texCoord[index];
        const std::array<float, 4> expected{{paint.x * kUvScale, paint.y * kUvScale,
            (paint.x + metrics.b) * kUvScale, (paint.y + font.height) * kUvScale}};
        Check(std::equal(expected.begin(), expected.end(), uv), "exact per-B power-of-two UVs");
        Check(staticFontInfoVGUI->pageForChar[index] == paint.page, "each initialized byte maps to its actual page");
    }
    for (std::size_t page = 0; page < owner.uploads.size(); ++page) {
        std::vector<unsigned char> expected(kPageBytes, 0);
        for (const auto& paint : font.paints) {
            if (paint.page != static_cast<int>(page)) {
                continue;
            }
            for (int row = 0; row < paint.height; ++row) {
                for (int col = 0; col < paint.width; ++col) {
                    const auto pixel = font.Pixel(paint.byte, col);
                    const auto offset = (static_cast<std::size_t>(paint.y + row) * kSide + paint.x + col) * kChannels;
                    Check(expected[offset + 3] == 0, "glyph bodies never overlap");
                    std::memcpy(expected.data() + offset, pixel.data(), pixel.size());
                }
            }
        }
        Check(expected == owner.uploads[page].pixels, "complete uploaded pixel bytes and zero padding");
    }
}

bool Width(Access& surface, ModelFont& font, Recorder& owner) {
    int measured = 0;
    int tall = 0;
    font.getTextSize("WiA", measured, tall);
    surface.drawPrintText("WiA", 3);
    std::printf("width measured=%d actual=%d end=%d expected-end=34\n", measured, owner.x - 13, owner.x);
    return measured == 21 && tall == 17 && owner.x == 34;
}

void DrawChecks(Access& surface, ModelFont& font, Recorder& owner) {
    const unsigned char text[]{'W', 'i', 'A', 0xe9, 0x80};
    owner.x = 13;
    owner.y = 19;
    owner.quads.clear();
    owner.binds.clear();
    surface.drawPrintText(reinterpret_cast<const char*>(text), static_cast<int>(sizeof(text)));
    Check(owner.quads.size() == sizeof(text) && owner.binds.size() == sizeof(text), "complete draw command count");
    int pen = 13;
    for (std::size_t i = 0; i < sizeof(text); ++i) {
        const auto byte = text[i];
        const auto abc = font.abc[byte];
        const auto& quad = owner.quads[i];
        pen += abc.a;
        const auto* uv = staticFontInfoVGUI->texCoord[byte];
        const int page = staticFontInfoVGUI->pageForChar[byte];
        Check(quad.x == pen && quad.y == 19 && quad.width == abc.b && quad.height == font.height,
            "actual draw rectangle uses A/B/height");
        Check(std::equal(quad.uv.begin(), quad.uv.end(), uv), "draw forwards the selected byte UVs exactly");
        Check(quad.texture == owner.uploads[static_cast<std::size_t>(page)].texture
            && owner.binds[i] == quad.texture, "actual page/bind command order");
        pen += abc.b + abc.c;
    }
    Check(owner.x == pen && owner.y == 19 && owner.quads.size() == sizeof(text), "actual final text pen");
    const auto draws = owner.quads.size();
    const int gets = owner.gets;
    surface.drawPrintText(nullptr, 9);
    Check(owner.gets == gets && owner.quads.size() == draws, "null text returns before engine calls");
    surface.drawPrintText("A", 0);
    surface.drawPrintText("A", -1);
    Check(owner.x == pen && owner.quads.size() == draws, "zero/negative length preserves pen without glyph draws");
    const auto uploads = owner.uploads.size();
    const auto paints = font.paints.size();
    surface.drawSetTextFont(&font);
    surface.invalidate(nullptr);
    surface.drawSetTextFont(&font);
    Check(owner.uploads.size() == uploads && font.paints.size() == paints,
        "same ID/context cache reuse and inherited empty invalidate");
    surface.drawSetTextFont(nullptr);
    surface.drawPrintText("A", 1);
    Check(owner.quads.size() == draws, "null font makes text no-op");
    int measured = -1;
    int tall = -1;
    font.getTextSize(nullptr, measured, tall);
    Check(measured == 0 && tall == 0, "whole Font null measurement");
    font.getTextSize("", measured, tall);
    Check(measured == 0 && tall == font.height, "whole Font empty first-line height");
    font.getTextSize("W\ni", measured, tall);
    Check(measured == 12 && tall == font.height * 2, "whole Font newline measurement semantics");
    surface.drawSetTextFont(&font);
    const char binary[]{'A', 0, 'i'};
    owner.x = 13;
    const auto beforeBinary = owner.quads.size();
    surface.drawPrintText(binary, static_cast<int>(sizeof(binary)));
    font.getTextSize(binary, measured, tall);
    Check(owner.quads.size() == beforeBinary + sizeof(binary) && owner.x == 23 && measured == 6,
        "explicit byte length includes NUL while getTextSize retains C-string termination");
    ++staticContextCount; // Explicitly simulated private generation, not a GPU context-loss hook.
    surface.drawSetTextFont(&font);
    Check(owner.uploads.size() == uploads * 2 && font.paints.size() == paints * 2,
        "actual stale-context branch rebuilds through controlled generation");
}

void DrawAllBytes(Access& surface, ModelFont& font, Recorder& owner) {
    std::array<unsigned char, kByteCount> text{};
    for (int byte = 0; byte < kByteCount; ++byte) {
        text[static_cast<std::size_t>(byte)] = static_cast<unsigned char>(byte);
    }
    owner.x = 13;
    owner.y = 19;
    owner.quads.clear();
    owner.binds.clear();
    surface.drawPrintText(reinterpret_cast<const char*>(text.data()), static_cast<int>(text.size()));
    Check(owner.quads.size() == text.size() && owner.binds.size() == text.size(), "all256 explicit byte draw calls");
    int pen = 13;
    for (std::size_t byte = 0; byte < text.size(); ++byte) {
        const auto abc = font.abc[byte];
        const auto& quad = owner.quads[byte];
        pen += abc.a;
        const int page = staticFontInfoVGUI->pageForChar[byte];
        const auto* uv = staticFontInfoVGUI->texCoord[byte];
        Check(page >= 0 && static_cast<std::size_t>(page) < owner.uploads.size(), "all256 draw page bounds");
        Check(quad.x == pen && quad.y == 19 && quad.width == abc.b && quad.height == font.height
            && quad.texture == owner.uploads[static_cast<std::size_t>(page)].texture
            && owner.binds[byte] == quad.texture && std::equal(quad.uv.begin(), quad.uv.end(), uv),
            "all256 texture/UV/ABC rectangles and command order");
        pen += abc.b + abc.c;
    }
    Check(owner.x == pen && owner.y == 19, "all256 end pen uses original ABC arithmetic");
}

void Packing(int width, int height, int expectedPages) {
    Recorder owner;
    ModelFont font(owner, 200 + width, 77);
    font.Uniform(width, height);
    Access surface(&owner);
    surface.drawSetTextFont(&font);
    Check(Admission(font), "packing corpus admits all256");
    ValidateAtlas(font, owner);
    DrawAllBytes(surface, font, owner);
    Check(static_cast<int>(owner.uploads.size()) == expectedPages, "known complete page count");
    if (width == 63 && height == 63) {
        Check(font.paints[7].x == 448 && font.paints[7].y == 0
            && font.paints[8].x == 0 && font.paints[8].y == 64
            && font.paints[63].x == 448 && font.paints[63].y == 448
            && font.paints[64].page == 1 && font.paints[64].x == 0 && font.paints[64].y == 0,
            "511 edge admitted, exact row/page transition");
    }
    std::printf("packing B=%d H=%d pages=%zu\n", width, height, owner.uploads.size());
    ClearAtlas();
}
}

// Valid authored construction of the REAL complete classes. Unrelated virtuals
// are typed abort bindings; no actual App, mouse.tga, SDL or provider is called.
namespace vgui {
SurfaceBase::SurfaceBase(Panel* panel) : _needsSwap(false), _app(nullptr), _embeddedPanel(panel),
    _emulatedCursor(nullptr), _currentCursor(nullptr) {}
SurfaceBase::~SurfaceBase() {
    DarStorage<char*>::Release(_modeInfoDar);
}
Font::Font(const char*, int, int, float, int, bool, bool, bool, bool) : _name(nullptr), _plat(nullptr), _id(-1) {}
Font::Font(const char*, void*, int, int, int, float, int, bool, bool, bool, bool) :
    _name(nullptr), _plat(nullptr), _id(-1) {}
#define TYPED_ABORT(signature) signature { Fail(#signature); }
TYPED_ABORT(void Font::init(const char*, void*, int, int, int, float, int, bool, bool, bool, bool))
TYPED_ABORT(void Font::getCharRGBA(int, int, int, int, int, uchar*))
TYPED_ABORT(void Font::getCharABCwide(int, int&, int&, int&))
TYPED_ABORT(int Font::getTall())
#if !defined(_WIN32)
TYPED_ABORT(int Font::getWide())
#endif
TYPED_ABORT(Panel* SurfaceBase::getPanel())
TYPED_ABORT(void SurfaceBase::requestSwap())
TYPED_ABORT(void SurfaceBase::resetModeInfo())
TYPED_ABORT(int SurfaceBase::getModeInfoCount())
TYPED_ABORT(bool SurfaceBase::getModeInfo(int, int&, int&, int&))
TYPED_ABORT(App* SurfaceBase::getApp())
TYPED_ABORT(void SurfaceBase::setEmulatedCursorVisible(bool))
TYPED_ABORT(void SurfaceBase::setEmulatedCursorPos(int, int))
TYPED_ABORT(void SurfaceBase::addModeInfo(int, int, int))
#undef TYPED_ABORT
}
EngineSurfaceWrap::EngineSurfaceWrap(vgui::Panel* panel, IEngineSurface* owner) : SurfaceBase(panel),
    _mouseBitmap(nullptr), _engineSurface(owner), _cursorLocked(false) {
    Check(staticFontInfoVGUIDar.getCount() == 0, "fixture cleaned prior cache before valid construction");
    staticFont = nullptr;
    staticFontInfoVGUI = nullptr;
    staticFontInfoVGUIDar.setCount(0);
    ++staticContextCount;
}
EngineSurfaceWrap::~EngineSurfaceWrap() {
    Check(staticFont == nullptr, "font objects outlive all atlas uses");
    Check(_mouseBitmap == nullptr && !_cursorLocked, "fixture never acquires cursor/bitmap provider state");
}
#define TYPED_ABORT(signature) signature { Fail(#signature); }
TYPED_ABORT(void EngineSurfaceWrap::setTitle(const char*))
TYPED_ABORT(bool EngineSurfaceWrap::setFullscreenMode(int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::setWindowedMode())
TYPED_ABORT(void EngineSurfaceWrap::setAsTopMost(bool))
TYPED_ABORT(void EngineSurfaceWrap::createPopup(vgui::Panel*))
TYPED_ABORT(bool EngineSurfaceWrap::hasFocus())
TYPED_ABORT(bool EngineSurfaceWrap::isWithin(int, int))
TYPED_ABORT(void EngineSurfaceWrap::GetMousePos(int&, int&))
TYPED_ABORT(void EngineSurfaceWrap::setCursor(vgui::Cursor*))
TYPED_ABORT(void EngineSurfaceWrap::AppHandler(void*, void*))
TYPED_ABORT(void EngineSurfaceWrap::lockCursor())
TYPED_ABORT(void EngineSurfaceWrap::unlockCursor())
TYPED_ABORT(void EngineSurfaceWrap::drawSetColor(int, int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::drawFilledRect(int, int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::drawOutlinedRect(int, int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::drawSetTextColor(int, int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::drawTexturedRect(int, int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::enableMouseCapture(bool))
TYPED_ABORT(void EngineSurfaceWrap::swapBuffers())
TYPED_ABORT(void EngineSurfaceWrap::pushMakeCurrent(vgui::Panel*, bool))
TYPED_ABORT(void EngineSurfaceWrap::popMakeCurrent(vgui::Panel*))
TYPED_ABORT(void EngineSurfaceWrap::applyChanges())
TYPED_ABORT(void EngineSurfaceWrap::drawLine(int, int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::drawPolyLine(int*, int*, int))
TYPED_ABORT(void EngineSurfaceWrap::drawTexturedPolygon(vgui::VGuiVertex*, int))
TYPED_ABORT(void EngineSurfaceWrap::drawSetTextureBGRA(int, const char*, int, int, int, int))
TYPED_ABORT(void EngineSurfaceWrap::drawUpdateRegionTextureBGRA(int, int, int, const unsigned char*, int, int))
TYPED_ABORT(void EngineSurfaceWrap::drawPrintCharAdd(int, int, int, int, float, float, float, float))
#undef TYPED_ABORT

int main(int argc, char** argv) {
    Check(argc == 2, "mode argument");
    const std::string mode = argv[1];
    int result = 0;
    {
        Recorder owner;
        ModelFont font(owner, 100, 41);
        Access surface(&owner);
        if (mode == "--capacity-diagnostic") {
            font.Uniform(kDiagnosticGlyphWidth, kDiagnosticGlyphHeight);
            owner.paintCount = &font.paintCount;
            owner.capacityStop = true;
        }
        surface.drawSetTextFont(&font);
        if (mode == "--admission") {
            result = Admission(font) ? 0 : 1;
        } else if (mode == "--width") {
            result = Width(surface, font, owner) ? 0 : 1;
        } else if (mode == "--core") {
            Check(Admission(font), "all256 admission before any skipped-byte UV read");
            ValidateAtlas(font, owner);
            Check(Width(surface, font, owner), "measurement/draw pen contract");
            DrawChecks(surface, font, owner);
            DrawAllBytes(surface, font, owner);
        } else {
            Fail("unknown mode or expected capacity intercept did not occur");
        }
        ClearAtlas();
    }
    if (mode == "--core") {
        Packing(63, 63, 4);
        Packing(31, 31, 1);
        {
            Recorder owner;
            ModelFont font(owner, 400, 19);
            font.Uniform(1, 15);
            font.abc[1].b = 510;
            font.abc[2].b = 0;
            font.maximum = 510;
            Access surface(&owner);
            surface.drawSetTextFont(&font);
            Check(Admission(font), "zero-width byte remains admitted");
            ValidateAtlas(font, owner);
            DrawAllBytes(surface, font, owner);
            Check(font.paints[0].x == 0 && font.paints[0].width == 1
                && font.paints[1].x == 0 && font.paints[1].y == 16
                && font.paints[2].x == 511 && font.paints[2].width == 0
                && font.paints[3].x == 0 && font.paints[3].y == 32,
                "512 wraps, 511 zero-width boundary and one pixel padding are exact");
            ClearAtlas();
        }
        {
            Recorder owner;
            ModelFont first(owner, 501, 41);
            ModelFont second(owner, 502, 73);
            Access surface(&owner);
            surface.drawSetTextFont(&first);
            surface.drawSetTextFont(&second);
            const auto uploads = owner.uploads.size();
            surface.drawSetTextFont(&first);
            Check(first.paintCount == kByteCount && second.paintCount == kByteCount
                && owner.uploads.size() == uploads, "distinct miss IDs own distinct atlases and reuse independently");
            ClearAtlas();
        }
        {
            Recorder owner;
            ModelFont first(owner, -1, 41);
            ModelFont second(owner, -1, 73);
            Access surface(&owner);
            surface.drawSetTextFont(&first);
            const auto uploads = owner.uploads.size();
            surface.drawSetTextFont(&second);
            Check(second.paints.empty() && owner.uploads.size() == uploads, "inherited ID-1 cache collision observed");
            std::puts("DIAGNOSTIC distinct ID-1 model fonts collide; cache-key/lifetime fix is outside selector scope");
            ClearAtlas();
        }
        std::puts("PASS actual extracted methods/whole Font measurement; no provider/GPU/SDL/readiness claim");
    }
    DarStorage<FontInfoVGUI*>::Release(staticFontInfoVGUIDar);
    return result;
}
