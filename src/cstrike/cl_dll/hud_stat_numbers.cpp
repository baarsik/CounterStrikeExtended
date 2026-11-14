#include "hud_stat_numbers.h"
#include "hud.h"
#include "cl_util.h"

int CHud::GetHudStatNumberWidth(std::int32_t amount) {
    return cse::extstats::StatPositions(amount) * GetSpriteWidth(m_HUD_number_0);
}

int CHud::DrawHudStatNumber(int x, int y, std::int32_t amount, int r, int g, int b) {
    const int displayed = cse::extstats::DisplayAmount(amount);
    const int positions = cse::extstats::StatPositions(amount);
    const int width = GetSpriteWidth(m_HUD_number_0);
    constexpr int DecimalBase = 10;
    int divisor = 1;
    for (int position = 1; position < positions; ++position) {
        divisor *= DecimalBase;
    }
    bool started = false;
    for (int position = 0; position < positions; ++position) {
        const int digit = displayed / divisor % DecimalBase;
        if (digit != 0 || started || divisor == 1) {
            const int sprite = m_HUD_number_0 + digit;
            SPR_Set(GetSprite(sprite), r, g, b);
            SPR_DrawAdditive(0, x, y, &GetSpriteRect(sprite));
            started = true;
        }
        x += width;
        divisor /= DecimalBase;
    }
    return x;
}
