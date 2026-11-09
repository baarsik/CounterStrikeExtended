#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Controlled OS bindings: this executable never calls the host clipboard or
// clock. Handles stay borrowed inside this fixture's own allocation domain.
namespace reference {
struct Storage {
    std::vector<char> bytes;
};
using HGLOBAL = Storage*;
using HANDLE = Storage*;
constexpr unsigned int GMEM_MOVEABLE = 2;
constexpr unsigned int GMEM_DDESHARE = 8192;
constexpr unsigned int CF_TEXT = 1;
Storage fixtureStorage;
bool lockAllowed = true;
std::uint32_t tick = 0;
std::uint32_t GetTickCount() {
    return tick;
}
bool OpenClipboard(void*) {
    return true;
}
void CloseClipboard() {}
HANDLE GetClipboardData(unsigned int) {
    return &fixtureStorage;
}
std::size_t GlobalSize(HANDLE value) {
    return value->bytes.size();
}
HGLOBAL GlobalAlloc(unsigned int, std::size_t bytes) {
    fixtureStorage.bytes.resize(bytes);
    return &fixtureStorage;
}
void* GlobalLock(HANDLE value) {
    return lockAllowed ? value->bytes.data() : nullptr;
}
void GlobalUnlock(HANDLE) {}
void SetClipboardData(unsigned int, HANDLE) {}
#include "win_reference_methods.inc"
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL reference: %s\n", message);
        std::exit(1);
    }
}
int main() {
    reference::fixtureStorage.bytes = {'A', 0, 'B', 0, 0, 0};
    Require(reference::getClipboardTextCount() == 6, "raw storage capacity");
    for (int offset = 0; offset <= 7; ++offset) {
        for (int capacity = 0; capacity <= 8; ++capacity) {
            unsigned char destination[12];
            std::memset(destination, 0x5a, sizeof(destination));
            const int actual = reference::getClipboardText(offset, reinterpret_cast<char*>(destination + 2), capacity);
            const int expected = offset < 6 && capacity > 0 ? (capacity < 6 - offset ? capacity : 6 - offset) : 0;
            Require(actual == expected, "valid offset/capacity counts");
            for (int index = 0; index < 12; ++index) {
                const unsigned char byte = index >= 2 && index < 2 + expected
                    ? static_cast<unsigned char>(reference::fixtureStorage.bytes[static_cast<std::size_t>(offset + index - 2)])
                    : 0x5a;
                Require(destination[index] == byte, "valid byte copy and guards");
            }
        }
    }
    const char raw[] = {'q', 0, static_cast<char>(0xff)};
    reference::setClipboardText(raw, 3);
    Require(reference::fixtureStorage.bytes == std::vector<char>({'q', 0, static_cast<char>(0xff), 0}),
        "valid write payload/terminator");
    reference::lockAllowed = false;
    char destination[4] = {'x', 'x', 'x', 'x'};
    Require(reference::getClipboardText(0, destination, 4) == 4 && destination[0] == 'x',
        "retain evidence: Win lock failure returns positive uncopied count");
#if defined(_WIN32)
    static_assert(sizeof(long) == 4, "Actual Win32 long conversion context");
    for (std::uint32_t value : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
        reference::tick = value;
        const long long expected = value <= INT32_MAX ? static_cast<long long>(value)
            : static_cast<long long>(value) - 4294967296LL;
        Require(reference::getTimeMillis() == expected, "exact retained GetTickCount signed conversion");
    }
    std::puts("PASS controlled retained Windows bodies: valid clipboard + signed32 clock + lock-failure divergence");
#else
    std::puts("PASS controlled retained clipboard bodies; Windows long clock conversion NOT executed on LP64");
#endif
    return 0;
}
