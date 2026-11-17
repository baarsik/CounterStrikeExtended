#include "sound.h"
#include "client.h"
#include "common.h"
#include "conprint.h"
#include "sound_callback.h"
#include "sys.h"
#include "vox_word.h"
#include "zone.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

qboolean GetWavinfo(char *name, byte *wav, int length, wavinfo_t *info);
void ResampleSfx(sfx_t *sound, int inputRate, int inputWidth, byte *data, int dataBytes);
extern "C" void Wavstream_Close(int index);
extern "C" void S_FreeChannel(channel_t *channel);
extern "C" char *VOX_GetDirectory(char *path, char *sentence, int size);
extern "C" int VOX_ParseWordParams(char *word, voxword_t *parameters, int first);
extern "C" sfxcache_t *VOX_LoadSound(channel_t *channel, char *name);
void SND_InitScaletable();
void S_MixChannelsToPaintbuffer(int end, int hires, int voiceOnly);

client_state_t cl{};
channel_t channels[MAX_CHANNELS]{};
wavstream_t wavstreams[MAX_CHANNELS]{};
volatile dma_t *shm = nullptr;
cvar_t hisound{};
cvar_t loadas8bit{};
cvar_t fs_precache_timings{};
int total_channels = 0;
int paintedtime = 0;
float g_SND_VoiceOverdrive = 1.0f;

namespace {
constexpr int PlaybackRate = 22050;
constexpr byte OutputSentinel = 0x5a;
constexpr int RiffLengthOffset = 4;
constexpr int FormatCodecOffset = 20;
constexpr int FormatChannelsOffset = 22;
constexpr int FormatRateOffset = 24;
constexpr int FormatBitsOffset = 34;
constexpr int PcmDataOffset = 44;
constexpr int StreamPrefixBytes = 0x8000;
constexpr int FirstChunkSamples = StreamPrefixBytes - PcmDataOffset;
constexpr int TailSamples = 4;
constexpr int MixVolume = 8;
constexpr int SOUND_VOLUME_LEVELS = 32;
constexpr int SOUND_SAMPLE_VALUES = 256;
constexpr int SOUND_VOLUME_SCALE = 8;
constexpr int MAX_VOX_SENTENCES = 16;
constexpr int MAX_VOX_WORDS = 32;

int snd_scaletable[SOUND_VOLUME_LEVELS][SOUND_SAMPLE_VALUES]{};
portable_samplepair_t paintbuffer[8]{};
voxword_t rgrgvoxword[MAX_VOX_SENTENCES][MAX_VOX_WORDS]{};
char *rgpparseword[MAX_VOX_WORDS]{};
char voxcomma[] = "_comma";
char voxperiod[] = "_period";
char *sentenceText = nullptr;
sfx_t sentenceSound{};
int sentenceLookups = 0;
dma_t playback{};
std::vector<std::unique_ptr<byte[]>> temporaryBuffers;
std::unique_ptr<byte[]> fileBytes;

struct FileState {
    int length = 0;
    int position = 0;
    int opens = 0;
    int closes = 0;
    int releases = 0;
    bool open = false;
    bool borrowed = false;
} file;

bool mappedReads = true;
bool failOpen = false;
bool failCache = false;
bool failTemp = false;
enum class CacheChange { None, Move, Evict };
CacheChange changeOnTempAlloc = CacheChange::None;
sfx_t *cacheUnderPressure = nullptr;
int trackedCacheBytes = 0;

void Require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

void Word(std::vector<byte> &bytes, std::uint16_t value) {
    bytes.push_back(static_cast<byte>(value));
    bytes.push_back(static_cast<byte>(value >> 8));
}

void Dword(std::vector<byte> &bytes, std::uint32_t value) {
    Word(bytes, static_cast<std::uint16_t>(value));
    Word(bytes, static_cast<std::uint16_t>(value >> 16));
}

void SetDword(std::vector<byte> &bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<byte>(value >> (index * CHAR_BIT));
    }
}

void Chunk(std::vector<byte> &bytes, const char *tag, const std::vector<byte> &payload, bool pad = true) {
    bytes.insert(bytes.end(), tag, tag + 4);
    Dword(bytes, static_cast<std::uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    if (pad && (payload.size() & 1)) {
        bytes.push_back(0);
    }
}

std::vector<byte> Wave(int width = 1, int sampleCount = 4, bool loop = false, bool pad = true) {
    std::vector<byte> bytes{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    std::vector<byte> format;
    Word(format, 1);
    Word(format, 1);
    Dword(format, PlaybackRate);
    Dword(format, PlaybackRate * width);
    Word(format, static_cast<std::uint16_t>(width));
    Word(format, static_cast<std::uint16_t>(width * CHAR_BIT));
    Chunk(bytes, "fmt ", format);
    if (loop) {
        std::vector<byte> cue(28);
        SetDword(cue, 0, 1);
        SetDword(cue, 24, 1);
        Chunk(bytes, "cue ", cue);
    }
    Chunk(bytes, "data", std::vector<byte>(sampleCount * width, 128), pad);
    if (loop) {
        std::vector<byte> list(24);
        SetDword(list, 16, 2);
        std::memcpy(list.data() + 20, "mark", 4);
        Chunk(bytes, "LIST", list);
    }
    SetDword(bytes, RiffLengthOffset, static_cast<std::uint32_t>(bytes.size() - 8));
    return bytes;
}

qboolean Parse(const std::vector<byte> &bytes, wavinfo_t &info, bool stream = false) {
    auto input = std::make_unique<byte[]>(std::max<std::size_t>(1, bytes.size()));
    std::copy(bytes.begin(), bytes.end(), input.get());
    char ordinary[] = "fixture.wav";
    char streamed[] = "*fixture.wav";
    return GetWavinfo(stream ? streamed : ordinary, input.get(), static_cast<int>(bytes.size()), &info);
}

void Parser(const std::string &mode) {
    wavinfo_t info{};
    if (mode == "pcm") {
        for (int width : {1, 2}) {
            Require(Parse(Wave(width), info) == TRUE, "complete PCM WAV is accepted");
            Require(info.rate == PlaybackRate && info.width == width && info.channels == 1,
                    "PCM format metadata is preserved");
            Require(info.samples == 4 && info.loopstart == -1 && info.dataofs == PcmDataOffset,
                    "PCM sample count and data offset match the authored file");
        }
    } else if (mode == "truncated") {
        const auto complete = Wave(2);
        for (std::size_t length = 0; length < complete.size(); ++length) {
            Require(Parse(std::vector<byte>(complete.begin(), complete.begin() + length), info) == FALSE,
                    "truncated ordinary WAV must be rejected without reading beyond input");
        }
    } else if (mode == "metadata") {
        for (int offset : {FormatCodecOffset, FormatChannelsOffset, FormatRateOffset, FormatBitsOffset}) {
            auto bytes = Wave();
            bytes[offset] = 0;
            bytes[offset + 1] = 0;
            Require(Parse(bytes, info) == FALSE, "zero PCM format field is rejected");
        }
        auto bytes = Wave();
        bytes[FormatBitsOffset] = 24;
        Require(Parse(bytes, info) == FALSE, "unsupported 24-bit PCM is rejected");
        bytes = Wave();
        SetDword(bytes, RiffLengthOffset, 4);
        Require(Parse(bytes, info) == FALSE, "chunks outside the declared RIFF payload are rejected");
        bytes = Wave();
        SetDword(bytes, FormatRateOffset, UINT32_MAX);
        Require(Parse(bytes, info) == FALSE, "negative sample rate is rejected");
    } else if (mode == "invalid-arguments") {
        auto bytes = Wave();
        char name[] = "fixture.wav";
        Require(GetWavinfo(nullptr, bytes.data(), static_cast<int>(bytes.size()), &info) == FALSE,
                "null sound name is rejected");
        Require(GetWavinfo(name, nullptr, static_cast<int>(bytes.size()), &info) == FALSE,
                "null WAV data is rejected");
        Require(GetWavinfo(name, bytes.data(), static_cast<int>(bytes.size()), nullptr) == FALSE,
                "null output metadata is rejected");
        Require(GetWavinfo(name, bytes.data(), -1, &info) == FALSE, "negative file length is rejected");
    } else if (mode == "chunk-bounds") {
        auto bytes = Wave();
        constexpr int FormatLengthOffset = 16;
        SetDword(bytes, FormatLengthOffset, 15);
        Require(Parse(bytes, info) == FALSE, "fmt payload must contain every PCM format field");
        bytes = Wave(1, 5, true);
        constexpr int CueLengthOffset = 40;
        constexpr int CueSampleOffset = 68;
        SetDword(bytes, CueLengthOffset, 24);
        bytes.erase(bytes.begin() + CueSampleOffset, bytes.begin() + CueSampleOffset + 4);
        SetDword(bytes, RiffLengthOffset, static_cast<std::uint32_t>(bytes.size() - 8));
        Require(Parse(bytes, info) == FALSE, "cue payload must contain the loop-start field");
        bytes = Wave(1, 5, true);
        bytes.pop_back();
        Require(Parse(bytes, info, true) == FALSE, "stream prefix exception does not allow truncated LIST metadata");
    } else if (mode == "loops" || mode == "loop-overflow" || mode == "loop-length") {
        auto bytes = Wave(1, 5, true);
        Require(Parse(bytes, info) == TRUE && info.loopstart == 1 && info.samples == 3,
                "Cool Edit cue and loop length are preserved");
        constexpr int CueSampleOffset = 68;
        constexpr int ListLoopLengthOffset = 110;
        if (mode == "loops") {
            SetDword(bytes, CueSampleOffset, UINT32_MAX);
            Require(Parse(bytes, info) == FALSE, "negative cue sample is rejected");
        } else if (mode == "loop-overflow") {
            SetDword(bytes, ListLoopLengthOffset, INT_MAX);
            Require(Parse(bytes, info) == FALSE, "overflowing loop sum is rejected");
        } else {
            SetDword(bytes, ListLoopLengthOffset, 6);
            Require(Parse(bytes, info) == FALSE, "loop beyond available samples is rejected without a fatal error");
        }
    } else if (mode == "stream-prefix") {
        auto bytes = Wave(1, StreamPrefixBytes * 2);
        bytes.resize(StreamPrefixBytes);
        Require(Parse(bytes, info, true) == TRUE && info.samples == StreamPrefixBytes * 2,
                "streaming metadata accepts an initial partial data chunk");
        Require(Parse(bytes, info) == FALSE, "ordinary loads require the complete declared data payload");
    } else if (mode == "odd-padding") {
        for (bool pad : {false, true}) {
            Require(Parse(Wave(1, 5, true, pad), info) == TRUE && info.samples == 3 && info.loopstart == 1,
                    "padded and legacy unpadded Cool Edit data both retain their loop marker");
        }
    }
}

sfxcache_t *AllocateOutput(sfx_t &sound, int samples, int width, int capacitySamples = 0) {
    const int capacity = capacitySamples ? capacitySamples : samples;
    auto *cache = static_cast<sfxcache_t *>(
        Cache_Alloc(&sound.cache, static_cast<int>(sizeof(sfxcache_t)) + capacity * width, "fixture"));
    Require(cache != nullptr, "fixture cache allocation succeeds");
    cache->length = samples;
    cache->loopstart = -1;
    cache->speed = PlaybackRate;
    cache->width = width;
    std::memset(cache->data, OutputSentinel, capacity * width);
    return cache;
}

void Resampler(const std::string &mode) {
    playback.speed = PlaybackRate;
    shm = &playback;
    sfx_t sound{};
    std::strcpy(sound.name, "fixture.wav");
    if (mode == "resample-valid") {
        std::array<byte, 4> byteInput{0, 128, 255, 64};
        auto* byteCache = AllocateOutput(sound, 4, 1);
        ResampleSfx(&sound, PlaybackRate, 1, byteInput.data(), static_cast<int>(byteInput.size()));
        const byte byteExpected[] = {128, 0, 127, 192};
        Require(std::memcmp(byteCache->data, byteExpected, sizeof(byteExpected)) == 0,
                "8-bit PCM fast path retains its signed output bytes");
        Cache_Free(&sound.cache);
        std::array<short, 4> input{-32768, -1024, 0, 32767};
        for (bool eightBit : {false, true}) {
            loadas8bit.value = eightBit;
            auto *cache = AllocateOutput(sound, 4, 2);
            ResampleSfx(&sound, PlaybackRate, 2, reinterpret_cast<byte *>(input.data()), sizeof(input));
            const byte expected[] = {128, 252, 0, 127};
            Require(eightBit ? std::memcmp(cache->data, expected, sizeof(expected)) == 0
                             : std::memcmp(cache->data, input.data(), sizeof(input)) == 0,
                    "valid 16-bit conversion is preserved");
            Cache_Free(&sound.cache);
        }
        loadas8bit.value = 0;
        auto *cache = AllocateOutput(sound, 4, 2);
        cache->loopstart = 2;
        ResampleSfx(&sound, PlaybackRate * 2, 2, reinterpret_cast<byte *>(input.data()), sizeof(input));
        const short expected[] = {-32768, 0};
        Require(cache->length == 2 && cache->loopstart == 1 && cache->speed == PlaybackRate,
                "downsampling retains the expected rate, length and loop position");
        Require(std::memcmp(cache->data, expected, sizeof(expected)) == 0, "downsampling selects expected samples");
        Cache_Free(&sound.cache);
        hisound.value = 1;
        cache = AllocateOutput(sound, 4, 2);
        cache->speed = PlaybackRate * 2;
        ResampleSfx(&sound, PlaybackRate * 2, 2, reinterpret_cast<byte*>(input.data()), sizeof(input));
        Require(cache->length == 4 && cache->speed == PlaybackRate * 2
                    && std::memcmp(cache->data, input.data(), sizeof(input)) == 0,
                "high-quality playback preserves the original double-rate samples");
    } else if (mode == "resample-fraction") {
        // The last source position fits signed 32-bit, but the final increment does not.
        constexpr int InputBytes = 8388608;
        constexpr int DeclaredSamples = 16777215;
        constexpr int InputRate = 2147483520;
        playback.speed = 256;
        std::vector<byte> input(InputBytes, 128);
        auto *cache = AllocateOutput(sound, DeclaredSamples, 1, 2);
        ResampleSfx(&sound, InputRate, 1, input.data(), InputBytes);
        Require(cache->length == 2 && cache->data[0] == 0 && cache->data[1] == 0,
                "final fixed-point increment cannot overflow signed storage");
    } else {
        int samples = mode == "resample-clear" ? 1 : 4;
        int bytes = mode == "resample-clear" ? 0 : 7;
        int rate = PlaybackRate;
        int capacity = samples;
        if (mode == "resample-product") {
            // Four fixed-point steps of 2^30 wrap a 32-bit product to zero.
            samples = 20971520;
            rate = 1073741824;
            playback.speed = 256;
            capacity = 5;
            bytes = 2;
        }
        auto input = std::make_unique<byte[]>(std::max(1, bytes));
        std::memset(input.get(), 128, std::max(1, bytes));
        auto *cache = AllocateOutput(sound, samples, 2, capacity);
        ResampleSfx(&sound, rate, 2, input.get(), bytes);
        for (int index = 0; index < capacity * 2; ++index) {
            Require(cache->data[index] == 0, "invalid resampling clears every output byte");
        }
    }
    Cache_Free(&sound.cache);
    shm = nullptr;
}

void SetFile(const std::vector<byte> &bytes) {
    file.length = static_cast<int>(bytes.size());
    fileBytes = std::make_unique<byte[]>(std::max<std::size_t>(1, bytes.size()));
    std::copy(bytes.begin(), bytes.end(), fileBytes.get());
}

void RequireClosedStream() {
    const std::array<byte, sizeof(wavstream_t)> empty{};
    Require(!file.open && file.closes == file.opens, "stream handle is closed exactly once per open");
    Require(std::memcmp(&wavstreams[0], empty.data(), empty.size()) == 0, "closed stream state is reset");
}

void Loader(const std::string &mode) {
    playback.speed = PlaybackRate;
    shm = &playback;
    const bool stream = mode.rfind("stream-", 0) == 0;
    sfx_t sound{};
    std::strcpy(sound.name, stream ? "*fixture.wav" : "fixture.wav");
    auto bytes = Wave();
    if (mode == "load-invalid" || mode == "stream-invalid") {
        bytes[FormatCodecOffset] = 3;
    } else if (mode == "load-stereo" || mode == "stream-stereo") {
        bytes[FormatChannelsOffset] = 2;
    } else if (mode == "stream-width") {
        bytes = Wave(2);
    } else if (mode == "stream-rate") {
        SetDword(bytes, FormatRateOffset, PlaybackRate / 2);
    }
    failCache = mode == "load-cache-failure" || mode == "stream-cache-failure";
    failTemp = mode == "load-temp-failure" || mode == "stream-temp-failure";
    mappedReads = mode != "load-fallback" && mode != "load-temp-failure";
    if (stream) {
        cacheUnderPressure = &sound;
    }
    SetFile(bytes);
    auto *cache = S_LoadSound(&sound, &channels[0]);
    const bool success = mode == "load-success" || mode == "load-fallback" || mode == "stream-success" ||
                         mode == "stream-refill-failure";
    Require((cache != nullptr) == success, "loader success matches the file and allocation conditions");
    if (stream) {
        if (success) {
            Require(file.open && file.closes == 0 && wavstreams[0].hFile == &file,
                    "successful stream retains its file handle");
            if (mode == "stream-refill-failure") {
                Cache_Free(&sound.cache);
                failCache = true;
                Require(S_LoadSound(&sound, &channels[0]) == nullptr, "failed refill reports failure");
            } else {
                const int pcmBytes = static_cast<int>(bytes.size()) - PcmDataOffset;
                Require(trackedCacheBytes == static_cast<int>(sizeof(sfxcache_t)) + pcmBytes,
                        "a short stream only caches its PCM payload");
                Require(S_LoadSound(&sound, &channels[0]) == cache && file.opens == 1,
                        "cached stream reuse neither reopens nor closes the handle");
                Wavstream_Close(0);
            }
        }
        RequireClosedStream();
        Require(file.releases == 0, "stream temporary buffers are not filesystem read-buffer leases");
    } else {
        Require(!file.open && !file.borrowed && file.closes == 1, "ordinary load always closes its file once");
        Require(file.releases == (mappedReads ? 1 : 0), "only acquired read buffers are released");
        if (success) {
            Require(cache->length == 4 && cache->width == 1, "successful ordinary load retains PCM metadata");
            Require(S_LoadSound(&sound, &channels[0]) == cache && file.opens == 1,
                    "cached ordinary sound needs no second file lease");
        }
    }
    if (sound.cache.data) {
        Cache_Free(&sound.cache);
    }
    shm = nullptr;
}

void StartFixtureChannel(channel_t &channel, sfx_t &sound, int end, int type) {
    channel.sfx = &sound;
    channel.leftvol = MixVolume;
    channel.rightvol = MixVolume;
    channel.pitch = PITCH_NORM;
    channel.isentence = -1;
    channel.entchannel = type;
    channel.end = end;
}

void StreamCache(const std::string &mode) {
    playback.speed = PlaybackRate;
    shm = &playback;
    sfx_t sound{};
    std::strcpy(sound.name, "*fixture.wav");
    auto bytes = Wave(1, FirstChunkSamples + TailSamples);
    constexpr byte FirstPcmSample = 7;
    bytes[PcmDataOffset] += FirstPcmSample;
    SetFile(bytes);
    cacheUnderPressure = &sound;
    auto *initial = S_LoadSound(&sound, &channels[0]);
    Require(initial && initial->length == FirstChunkSamples, "initial stream caches only the first PCM chunk");
    if (mode == "stream-reload-pcm") {
        wavstreams[0].csamplesplayed = 1;
        Cache_Free(&sound.cache);
    } else {
        Wavstream_Close(0);
        changeOnTempAlloc = mode == "stream-reopen-move" ? CacheChange::Move : CacheChange::Evict;
    }
    auto *cache = S_LoadSound(&sound, &channels[0]);
    Require(cache && cache == sound.cache.data && changeOnTempAlloc == CacheChange::None,
            "stream load returns the current cache after reading the WAV");
    Require(cache->length == FirstChunkSamples && cache->data[0] == FirstPcmSample,
            "restored cache retains the original chunk boundaries and PCM bytes");
    Wavstream_Close(0);
    RequireClosedStream();
    Cache_Free(&sound.cache);
}

void Mixer(const std::string &mode) {
    constexpr int PaintSamples = 3;
    constexpr int HealthyPcmSample = 2;
    constexpr int TailPcmSample = 4;
    playback.speed = PlaybackRate;
    shm = &playback;
    total_channels = 2;
    SND_InitScaletable();

    sfx_t healthy{};
    std::strcpy(healthy.name, "cached.wav");
    auto *healthyCache = AllocateOutput(healthy, PaintSamples + 1, 1);
    std::memset(healthyCache->data, HealthyPcmSample, healthyCache->length);
    StartFixtureChannel(channels[1], healthy, healthyCache->length, CHAN_AUTO);

    sfx_t sound{};
    const bool stream = mode != "mix-reload-failure";
    std::strcpy(sound.name, stream ? "*fixture.wav" : "fixture.wav");
    if (stream) {
        auto bytes = Wave(1, FirstChunkSamples + TailSamples);
        std::fill(bytes.begin() + StreamPrefixBytes, bytes.end(), 128 + TailPcmSample);
        if (mode == "mix-stream-eof") {
            bytes.resize(StreamPrefixBytes);
        }
        SetFile(bytes);
        cacheUnderPressure = &sound;
        auto *cache = S_LoadSound(&sound, &channels[0]);
        Require(cache && cache->length == FirstChunkSamples, "the mixer begins with a valid stream chunk");
        StartFixtureChannel(channels[0], sound, 1, CHAN_STREAM);
        channels[0].pos = FirstChunkSamples - 1;
        wavstreams[0].csamplesplayed = channels[0].pos;
        if (mode == "mix-stream-move") {
            changeOnTempAlloc = CacheChange::Move;
        } else if (mode == "mix-stream-evict") {
            changeOnTempAlloc = CacheChange::Evict;
        } else if (mode == "mix-stream-reload-failure") {
            Cache_Free(&sound.cache);
            failTemp = true;
        } else {
            failTemp = mode == "mix-stream-temp-failure";
        }
    } else {
        StartFixtureChannel(channels[0], sound, PaintSamples + 1, CHAN_AUTO);
        failOpen = true;
    }

    S_MixChannelsToPaintbuffer(PaintSamples, 0, 0);
    Require(channels[1].pos == PaintSamples, "the next channel advances through the same paint pass");
    const bool moved = mode == "mix-stream-move";
    for (int sample = 0; sample < PaintSamples; ++sample) {
        const int expected = MixVolume * (HealthyPcmSample + (moved && sample > 0 ? TailPcmSample : 0));
        Require(paintbuffer[sample].left == expected && paintbuffer[sample].right == expected,
                "the paint buffer contains the healthy channel and any successfully refilled PCM");
    }
    if (moved) {
        auto *cache = static_cast<sfxcache_t *>(sound.cache.data);
        Require(changeOnTempAlloc == CacheChange::None && cache && cache->length == TailSamples,
                "the moved cache holds the final partial chunk");
        Require(channels[0].pos == PaintSamples - 1 && channels[0].end == TailSamples + 1,
                "refill continues from the start of the new chunk");
        S_FreeChannel(&channels[0]);
        RequireClosedStream();
    } else if (stream) {
        Require(channels[0].sfx == nullptr && sound.cache.data == nullptr,
                "failed refill retires the channel and releases any remaining cache");
        RequireClosedStream();
    } else {
        Require(channels[0].sfx == &sound && channels[0].pos == 0, "failed ordinary load may be retried");
        auto bytes = Wave();
        bytes[PcmDataOffset] += TailPcmSample;
        SetFile(bytes);
        failOpen = false;
        channels[1].pos = 0;
        std::fill(std::begin(paintbuffer), std::end(paintbuffer), portable_samplepair_t{});
        S_MixChannelsToPaintbuffer(PaintSamples, 0, 0);
        Require(channels[0].pos == PaintSamples && paintbuffer[0].left == MixVolume * (HealthyPcmSample + TailPcmSample),
                "the skipped channel contributes PCM after a successful retry");
        Cache_Free(&sound.cache);
    }
    Cache_Free(&healthy.cache);
}

void Vox(const std::string &mode) {
    playback.speed = PlaybackRate;
    shm = &playback;
    if (mode == "vox-empty-text") {
        char path[SOUND_NAME_BYTES]{};
        char empty[]{""};
        voxword_t parameters{};
        Require(VOX_GetDirectory(path, empty, sizeof(path)) == nullptr, "empty VOX text has no directory");
        Require(!VOX_ParseWordParams(empty, &parameters, true), "empty VOX token is skipped");
    }

    char name[] = "fixture";
    channel_t channel{};
    channel.isentence = -1;
    char empty[] = "";
    char directoryOnly[] = "vox/";
    char parametersOnly[] = "(p120) (v50)";
    char delimitersOnly[] = "   ";
    for (char *text : {parametersOnly, directoryOnly, delimitersOnly, empty}) {
        sentenceText = text;
        Require(VOX_LoadSound(&channel, name) == nullptr && channel.isentence == -1 && channel.sfx == nullptr,
                "VOX with no words leaves the channel untouched");
        Require(sentenceLookups == 0 && file.opens == 0, "empty VOX does not resolve or open a WAV");
    }

    char playable[] = "vox/ (p120) clik";
    sentenceText = playable;
    SetFile(Wave());
    Require(VOX_LoadSound(&channel, name) != nullptr && channel.sfx == &sentenceSound && sentenceLookups == 1,
            "leading separators and parameter-only tokens can precede a playable word");
    S_FreeChannel(&channel);
    Cache_Free(&sentenceSound.cache);
}

short NativeLittleShort(short value) {
    return value;
}
} // namespace

short (*LittleShort)(short) = NativeLittleShort;

void *Cache_Check(cache_user_t *user) {
    return user->data;
}

void *Cache_Alloc(cache_user_t *user, int size, const char *) {
    Require(user->data == nullptr && size > 0, "cache allocation owns a fresh positive-size block");
    if (failCache) {
        return nullptr;
    }
    user->data = std::calloc(1, static_cast<std::size_t>(size));
    if (cacheUnderPressure && user == &cacheUnderPressure->cache) {
        trackedCacheBytes = size;
    }
    return user->data;
}

void Cache_Free(cache_user_t *user) {
    Require(user->data != nullptr, "only live cache blocks can be freed");
    std::free(user->data);
    user->data = nullptr;
}

FileHandle_t FS_Open(const char *, const char *) {
    if (failOpen) {
        return nullptr;
    }
    Require(!file.open, "file is not opened twice");
    file.open = true;
    file.position = 0;
    ++file.opens;
    return &file;
}

void FS_Close(FileHandle_t handle) {
    Require(handle == &file && file.open && !file.borrowed, "release read buffer before closing its live owner");
    file.open = false;
    ++file.closes;
}

void *FS_GetReadBuffer(FileHandle_t handle, int *size) {
    Require(handle == &file && file.open, "read buffer has a live file owner");
    *size = file.length;
    if (!mappedReads) {
        return nullptr;
    }
    file.borrowed = true;
    return fileBytes.get();
}

void FS_ReleaseReadBuffer(FileHandle_t handle, void *data) {
    Require(handle == &file && file.open && file.borrowed && data == fileBytes.get(),
            "the original read buffer is released exactly once to its live owner");
    file.borrowed = false;
    ++file.releases;
}

unsigned int FS_Size(FileHandle_t handle) {
    Require(handle == &file && file.open, "size query uses the open fixture file");
    return static_cast<unsigned int>(file.length);
}

void FS_Seek(FileHandle_t handle, int position, FileSystemSeek_t origin) {
    Require(handle == &file && file.open && origin == FILESYSTEM_SEEK_HEAD && position <= file.length,
            "limited loader uses an absolute in-file seek");
    file.position = position;
}

int FS_Read(void *output, int size, int count, FileHandle_t handle) {
    Require(output && handle == &file && file.open && count == 1 && size >= 0,
            "read requires a buffer and a live file");
    const int copied = std::min(size, file.length - file.position);
    std::memcpy(output, fileBytes.get() + file.position, static_cast<std::size_t>(copied));
    file.position += copied;
    return copied;
}

void *Hunk_TempAlloc(int size) {
    Require(size > 0, "temporary allocation is positive");
    if (failTemp) {
        return nullptr;
    }
    if (changeOnTempAlloc != CacheChange::None) {
        auto *owner = &cacheUnderPressure->cache;
        Require(owner->data != nullptr && trackedCacheBytes > 0, "cache pressure targets a live allocation");
        void *replacement = nullptr;
        if (changeOnTempAlloc == CacheChange::Move) {
            replacement = std::malloc(trackedCacheBytes);
            Require(replacement != nullptr, "fixture moves the cached bytes to a new allocation");
            std::memcpy(replacement, owner->data, trackedCacheBytes);
        }
        Cache_Free(owner);
        owner->data = replacement;
        changeOnTempAlloc = CacheChange::None;
    }
    temporaryBuffers.push_back(std::make_unique<byte[]>(size));
    return temporaryBuffers.back().get();
}

byte *COM_LoadStackFile(char *, void *, int, int *length) {
    if (failOpen) {
        return nullptr;
    }
    *length = file.length;
    return fileBytes.get();
}

sfxcache_t *VoiceSE_GetSFXCache(sfx_t *) {
    Require(false, "WAV cases do not use the voice decoder");
    return nullptr;
}

void Q_memset(void *destination, int value, int count) {
    std::memset(destination, value, static_cast<std::size_t>(count));
}

void Q_memcpy(void *destination, const void *source, int count) {
    std::memcpy(destination, source, static_cast<std::size_t>(count));
}

void Q_strcpy(char *destination, const char *source) {
    std::strcpy(destination, source);
}

void Q_strncpy(char *destination, const char *source, int count) {
    std::strncpy(destination, source, static_cast<std::size_t>(count));
}

int Q_strncmp(const char *first, const char *second, int count) {
    return std::strncmp(first, second, static_cast<std::size_t>(count));
}

int Q_strlen(const char *text) {
    return static_cast<int>(std::strlen(text));
}

int Q_atoi(const char *text) {
    return std::atoi(text);
}

extern "C" char *VOX_LookupString(char *, int *) {
    return sentenceText;
}

sfx_t *S_FindName(char *path, int *inCache) {
    ++sentenceLookups;
    Require(std::strcmp(path, "vox/clik.wav") == 0, "VOX resolves the playable word");
    std::strcpy(sentenceSound.name, path);
    *inCache = 0;
    return &sentenceSound;
}

extern "C" void SND_CloseMouth(channel_t *) {}

extern "C" void VoiceSE_NotifyFreeChannel(int) {
    Require(false, "WAV cases do not free network voice channels");
}

void SND_MoveMouth(channel_t *, sfxcache_t *, int) {
    Require(false, "the mixer fixture has no mouth animation entity");
}

extern "C" void VOX_TrimStartEndTimes(channel_t *, sfxcache_t *) {
    Require(false, "the mixer fixture does not advance VOX words");
}

int VOX_FPaintPitchChannelFrom8Offs(portable_samplepair_t *, channel_t *, sfxcache_t *, int, int, int, int) {
    Require(false, "the mixer fixture uses normal pitch");
    return 0;
}

int VoiceSE_GetSoundDataCallback(sfxcache_t *, char *, int, int, int) {
    Require(false, "WAV cases do not use voice callbacks");
    return 0;
}

void Con_Printf(const char *, ...) {}

void Con_DPrintf(const char *, ...) {}

double Sys_FloatTime() {
    return 0;
}

[[noreturn]] void Sys_Error(const char *, ...) {
    Require(false, "malformed WAV must not abort the engine");
    std::abort();
}

#include "owned_helpers.inc"

int main(int argc, char **argv) {
    Require(argc >= 2, "expected a WAV case");
    const std::string mode = argv[1];
    if (mode == "corpus") {
        Require(argc == 3, "expected the shipped WAV directory");
        std::size_t files = 0;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(argv[2])) {
            if (entry.path().extension() == ".wav") {
                std::ifstream input(entry.path(), std::ios::binary);
                const std::vector<byte> bytes((std::istreambuf_iterator<char>(input)), {});
                wavinfo_t info{};
                if (!Parse(bytes, info)) {
                    std::fprintf(stderr, "Rejected shipped WAV: %s\n", entry.path().string().c_str());
                    return 1;
                }
                ++files;
            }
        }
        Require(files > 0, "shipped corpus is nonempty");
        std::printf("Accepted %zu shipped WAV files\n", files);
    } else if (mode.rfind("mix-", 0) == 0) {
        Mixer(mode);
    } else if (mode.rfind("vox-", 0) == 0) {
        Vox(mode);
    } else if (mode.rfind("stream-reopen-", 0) == 0 || mode == "stream-reload-pcm") {
        StreamCache(mode);
    } else if (mode.rfind("resample-", 0) == 0) {
        Resampler(mode);
    } else if (mode.rfind("load-", 0) == 0 || (mode.rfind("stream-", 0) == 0 && mode != "stream-prefix")) {
        Loader(mode);
    } else {
        Parser(mode);
    }
}
