#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <limits>
#include "../../../src/public/cse_posix_crt.h"

int Format(char *destination, std::size_t capacity, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = _vsnprintf(destination, capacity, format, arguments);
    va_end(arguments);
    return result;
}

int main() {
    if (_stricmp("CounterStrike", "COUNTERSTRIKE") != 0 || stricmp("Half-Life", "half-life") != 0
        || _strnicmp("models/player", "MODELS/weapon", 7) != 0
        || strnicmp("sound", "SOUNDtrack", 5) != 0 || _stricmp("amxx", "rehlds") >= 0) {
        return 1;
    }
    const char* originals[] = {"", "CounterStrike", "\xc4\x85"};
    for (const char* original : originals) {
        char* copy = _strdup(original);
        if (!copy || std::strcmp(copy, original) != 0) {
            std::free(copy);
            return 1;
        }
        if (*copy) {
            copy[0] = 'X';
            if (copy[0] == original[0]) {
                std::free(copy);
                return 1;
            }
        }
        std::free(copy);
    }
    if (_atoi64("76561198000000001") != INT64_C(76561198000000001)
        || _atoi64("-9223372036854775808") != (std::numeric_limits<std::int64_t>::min)()
        || _atoi64("92233720368547758080") != (std::numeric_limits<std::int64_t>::max)()
        || _wtoi(L"2147483648") != INT_MAX
        || _wtoi(L"-2147483649") != INT_MIN
        || _wtoi(L"\u2003-\uff11\uff12tail") != -12) {
        return 1;
    }
    constexpr std::size_t MessageCapacity = 32;
    char message[MessageCapacity]{};
    constexpr int MaximumPlayerId = 2147483647;
    if (_snprintf(message, sizeof(message), "%d", MaximumPlayerId) != 10
        || std::strcmp(message, "2147483647") != 0
        || Format(message, sizeof(message), "%s:%d", "menu", 531) != 8
        || std::strcmp(message, "menu:531") != 0) {
        return 1;
    }
    return 0;
}
