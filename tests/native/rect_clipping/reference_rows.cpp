#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include "vid.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include "cse_rect.h"
#endif

struct TCoordRect {
    float s0;
    float t0;
    float s1;
    float t1;
};
extern bool g_bScissor;
extern RECT g_ScissorRect;
extern RECT window_rect;
extern int bDoScaledFBO;
extern int windowedResult;
extern int windowedCalls;
void ImportedScreenDimensions(int&, int&);
bool SurfaceIntersectRect(RECT*, const RECT*, const RECT*);
bool ScissorRect_TCoords(int, int, int, int, float, float, float, float, RECT*, TCoordRect*);
bool ScissorRect(int, int, int, int, RECT*);
float RectCoordinateDifference(
#if defined(_WIN32)
    LONG, LONG
#else
    std::int32_t, std::int32_t
#endif
);
namespace {
std::uint32_t Bits(float value) {
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
std::uint32_t Next(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
std::int32_t Signed(std::uint32_t value) {
    constexpr auto Maximum = (std::numeric_limits<std::int32_t>::max)();
    if (value <= static_cast<std::uint32_t>(Maximum)) {
        return static_cast<std::int32_t>(value);
    }
    return -1 - static_cast<std::int32_t>((std::numeric_limits<std::uint32_t>::max)() - value);
}
void Rect(const RECT& rectangle) {
    std::printf("[%d,%d,%d,%d]", static_cast<int>(rectangle.left), static_cast<int>(rectangle.top),
        static_cast<int>(rectangle.right), static_cast<int>(rectangle.bottom));
}
void Intersect(const RECT& first, const RECT& second, std::uint32_t index) {
    for (int alias = 0; alias < 3; ++alias) {
        RECT a = first;
        RECT b = second;
        RECT output{123, 456, 789, 1011};
        RECT* destination = alias == 1 ? &a : alias == 2 ? &b : &output;
        const bool result = SurfaceIntersectRect(destination, &a, &b);
        std::printf("{\"kind\":\"intersection\",\"case\":%u,\"alias\":%d,\"a\":", index, alias);
        Rect(first);
        std::printf(",\"b\":");
        Rect(second);
        std::printf(",\"result\":%d,\"out\":", result ? 1 : 0);
        Rect(*destination);
        std::puts("}");
    }
}
void Scissor(const RECT& source, const RECT& clip, std::uint32_t index) {
    for (int enabled = 0; enabled < 2; ++enabled) {
        for (int alias = 0; alias < 2; ++alias) {
            for (int coordinates = 0; coordinates < 2; ++coordinates) {
                g_bScissor = enabled != 0;
                g_ScissorRect = clip;
                RECT output{123, 456, 789, 1011};
                RECT* destination = alias ? &g_ScissorRect : &output;
                TCoordRect uv{-9.5f, -8.25f, 7.125f, 6.0625f};
                const bool result = ScissorRect_TCoords(source.left, source.top, source.right, source.bottom,
                    -0.25f, 0.125f, 0.75f, 1.125f, destination, coordinates ? &uv : nullptr);
                std::printf("{\"kind\":\"uv\",\"case\":%u,\"enabled\":%d,\"alias\":%d,\"uvptr\":%d,\"result\":%d,\"out\":",
                    index, enabled, alias, coordinates, result ? 1 : 0);
                Rect(*destination);
                std::printf(",\"uvbits\":[%u,%u,%u,%u]}\n", Bits(uv.s0), Bits(uv.t0), Bits(uv.s1), Bits(uv.t1));
            }
            g_bScissor = enabled != 0;
            g_ScissorRect = clip;
            RECT output{123, 456, 789, 1011};
            RECT* destination = alias ? &g_ScissorRect : &output;
            const bool result = ScissorRect(source.left, source.top, source.right, source.bottom, destination);
            std::printf("{\"kind\":\"scissor\",\"case\":%u,\"enabled\":%d,\"alias\":%d,\"result\":%d,\"out\":",
                index, enabled, alias, result ? 1 : 0);
            Rect(*destination);
            std::puts("}");
        }
    }
}
}
int main() {
    constexpr std::int32_t Minimum = (std::numeric_limits<std::int32_t>::min)();
    constexpr std::int32_t Maximum = (std::numeric_limits<std::int32_t>::max)();
    constexpr RECT Cases[] = {{0, 0, 10, 10}, {1, 1, 9, 9}, {5, 1, 5, 9}, {1, 5, 9, 5}, {5, 5, 5, 5},
        {6, 1, 4, 9}, {1, 6, 9, 4}, {9, 9, 1, 1}, {10, 0, 20, 10}, {0, 10, 10, 20},
        {-10, -10, 0, 0}, {-5, -5, 5, 5}, {Minimum, Minimum, Maximum, Maximum},
        {Minimum + 1, Minimum + 1, Maximum - 1, Maximum - 1}, {Minimum, 0, 0, Maximum},
        {0, Minimum, Maximum, 0}, {Maximum - 1, Maximum - 1, Maximum, Maximum},
        {Minimum, Minimum, Minimum + 1, Minimum + 1}};
    std::uint32_t index = 0;
    for (const auto& first : Cases) {
        for (const auto& second : Cases) {
            Intersect(first, second, index);
            Scissor(first, second, index);
            ++index;
        }
    }
    std::uint32_t state = 0x19c06b25U;
    for (int random = 0; random < 256; ++random) {
        const RECT first{Signed(Next(state)), Signed(Next(state)), Signed(Next(state)), Signed(Next(state))};
        const RECT second{Signed(Next(state)), Signed(Next(state)), Signed(Next(state)), Signed(Next(state))};
        Intersect(first, second, index);
        Scissor(first, second, index);
        ++index;
    }
    constexpr std::int32_t Positions[] = {Minimum, Minimum + 1, -16777217, -1, 0, 1, 16777217, Maximum - 1, Maximum};
    for (auto position : Positions) {
        for (auto origin : Positions) {
            const auto difference = RectCoordinateDifference(position, origin);
            std::printf("{\"kind\":\"difference\",\"position\":%d,\"origin\":%d,\"bits\":%u}\n",
                position, origin, Bits(difference));
            for (int mode = 0; mode < 4; ++mode) {
                window_rect = {origin, position, position, origin};
                bDoScaledFBO = mode & 1;
                windowedResult = (mode & 2) != 0;
                windowedCalls = 0;
                vid.width = 640;
                vid.height = 480;
                int width = 0;
                int height = 0;
                ImportedScreenDimensions(width, height);
                std::printf("{\"kind\":\"screen\",\"position\":%d,\"origin\":%d,\"mode\":%d,\"width\":%d,\"height\":%d,\"calls\":%d}\n",
                    position, origin, mode, width, height, windowedCalls);
            }
        }
    }
    return 0;
}
