#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>

class Formatter {
public:
    void FileTimeToString(char*, int, long);
};
namespace {
int calls = 0;
char* NullCtime(const time_t*) {
    ++calls;
    errno = EIO;
    return nullptr;
}
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}
}
#define ctime NullCtime
#include "actual_formatter.h"
#undef ctime

int main() {
    Formatter formatter;
    std::array<char, 8> output;
    output.fill('Z');
    errno = 0;
    formatter.FileTimeToString(output.data(), static_cast<int>(output.size()), 0);
    Require(calls == 1 && output[0] == 0 && errno == EINVAL, "null ctime maps to explicit empty failure");
    for (std::size_t index = 1; index < output.size(); ++index) {
        Require(output[index] == 'Z', "failure writes only the supported terminator");
    }
    errno = 0;
    formatter.FileTimeToString(nullptr, 1, 0);
    Require(calls == 1 && errno == EINVAL, "null destination never reaches ctime");
    output.fill('Z');
    errno = 0;
    formatter.FileTimeToString(output.data(), 0, 0);
    Require(calls == 1 && errno == EINVAL && output[0] == 'Z', "zero capacity never writes before destination");
    std::puts("actual_formatter_failure_passed=1");
    return 0;
}
