#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "../../../GoldSrc-SDK/engine/sdl3_audio_ring.h"
#include "../../../GoldSrc-SDK/engine/vox_word.h"
#include "../strings/high_offset_buffer.h"

int main() {
    cse::AudioRing ring;
    constexpr int Capacity = cse::AudioRing::BufferBytes;
    std::array<unsigned char, Capacity> original{};
    for (std::size_t index = 0; index < original.size(); ++index) {
        original[index] = static_cast<unsigned char>((index * 17 + 57) & UINT8_MAX);
    }
    std::copy(original.begin(), original.end(), ring.Data());
    std::vector<unsigned char> expected(original.begin(), original.end());
    expected.insert(expected.end(), original.begin(), original.end());
    expected.insert(expected.end(), original.begin(), original.begin() + 7);
    std::vector<unsigned char> output;
    const auto sink = [&output](const unsigned char *bytes, int count) {
        output.insert(output.end(), bytes, bytes + count);
        return true;
    };
    if (!ring.Drain(Capacity * 2 + 7, sink, cse::AudioRing::PlaybackPolicy::PreserveSamples)
        || output != expected || ring.MonoSamplePosition() != 3
        || !std::equal(original.begin(), original.end(), ring.Data())) {
        std::fputs("Native effect playback altered or reordered mixer PCM\n", stderr);
        return 1;
    }
    if (ring.Drain(10, [](const unsigned char *, int) { return false; },
            cse::AudioRing::PlaybackPolicy::PreserveSamples)
        || ring.MonoSamplePosition() != 3 || !std::equal(original.begin(), original.end(), ring.Data())) {
        return 1;
    }
    HighOffsetBuffer storage(nullptr);
    if (!storage.get()) {
        return 2;
    }
    voxword_t word{};
    word.sfx = reinterpret_cast<sfx_s *>(storage.get());
    if (reinterpret_cast<void *>(word.sfx) != storage.get()) {
        return 1;
    }
    std::printf("Effect ring preserves exact PCM; native VOX word storage is %zu bytes\n", sizeof(word));
    return 0;
}
