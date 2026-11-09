#include <cstdio>

void OutputDebugStringA(const char* text) {
    std::fputs(text, stderr);
}
