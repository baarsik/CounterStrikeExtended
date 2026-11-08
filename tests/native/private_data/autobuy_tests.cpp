#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class CBasePlayer {
public:
    char m_autoBuyString[256]{};
    void AddAutoBuyData(const char *data);
};

#include "autobuy_header.h"

namespace {
bool CheckAppend(const std::string &initial, const char *data, const std::string &expected) {
    constexpr std::uint32_t GuardValue = UINT32_C(0x4f4f4f4f);
    struct GuardedPlayer {
        std::uint32_t before = GuardValue;
        CBasePlayer player;
        std::uint32_t after = GuardValue;
    } guarded;
    std::memcpy(guarded.player.m_autoBuyString, initial.c_str(), initial.size() + 1);
    guarded.player.AddAutoBuyData(data);
    return guarded.before == GuardValue && guarded.after == GuardValue
        && std::memchr(guarded.player.m_autoBuyString, 0, sizeof(guarded.player.m_autoBuyString))
        && std::strcmp(guarded.player.m_autoBuyString, expected.c_str()) == 0;
}
} // namespace

int main() {
    if (!CheckAppend("", "ak47", "ak47") || !CheckAppend("ak47", "primammo", "ak47 primammo")
        || !CheckAppend(std::string(254, 'a'), "b", std::string(254, 'a') + ' ')
        || !CheckAppend(std::string(255, 'a'), "b", std::string(255, 'a'))
        || !CheckAppend("", std::string(300, 'a').c_str(), std::string(255, 'a'))) {
        std::fputs("Autobuy content, terminator or guard bytes failed\n", stderr);
        return 1;
    }
    std::puts("Autobuy native lengths preserve content and terminator bounds");
    return 0;
}
