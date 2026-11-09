#include "NativeKeyState.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <type_traits>
namespace keys = cse_vgui2_input;
static_assert(std::is_standard_layout<keys::Snapshot>::value && std::is_trivial<keys::Snapshot>::value,
    "Snapshot has no external ownership or dynamic construction");
namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
}
int main() {
    for (unsigned int bits = 0; bits < 4096; ++bits) {
        keys::Snapshot snapshot{};
        snapshot.capsLockDown = (bits & 1U) != 0;
        snapshot.numLockDown = (bits & 2U) != 0;
        snapshot.scrollLockDown = (bits & 4U) != 0;
        snapshot.leftShiftDown = (bits & 8U) != 0;
        snapshot.rightShiftDown = (bits & 16U) != 0;
        snapshot.leftControlDown = (bits & 32U) != 0;
        snapshot.rightControlDown = (bits & 64U) != 0;
        snapshot.leftAltDown = (bits & 128U) != 0;
        snapshot.rightAltDown = (bits & 256U) != 0;
        snapshot.capsLockToggled = (bits & 512U) != 0;
        snapshot.numLockToggled = (bits & 1024U) != 0;
        snapshot.scrollLockToggled = (bits & 2048U) != 0;
        // Numeric values are independently pinned to the actual Windows SDK in
        // the MSVC probe; these bit positions are the fixed 16-bit API protocol.
        const int codes[] = {0x14, 0x90, 0x91, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0x12, 0x10, 0x11};
        const bool down[] = {snapshot.capsLockDown, snapshot.numLockDown, snapshot.scrollLockDown,
            snapshot.leftShiftDown, snapshot.rightShiftDown, snapshot.leftControlDown, snapshot.rightControlDown,
            snapshot.leftAltDown, snapshot.rightAltDown, snapshot.leftAltDown || snapshot.rightAltDown,
            snapshot.leftShiftDown || snapshot.rightShiftDown, snapshot.leftControlDown || snapshot.rightControlDown};
        const bool toggled[] = {snapshot.capsLockToggled, snapshot.numLockToggled, snapshot.scrollLockToggled};
        for (int index = 0; index < 12; ++index) {
            std::uint16_t word = 0x55aa;
            Require(keys::TryGetKeyState(snapshot, codes[index], word), "all 12 private VK codes are recognized");
            const unsigned int expected = (down[index] ? 0x8000U : 0U) | (index < 3 && toggled[index] ? 1U : 0U);
            Require(word == expected, "low toggle/high down word matches independent packed-bit oracle");
        }
        for (int unknown : {-1, 0, 1, 0x13, 0xa6, 0x7fffffff}) {
            std::uint16_t word = 0x55aa;
            Require(!keys::TryGetKeyState(snapshot, unknown, word) && word == 0x55aa,
                "unknown private keys refuse without altering output");
        }
    }
    std::puts("PASS 49152 known packed states +24576 unknown-state output-preservation cases");
}
