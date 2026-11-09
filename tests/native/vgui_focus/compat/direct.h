#pragma once

// Only the uncalled production KeyValues file-saving path needs this Windows CRT name.
#include <sys/stat.h>
#include <cstdlib>
#include <cwchar>
#define _mkdir(path) mkdir(path, 0700)
#define _snwprintf swprintf

// Wide-string numeric conversion is outside this fixture; fail if accidentally entered.
inline int _wtoi(const wchar_t*) {
    std::abort();
}
