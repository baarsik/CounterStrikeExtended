#include "VGui1NativeRuntime.h"
#include "IEngineSurface.h"
#include "vid.h"
#include "VGui1HostImageOwner.h"
#include "VGUI_Font.h"
#include "native_module.h"
#include "native_runtime_host.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" int CseNativeRuntimeOpenV1(const cse::native_runtime::BindingV1*) noexcept;
extern "C" int CseNativeRuntimeCloseV1() noexcept;
extern int is15bit;

int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace cse_vgui1_native;
    const std::string enginePath = std::string(argv[1]) + "/swds.dylib";
    const std::string fontPath = std::string(argv[1]) + "/vgui.dylib";
    void* image = cse_platform::LoadModule(enginePath.c_str(), cse_platform::ModuleFallback::None);
    assert(image);
    auto factory = reinterpret_cast<void* (*)(const char*, int*)>(cse_platform::GetSymbol(image, "CreateInterface"));
    assert(factory);
    auto* backend = static_cast<IEngineSurface*>(factory("EngineSurface007", nullptr));
    assert(backend);
    cse_vgui1_host::ImageOwner owner;
    const cse_vgui1_host::AdmittedImageV1 admitted{sizeof(admitted), cse_vgui1_host::AdmittedImageVersion,
        1, image, enginePath.c_str()};
    assert(owner.Open(admitted, cse_vgui1_host::AnchorMode::AdoptInitialReference, image)
        == cse_vgui1_host::Result::Ready);
    assert(owner.EnterRoot() == cse_vgui1_host::Result::Ready);
    const cse::native_runtime::BindingV1 binding{sizeof(binding), cse::native_runtime::BindingVersion,
        1, enginePath.c_str(), fontPath.c_str(), owner.Contract()};
    assert(CseNativeRuntimeOpenV1(&binding) == cse::native_runtime::Ready);
    assert(RendererCreated() == Result::Ready);
    assert(BindEmbeddedFiles(nullptr, 0, nullptr, 0) == Result::Ready);
    // The adapter uses the wrapper only as an opaque identity. No fake surface
    // methods or replacement font/raster implementation participate here.
    alignas(void*) unsigned char surfaceIdentity[sizeof(void*)]{};
    auto* key = reinterpret_cast<EngineSurfaceWrap*>(surfaceIdentity);
    assert(AttachEmbedded(key, backend) == Result::Ready);
    vgui::Font font("Helvetica", 16, 0, 0, 400, false, false, false, false);
    assert(SelectEmbedded(key, &font) == Result::Ready);
    constexpr int Width = 64;
    constexpr int Height = 32;
    constexpr int RowPixels = Width + 1;
    std::vector<unsigned short> pixels(RowPixels * Height, 0);
    assert(reinterpret_cast<std::uintptr_t>(pixels.data()) > UINT32_MAX);
    vid.width = Width;
    vid.height = Height;
    vid.rowbytes = RowPixels * sizeof(unsigned short);
    vid.buffer = reinterpret_cast<byte*>(pixels.data());
    int a = 0;
    int b = 0;
    int c = 0;
    font.getCharABCwide('A', a, b, c);
    const int height = font.getTall();
    std::vector<unsigned char> reference(b * height * 4, 0);
    font.getCharRGBA('A', 0, 0, b, height, reference.data());
    assert(std::count(reference.begin(), reference.end(), 0) > 0);
    assert(std::count_if(reference.begin(), reference.end(), [](unsigned char value) { return value != 0; }) > 0);
    std::size_t transparentRgb = 0;
    std::size_t covered = 0;
    for (std::size_t offset = 0; offset < reference.size(); offset += 4) {
        transparentRgb += reference[offset + 3] == 0
            && (reference[offset] || reference[offset + 1] || reference[offset + 2]);
        covered += reference[offset + 3] != 0;
    }
    std::printf("VGUI1 actual LCD glyph alpha-zero-RGB=%zu covered=%zu\n", transparentRgb, covered);
    constexpr int PenX = 3;
    constexpr int PenY = 2;
    for (int format : {0, 1}) {
        is15bit = format;
        std::fill(pixels.begin(), pixels.end(), 0);
        backend->drawSetTextPos(PenX, PenY);
        backend->drawSetTextColor(0, 255, 0, 0);
        assert(DrawEmbedded(key, "A", 1) == Result::Ready);
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < b; ++column) {
                // Coverage is alpha, independently of hidden RGB left by LCD
                // transfer/rounding. Fully transparent texels must not draw.
                const unsigned char coverage = reference[(row * b + column) * 4 + 3];
                const unsigned short expected = coverage ? (format ? 0x03e0 : 0x07e0) : 0;
                assert(pixels[(row + PenY) * RowPixels + PenX + a + column] == expected);
            }
        }
        int penX = 0;
        int penY = 0;
        backend->drawGetTextPos(penX, penY);
        assert(penX == PenX + a + b + c && penY == PenY);
    }
    {
        Scope paint(EmbeddedRuntime());
        assert(paint.Status() == Result::Ready);
        assert(DetachEmbedded(key) == Result::Busy);
        assert(RendererWillDestroy() == Result::Busy);
        assert(CseNativeRuntimeCloseV1() != cse::native_runtime::Closed);
    }
    assert(RendererWillDestroy() == Result::Ready);
    assert(DrawEmbedded(key, "A", 1) != Result::Ready);
    assert(RendererCreated() == Result::Ready);
    assert(SelectEmbedded(key, &font) == Result::Ready);
    assert(DetachEmbedded(key) == Result::Ready);
    assert(RendererWillDestroy() == Result::Ready);
    vid.buffer = nullptr;
    assert(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);
    assert(owner.LeaveRoot() == cse_vgui1_host::Result::Ready);
    assert(owner.Close() == cse_vgui1_host::Result::Closed);
    std::puts("PASS actual SW atlas, real Helvetica glyph bytes, 555/565, pen advance, quiescence and recreation");
}
