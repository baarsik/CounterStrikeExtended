#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#if __has_include("cse_extended_stats_protocol.h")
#include "cse_extended_stats_protocol.h"

static void Check(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    using namespace cse::extstats;
    Check(argc == 2, "scenario argument");
    if (std::strcmp(argv[1], "conversion") == 0) {
        const int amounts[] = {0, 1, 255, 256, 511, 512, 999, 1000, 32767, 32768, 65535, 65536, 99999, 100000};
        for (const int amount : amounts) {
            Check(NormalizeHealth(static_cast<float>(amount), amount > 0) == amount, "health not truncated");
            Check(NormalizeArmor(static_cast<float>(amount)) == amount, "armor not truncated");
        }
        Check(NormalizeHealth(0.5f, true) == 1, "fractional living health stays alive");
        Check(NormalizeHealth(99999.0f, false) == 0, "dead state overrides positive health");
        Check(NormalizeHealth(-1.0f, true) == 0, "negative health is zero");
        Check(NormalizeArmor(-1.0f) == 0, "negative armor is zero");
        Check(NormalizeArmor(std::numeric_limits<float>::quiet_NaN()) == 0, "NaN is safe");
        Check(NormalizeArmor(std::numeric_limits<float>::infinity()) == INT32_MAX, "infinity saturates");
        Check(NormalizeArmor(static_cast<float>(INT32_MAX)) == INT32_MAX, "rounded int limit is safe");
        Check(NormalizeHealth(std::numeric_limits<float>::max(), true) == INT32_MAX, "huge health saturates");
        Check(DisplayAmount(100000) == 99999 && DisplayAmount(INT32_MAX) == 99999, "display saturation only");
        Check(DisplayAmount(-1) == 0 && DisplayAmount(1000) == 1000, "display domain");
    } else {
        const unsigned char packet[] = {32, 0x9f, 0x86, 0x01, 0x00};
        int player = 0;
        std::int32_t amount = 0;
        Check(DecodeMessage(packet, sizeof(packet), 32, player, amount), "decode full message");
        Check(player == 32 && amount == 99999, "five-byte little endian payload");
        Check(!DecodeMessage(packet, 4, 32, player, amount), "reject truncated packet");
        Check(!DecodeMessage(packet, 6, 32, player, amount), "reject trailing packet data");
        Check(!DecodeMessage(packet, 5, 31, player, amount), "reject invalid player index");
        Check(!DecodeMessage(nullptr, 5, 32, player, amount), "reject null packet");
        const unsigned char invalidation[] = {1, 255, 255, 255, 255};
        Check(DecodeMessage(invalidation, 5, 32, player, amount) && amount == -1, "invalidation preserved");
        const unsigned char negative[] = {1, 254, 255, 255, 255};
        Check(!DecodeMessage(negative, 5, 32, player, amount), "reject other negative amounts");
        const unsigned char maximum[] = {1, 255, 255, 255, 127};
        Check(DecodeMessage(maximum, 5, 32, player, amount) && amount == INT32_MAX, "full signed domain");
    }
    return 0;
}
#else
int main() {
    std::fputs("FAIL: extended stats wire/conversion contract is not implemented\n", stderr);
    return 1;
}
#endif
