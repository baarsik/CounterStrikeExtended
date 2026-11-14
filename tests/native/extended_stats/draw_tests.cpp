#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if __has_include("hud_stat_numbers.h")
#include "hud_stat_numbers.h"
struct Rect {
    int sprite;
};
struct Draw {
    int digit;
    int x;
};
static std::vector<Draw> draws;
static void Check(bool condition, const char* why) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
class CHud {
  public:
    int m_HUD_number_0 = 0;
    Rect rects[10] = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}};
    int GetSpriteWidth(int) {
        return 20;
    }
    int GetSprite(int i) {
        Check(i >= 0 && i <= 9, "sprite index remains a decimal digit");
        return i;
    }
    Rect& GetSpriteRect(int i) {
        Check(i >= 0 && i <= 9, "rectangle index remains a decimal digit");
        return rects[i];
    }
    int GetHudStatNumberWidth(std::int32_t amount);
    int DrawHudStatNumber(int x, int y, std::int32_t amount, int r, int g, int b);
};
static void SPR_Set(int, int, int, int) {
}
static void SPR_DrawAdditive(int, int x, int, const Rect* rect) {
    draws.push_back({rect->sprite, x});
}
#include "draw_bodies.inc"
int main(int, char** argv) {
    CHud hud;
    if (std::strcmp(argv[1], "layout") == 0) {
        Check(cse::extstats::ArmorAnchor(128, 24, 20, 100) == 128, "ordinary anchor preserved");
        Check(cse::extstats::ArmorAnchor(128, 24, 20, 99999) == 154, "five digits leave a full glyph gap");
        Check(hud.GetHudStatNumberWidth(0) == 60 && hud.GetHudStatNumberWidth(1000) == 80, "reserved positions");
    } else {
        const int amounts[] = {0, 1, 255, 256, 999, 1000, 65536, 99999, 100000, 2147483647};
        for (const int amount : amounts) {
            draws.clear();
            const int right = hud.DrawHudStatNumber(10, 0, amount, 255, 160, 0);
            Check(right == 10 + hud.GetHudStatNumberWidth(amount), "draw advance matches measured width");
            Check(!draws.empty() && draws.size() <= 5, "one through five digits drawn");
            int reconstructed = 0;
            for (const Draw& draw : draws) {
                reconstructed = reconstructed * 10 + draw.digit;
            }
            Check(reconstructed == (amount > 99999 ? 99999 : amount), "displayed decimal amount");
            if (amount == 0) {
                Check(draws[0].x == 50, "zero retains three-digit alignment");
            }
        }
    }
    return 0;
}
#else
int main() {
    std::fputs("FAIL: bounded stat rendering missing\n", stderr);
    return 1;
}
#endif
