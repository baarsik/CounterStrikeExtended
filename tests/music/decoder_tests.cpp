#include "../../GoldSrc-SDK/engine/mpg123_decoder.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); return EXIT_FAILURE; \
} } while (false)

int main(int argc, char **argv) {
    CHECK(argc == 3);
    cse::Mpg123Decoder decoder;
    CHECK(!decoder.Open("cse-missing-track.mp3"));
    CHECK(!decoder.Open(""));
    CHECK(!decoder.Open(nullptr));
    CHECK(!decoder.Open(argv[2]));
    CHECK(decoder.Open(argv[1]));
    const auto format = decoder.Format();
    CHECK(format.freq > 0);
    CHECK(format.channels == 1 || format.channels == 2);
    CHECK(format.format == SDL_AUDIO_S16);

    // A small output buffer forces a partial final read. Reading it again with
    // a large buffer must produce exactly the same complete PCM, including EOF.
    std::vector<unsigned char> smallReads;
    std::array<unsigned char, 28> small{};
    bool ended = false;
    for (int count = 0; count < 1000000 && !ended; ++count) {
        const auto result = decoder.Read(small.data(), small.size());
        CHECK(!result.error);
        smallReads.insert(smallReads.end(), small.begin(), small.begin() + result.bytes);
        ended = result.end;
    }
    CHECK(ended);
    // The upstream sweep Xing/LAME header specifies 40 * 1152 - 576 - 1404
    // = 44100 stereo frames: exactly one second, or 176400 signed-16 PCM bytes.
    CHECK(format.freq == 44100);
    CHECK(format.channels == 2);
    CHECK(smallReads.size() == 176400);
    CHECK(decoder.Rewind());
    std::vector<unsigned char> largeReads;
    std::array<unsigned char, 16384> large{};
    ended = false;
    for (int count = 0; count < 100000 && !ended; ++count) {
        const auto result = decoder.Read(large.data(), large.size());
        CHECK(!result.error);
        largeReads.insert(largeReads.end(), large.begin(), large.begin() + result.bytes);
        ended = result.end;
    }
    CHECK(ended);
    CHECK(smallReads == largeReads);
    CHECK(decoder.Rewind());
    const auto first = decoder.Read(small.data(), small.size());
    CHECK(!first.error);
    CHECK(first.bytes == small.size());
    CHECK(std::equal(small.begin(), small.end(), smallReads.begin()));
    decoder.Close();
    decoder.Close();
    CHECK(decoder.Read(small.data(), small.size()).error);
    std::printf("Decoded %zu complete PCM bytes at %d Hz, %d channels.\n",
                smallReads.size(), format.freq, format.channels);
    return EXIT_SUCCESS;
}
