#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../strings/high_offset_buffer.h"
#if defined(_WIN32)
#include <mmsystem.h>
#include <dsound.h>
#else
using DWORD = std::uint32_t;
using HRESULT = std::int32_t;
// Test-only result categories. The transfer methods compare equality, never numeric HRESULT values.
constexpr HRESULT DS_OK = 0;
constexpr HRESULT DSERR_BUFFERLOST = 1;
#endif

namespace {
constexpr int FixtureStereoChannels = 2;
constexpr int RingFrames = 16;
constexpr int MonoSamples = RingFrames * FixtureStereoChannels;
constexpr int RingBytes = MonoSamples * sizeof(std::int16_t);
constexpr int SOUND_SAMPLE_MIN = INT16_MIN;
constexpr int SOUND_SAMPLE_MAX = INT16_MAX;
constexpr std::int16_t UntouchedSample = 0x5555;
#if !CSE_SOUND_USE_SDL
constexpr int LegacyLockRetryLimit = 10000;
#endif
constexpr HRESULT UnexpectedLockFailure = static_cast<HRESULT>(-1);

struct SamplePair {
    int left;
    int right;
};

struct DmaView {
    int samples = MonoSamples;
    unsigned char *buffer = nullptr;
};

struct FakeDirectSound {
    void *data = nullptr;
    int lostAttempts = 0;
    bool unexpectedFailure = false;
    int locks = 0;
    int unlocks = 0;

    HRESULT Lock(DWORD offset, DWORD bytes, void **first, DWORD *firstBytes,
                 void **second, DWORD *secondBytes, DWORD flags) {
        ++locks;
        if (offset != 0 || bytes != RingBytes || flags != 0 || unexpectedFailure) {
            return UnexpectedLockFailure;
        }
        if (locks <= lostAttempts) {
            return DSERR_BUFFERLOST;
        }
        *first = data;
        *firstBytes = RingBytes;
        if (second) {
            *second = nullptr;
        }
        if (secondBytes) {
            *secondBytes = 0;
        }
        return DS_OK;
    }

    HRESULT Unlock(void *first, DWORD bytes, void *second, DWORD secondBytes) {
        if (first != data || bytes != RingBytes || second || secondBytes != 0) {
            return UnexpectedLockFailure;
        }
        ++unlocks;
        return DS_OK;
    }
};

SamplePair paintbuffer[8]{};
FakeDirectSound *pDSBuf = nullptr;
#if !CSE_SOUND_USE_SDL
DWORD gSndBufSize = RingBytes;
#endif
DmaView dma;
volatile DmaView *shm = &dma;
struct Volume {
    float value = 1.0f;
} volume;
int paintedtime = 0;
int *snd_p = nullptr;
short *snd_out = nullptr;
int snd_vol = 0;
int snd_linear_count = 0;
std::string diagnostic;
std::vector<std::string> lifecycle;

void Con_Printf(const char *format, ...) {
    std::array<char, 256> buffer{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
    va_end(arguments);
    diagnostic += buffer.data();
}

void S_Shutdown() {
    lifecycle.emplace_back("shutdown");
}

void S_Startup() {
    lifecycle.emplace_back("startup");
}

#if CSE_SOUND_USE_SDL
int nativeLocks = 0;
int nativeUnlocks = 0;
bool failNativeLock = false;

bool SNDDMA_LockBuffer() {
    ++nativeLocks;
    return !failNativeLock;
}

void SNDDMA_UnlockBuffer() {
    ++nativeUnlocks;
}
#endif

#if defined(_MSC_VER)
#pragma warning(push)
// Match the original transfer function's scoped suppression in snd_mix.cpp.
#pragma warning(disable: 4701)
#endif
#include "transfer_header.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Sound transfer check failed at line %d: %s\n", __LINE__, #condition); \
        return false; \
    } \
} while (false)

bool CheckTransfer(std::int16_t *buffer, bool hardware) {
    const SamplePair samples[] = {
        {-40000, 40000}, {-32768, 32767}, {-1, 1}, {0, 0},
        {12345, -12345}, {32768, -32769}, {256, -256}, {100, 200}
    };
    std::copy(std::begin(samples), std::end(samples), std::begin(paintbuffer));
    std::fill(buffer, buffer + MonoSamples, UntouchedSample);
    dma.buffer = reinterpret_cast<unsigned char *>(buffer);
    FakeDirectSound device;
    device.data = buffer;
    pDSBuf = hardware ? &device : nullptr;
    paintedtime = 6;
    volume.value = 1.0f;
#if CSE_SOUND_USE_SDL
    nativeLocks = 0;
    nativeUnlocks = 0;
#endif
    S_TransferStereo16(10);
    constexpr std::array<std::int16_t, MonoSamples> expected = {
        12345, -12345, 32767, -32768, 256, -256, 100, 200,
        UntouchedSample, UntouchedSample, UntouchedSample, UntouchedSample,
        UntouchedSample, UntouchedSample, UntouchedSample, UntouchedSample,
        UntouchedSample, UntouchedSample, UntouchedSample, UntouchedSample,
        UntouchedSample, UntouchedSample, UntouchedSample, UntouchedSample,
        -32768, 32767, -32768, 32767, -1, 1, 0, 0
    };
    CHECK(std::equal(expected.begin(), expected.end(), buffer));
#if CSE_SOUND_USE_SDL
    CHECK(nativeLocks == 1 && nativeUnlocks == 1 && device.locks == 0 && device.unlocks == 0);
#else
    CHECK(device.locks == (hardware ? 1 : 0) && device.unlocks == (hardware ? 1 : 0));
#endif
    CHECK(diagnostic.empty() && lifecycle.empty());
    return true;
}

bool CheckVolume(std::int16_t *buffer) {
    std::fill(buffer, buffer + MonoSamples, UntouchedSample);
    pDSBuf = nullptr;
    paintedtime = 0;
    volume.value = 0.5f;
    S_TransferStereo16(4);
    constexpr std::array<std::int16_t, 16> expected = {
        -20000, 20000, -16384, 16383, -1, 0, 0, 0,
        6172, -6173, 16384, -16385, 128, -128, 50, 100
    };
    CHECK(std::equal(expected.begin(), expected.end(), buffer));
    CHECK(std::all_of(buffer + expected.size(), buffer + MonoSamples,
        [](std::int16_t sample) { return sample == UntouchedSample; }));
    volume.value = 0.0f;
    S_TransferStereo16(4);
    CHECK(std::all_of(buffer, buffer + expected.size(), [](std::int16_t sample) { return sample == 0; }));
    return true;
}

bool CheckFailures(std::int16_t *buffer) {
    paintedtime = 6;
    volume.value = 1.0f;
#if CSE_SOUND_USE_SDL
    (void)buffer;
    nativeLocks = 0;
    nativeUnlocks = 0;
    failNativeLock = true;
    S_TransferStereo16(10);
    CHECK(nativeLocks == 1 && nativeUnlocks == 0);
    CHECK(lifecycle == std::vector<std::string>({"shutdown", "startup"}));
    CHECK(diagnostic == "S_TransferStereo16: SDL::Lock Sound Buffer Failed\n");
    failNativeLock = false;
#else
    FakeDirectSound device;
    device.data = buffer;
    pDSBuf = &device;
    device.lostAttempts = 3;
    S_TransferStereo16(10);
    CHECK(device.locks == 4 && device.unlocks == 1 && lifecycle.empty());
    device = FakeDirectSound{};
    device.data = buffer;
    device.unexpectedFailure = true;
    S_TransferStereo16(10);
    CHECK(device.locks == 1 && device.unlocks == 0);
    CHECK(lifecycle == std::vector<std::string>({"shutdown", "startup"}));
    CHECK(diagnostic == "S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
    diagnostic.clear();
    lifecycle.clear();
    device = FakeDirectSound{};
    device.data = buffer;
    device.lostAttempts = LegacyLockRetryLimit + 1;
    S_TransferStereo16(10);
    CHECK(device.locks == LegacyLockRetryLimit + 1 && device.unlocks == 0);
    CHECK(lifecycle == std::vector<std::string>({"shutdown", "startup"}));
    CHECK(diagnostic == "S_TransferStereo16: DS: couldn't restore buffer\n");
    diagnostic.clear();
    lifecycle.clear();
    pDSBuf = nullptr;
#endif
    diagnostic.clear();
    lifecycle.clear();
    return true;
}
} // namespace

int main(int argc, char **argv) {
    HighOffsetBuffer storage(nullptr);
    if (argc != 2 || !storage.get()) {
        return 2;
    }
    auto *buffer = reinterpret_cast<std::int16_t *>(storage.get());
    if (!CheckTransfer(buffer, false) || !CheckTransfer(buffer, true) || !CheckVolume(buffer) || !CheckFailures(buffer)) {
        return 1;
    }
    if (!CheckTransfer(buffer, false)) {
        return 1;
    }
    auto *file = std::fopen(argv[1], "wb");
    if (!file) {
        return 2;
    }
    const auto bytes = std::fwrite(buffer, 1, RingBytes, file);
    const int closed = std::fclose(file);
    if (bytes != RingBytes || closed != 0) {
        return 2;
    }
    std::puts("Sound transfer preserves clipped stereo PCM, ring wrapping, high addresses and failure lifecycle");
    return 0;
}
