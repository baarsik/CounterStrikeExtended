#include "VGui1NativeAtlas.h"
#include "IEngineSurface.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "embedded_adapter.inc"

// Typed unrelated lifetime binding. No Atlas is constructed; the actual private
// EmbeddedSurface has a null unique_ptr. Abort if the fixture violates that.
namespace cse_vgui1_native {
Atlas::~Atlas() noexcept {
    std::abort();
}
}
namespace {
class Receiver final : public IEngineSurface {
public:
    int bound = 0;
    int calls = 0;
    std::array<int, 4> arguments{};
    std::array<float, 4> coordinates{};
    void drawSetTexture(int id) override {
        bound = id;
    }
    void drawPrintChar(int x, int y, int width, int height, float s0, float t0, float s1, float t1) override {
        ++calls;
        arguments = {{x, y, width, height}};
        coordinates = {{s0, t0, s1, t1}};
    }
#define UNRELATED(signature) \
    signature override { \
        std::abort(); \
    }
    UNRELATED(void pushMakeCurrent(int*, int*, int*, bool))
    UNRELATED(void popMakeCurrent())
    UNRELATED(void drawFilledRect(int, int, int, int))
    UNRELATED(void drawOutlinedRect(int, int, int, int))
    UNRELATED(void drawLine(int, int, int, int))
    UNRELATED(void drawPolyLine(int*, int*, int))
    UNRELATED(void drawTexturedPolygon(vgui2::VGuiVertex*, int))
    UNRELATED(void drawSetTextureRGBA(int, const char*, int, int, int, int))
    UNRELATED(void drawTexturedRect(int, int, int, int))
    UNRELATED(void drawTexturedRectAdd(int, int, int, int))
    UNRELATED(int createNewTextureID())
    UNRELATED(void drawSetColor(int, int, int, int))
    UNRELATED(void drawSetTextColor(int, int, int, int))
    UNRELATED(void drawSetTextPos(int, int))
    UNRELATED(void drawGetTextPos(int&, int&))
    UNRELATED(void drawPrintCharAdd(int, int, int, int, float, float, float, float))
    UNRELATED(void drawSetTextureFile(int, const char*, int, bool))
    UNRELATED(void drawGetTextureSize(int, int&, int&))
    UNRELATED(int isTextureIdValid(int))
    UNRELATED(void drawSetSubTextureRGBA(int, int, int, const unsigned char*, int, int))
    UNRELATED(void drawFlushText())
    UNRELATED(void resetViewPort())
    UNRELATED(void drawSetTextureBGRA(int, const char*, int, int, int, int))
    UNRELATED(void drawUpdateRegionTextureBGRA(int, int, int, const unsigned char*, int, int))
#undef UNRELATED
};
}
int main() {
    using namespace cse_vgui1_native;
    Receiver receiver;
    EmbeddedSurface context{&receiver, {}};
    const Quad cases[]{
        {0, 0, 9, 20, 71, {0.0f, 0.25f, 0.5f, 0.75f}},
        {13, 3, 9, 20, 72, {0.0f, 0.25f, 0.5f, 0.75f}},
        {-13, -3, 9, 20, 73, {0.0f, 0.25f, 0.5f, 0.75f}},
        {(std::numeric_limits<int>::max)() - 9, (std::numeric_limits<int>::max)() - 20,
            9, 20, 74, {0.0f, 0.25f, 0.5f, 0.75f}},
    };
    bool passed = true;
    for (const auto& quad : cases) {
        Draw(&context, quad);
        const std::array<int, 4> expected{{quad.x, quad.y, quad.wide, quad.tall}};
        const std::array<float, 4> uv{{quad.uv[0], quad.uv[1], quad.uv[2], quad.uv[3]}};
        const bool equal = receiver.arguments == expected && receiver.coordinates == uv && receiver.bound == quad.texture;
        std::printf("origin=%d,%d expected=%d,%d,%d,%d actual=%d,%d,%d,%d equal=%d\n", quad.x, quad.y,
            expected[0], expected[1], expected[2], expected[3], receiver.arguments[0], receiver.arguments[1],
            receiver.arguments[2], receiver.arguments[3], static_cast<int>(equal));
        passed = passed && equal;
    }
    return passed && receiver.calls == 4 ? 0 : 1;
}
