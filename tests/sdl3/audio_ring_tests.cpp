#include "../../GoldSrc-SDK/engine/sdl3_audio_ring.h"
#include <algorithm>
#include <cassert>
#include <vector>

int main() {
    cse::AudioRing ring;
    const int capacity = cse::AudioRing::BufferBytes;
    constexpr unsigned char pattern = 0x5a;
    std::fill(ring.Data(), ring.Data() + capacity, pattern);
    std::vector<unsigned char> output;
    const auto append = [&output](const unsigned char *data, int bytes) {
        output.insert(output.end(), data, data + bytes);
        return true;
    };
    assert(ring.Drain(6, append));
    assert(output.size() == 6 && output[0] == 0 && output[1] == 0x5a);
    assert(ring.MonoSamplePosition() == 3);
    assert(ring.Drain(capacity - 8, append));
    assert(ring.MonoSamplePosition() == (capacity - 2) / 2);
    output.clear();
    assert(ring.Drain(capacity * 2 + 7, append));
    assert(output.size() == static_cast<unsigned int>(capacity * 2 + 7));
    assert(output[0] == 0x5a && output[1] == 0x5a);
    assert(output[2] == 0 && output[capacity + 2] == 0 && output[capacity * 2 + 2] == 0);
    assert(ring.MonoSamplePosition() == 2);
    const int position = ring.MonoSamplePosition();
    assert(!ring.Drain(8, [](const unsigned char *, int) { return false; }));
    assert(ring.MonoSamplePosition() == position);
    assert(ring.Drain(0, append));
    assert(ring.Drain(-1, append));
    ring.Reset();
    assert(ring.MonoSamplePosition() == 0);
    assert(std::all_of(ring.Data(), ring.Data() + capacity, [](unsigned char byte) { return byte == 0; }));
}
