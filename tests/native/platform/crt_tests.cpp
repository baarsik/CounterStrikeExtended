#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "../../../GoldSrc-SDK/public/cse_posix_crt.h"

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
