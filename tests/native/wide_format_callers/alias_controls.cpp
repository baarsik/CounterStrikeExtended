#include <cstdarg>
#include <cwchar>
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "cse_posix_crt.h"
}
#ifdef swprintf
#error Standard swprintf must not acquire a global compatibility alias
#endif

int Format(wchar_t* output, std::size_t count, const wchar_t* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = _vsnwprintf(output, count, format, arguments);
    va_end(arguments);
    return result;
}

int main() {
    constexpr std::size_t kCapacity = 4;
    wchar_t output[kCapacity + 1]{L'Z', L'Z', L'Z', L'Z', L'Z'};
    if (_snwprintf(output, kCapacity, L"%s", L"ABCD") != static_cast<int>(kCapacity) || output[kCapacity - 1] != L'D' || output[kCapacity] != L'Z'
        || Format(output, kCapacity, L"%I64d", 4294967297LL) != -1 || output[kCapacity - 1] != L'4' || output[kCapacity] != L'Z') {
        std::abort();
    }
    std::puts("PASS: native direct/va aliases and extern-C include boundary; standard swprintf unaliased");
}
