#include "../a3d_module/caller_interfaces.h"
#include "cse_a3d_abi.h"
#include "sdl3_audio_backend.h"
#include "mpg123_music.h"
#include "production_dma_record.h"
#include "capture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <type_traits>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); \
        std::exit(1); \
    } \
} while (false)

// Exact extracted production DMA record and bodies; prepare.py verifies physical
// fields and callable signatures by compiling the actual owning source recipe.
static cse::SdlAudioBackend audioBackend;
static volatile dma_t sn{};
static volatile dma_t* shm = nullptr;
static qboolean fakedma = false;
static int snd_blocked = 0;
#include "production_dma.inc"

namespace {
constexpr DWORD kDirectFeature = 0x08;
constexpr DWORD kSampleRate = 22050;
constexpr DWORD kBufferBytes = 4096;
constexpr unsigned kCycles = 3;
constexpr DWORD kNativeInitializationMode = 1;
constexpr unsigned kSignalPeriodSamples = 64;
constexpr Uint32 kProgressTimeoutMilliseconds = 2000;
constexpr Uint32 kPollMilliseconds = 5;
constexpr float kMusicVolume = 0.1f;
constexpr Uint32 kObserveMilliseconds = 120;
constexpr std::int16_t kSignalAmplitude = 1000;
constexpr auto expected = cse::a3d_abi::Describe<IA3dSource202, IA3dGeomTarget, IA3dListenerTarget>();
using Factory = HRESULT (*)(REFCLSID, REFIID, void**);
using CanUnload = HRESULT (*)();
using Open = decltype(&SDL_OpenAudioDeviceStream);
using Put = decltype(&SDL_PutAudioStreamData);
#if A3D_CAPTURE
A3dCapture& capture = A3dLiveCapture();
const Open realOpen = A3dLiveOriginalOpen();
const Put realPut = A3dLiveOriginalPut();
#else
A3dCapture capture;
const Open realOpen = SDL_OpenAudioDeviceStream;
const Put realPut = SDL_PutAudioStreamData;
#endif

void VerifyProvider(void* module, const char* expectedPath) {
    char canonicalExpected[PATH_MAX]{};
    char canonicalActual[PATH_MAX]{};
    CHECK(realpath(expectedPath, canonicalExpected));
    Dl_info info{};
    CHECK(dladdr(reinterpret_cast<const void*>(realOpen), &info));
    CHECK(realpath(info.dli_fname, canonicalActual));
    CHECK(std::strcmp(canonicalExpected, canonicalActual) == 0);
    CHECK(dlsym(module, "SDL_InitSubSystem") == reinterpret_cast<void*>(SDL_InitSubSystem));
    const void* modulePut = dlsym(module, "SDL_PutAudioStreamData");
#if A3D_CAPTURE
    CHECK(modulePut == reinterpret_cast<const void*>(realPut) ||
        modulePut == reinterpret_cast<const void*>(A3dLiveObservedPut()));
#else
    CHECK(modulePut == reinterpret_cast<const void*>(realPut));
#endif
    unsigned providers = 0;
    for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
        const char* path = _dyld_get_image_name(index);
        if (std::strstr(path, "libSDL3")) {
            ++providers;
        }
    }
    CHECK(providers == 1);
    std::printf("provider=%s providers=%u init=%p open=%p put=%p\n", canonicalActual, providers,
        reinterpret_cast<void*>(SDL_InitSubSystem), reinterpret_cast<void*>(realOpen),
        reinterpret_cast<void*>(realPut));
}

template<class T> T* Query(IUnknown* object, REFIID iid) {
    T* result = nullptr;
    CHECK(object->QueryInterface(iid, reinterpret_cast<void**>(&result)) == S_OK && result);
    return result;
}

struct Module {
    void* handle = nullptr;
    Factory factory = nullptr;
    CanUnload unload = nullptr;
    const char* path = nullptr;
    Module(const char* modulePath, const char* provider) : path(modulePath) {
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            std::fprintf(stderr, "%s\n", dlerror());
        }
        CHECK(handle);
        auto describe = reinterpret_cast<cse::abi::GetModuleAbi>(dlsym(handle, "CseGetModuleAbi"));
        CHECK(describe && cse::abi::Check(expected, describe()) == cse::abi::Compatibility::Compatible);
        factory = reinterpret_cast<Factory>(dlsym(handle, "DllGetClassObject"));
        unload = reinterpret_cast<CanUnload>(dlsym(handle, "DllCanUnloadNow"));
        CHECK(factory && unload && unload() == S_OK);
        VerifyProvider(handle, provider);
    }
    ~Module() {
        CHECK(unload() == S_OK);
        CHECK(dlclose(handle) == 0);
        void* residual = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
        CHECK(!residual);
    }
    IA3d3* Api() {
        IClassFactory* object = nullptr;
        CHECK(factory(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&object)) == S_OK);
        IA3d3* api = nullptr;
        CHECK(object->CreateInstance(nullptr, IID_IA3d3, reinterpret_cast<void**>(&api)) == S_OK);
        object->Release();
        capture.admitting = true;
        const HRESULT result = api->Init(nullptr, kDirectFeature, 0);
        capture.admitting = false;
        CHECK(result == S_OK);
        CHECK(api->SetResourceManagerMode(A3D_RESOURCE_MODE_OFF) == S_OK);
        return api;
    }
    IDirectSound* Provider() {
        IClassFactory* object = nullptr;
        CHECK(factory(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&object)) == S_OK);
        IDirectSound* sound = nullptr;
        CHECK(object->CreateInstance(nullptr, IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
        object->Release();
        auto* device = Query<IA3dNativeDeviceV1>(sound, IID_IA3dNativeDeviceV1);
        const GUID invalid{0xffffffff, 0xffff, 0xffff, {}};
        DWORD features = 0;
        CHECK(FAILED(device->InitializeDevice(&invalid, kDirectFeature, kNativeInitializationMode, &features)));
        CHECK(features == 0);
        capture.admitting = true;
        const HRESULT result = device->InitializeDevice(nullptr, kDirectFeature, kNativeInitializationMode, &features);
        capture.admitting = false;
        CHECK(result == S_OK && features == kDirectFeature);
        device->Release();
        return sound;
    }
};

IA3dSource202* Source(IA3d3* api) {
    IA3dSource* raw = nullptr;
    CHECK(api->NewSource(0, &raw) == S_OK && raw);
    auto* source = reinterpret_cast<IA3dSource202*>(raw);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, kSampleRate, kSampleRate * 2, 2, 16, 0};
    CHECK(source->SetWaveFormat(&format) == S_OK);
    CHECK(source->AllocateWaveData(kBufferBytes) == S_OK);
    void* first = nullptr;
    void* second = nullptr;
    DWORD firstBytes = 0;
    DWORD secondBytes = 0;
    CHECK(source->Lock(0, kBufferBytes, &first, &firstBytes, &second, &secondBytes, 0) == S_OK);
    CHECK(firstBytes == kBufferBytes && !secondBytes);
    auto* samples = static_cast<std::int16_t*>(first);
    for (DWORD index = 0; index < kBufferBytes / sizeof(*samples); ++index) {
        samples[index] = index % kSignalPeriodSamples < kSignalPeriodSamples / 2 ?
            kSignalAmplitude : -kSignalAmplitude;
    }
    CHECK(source->Unlock(first, firstBytes, second, secondBytes) == S_OK);
    CHECK(source->SetGain(0.5f) == S_OK);
    CHECK(source->SetPosition3f(0, 0, -1) == S_OK);
    return source;
}

DWORD Cursor(IA3dSource202* buffer) {
    DWORD cursor = 0;
    CHECK(buffer->GetWavePosition(&cursor) == S_OK);
    return cursor;
}

void ObserveProgress(IA3dSource202* buffer, const char* stage) {
    const DWORD before = Cursor(buffer);
    const Uint64 deadline = SDL_GetTicks() + kProgressTimeoutMilliseconds;
    while (Cursor(buffer) == before && SDL_GetTicks() < deadline) {
        SDL_Delay(kPollMilliseconds);
    }
    std::printf("progress stage=%s before=%u after=%u\n", stage, before, Cursor(buffer));
    CHECK(Cursor(buffer) != before);
}

void StartOwners(cse::Mpg123MusicPlayer& music, const char* track) {
    CHECK(SNDDMA_Init());
    CHECK(SNDDMA_LockBuffer());
    auto* samples = reinterpret_cast<std::int16_t*>(sn.buffer);
    for (int index = 0; index < sn.samples; ++index) {
        samples[index] = index % kSignalPeriodSamples < kSignalPeriodSamples / 2 ? kSignalAmplitude : -kSignalAmplitude;
    }
    SNDDMA_UnlockBuffer();
    CHECK(music.Init() && music.Open(track, true) && music.Start());
    music.SetVolume(kMusicVolume);
    CHECK(music.IsPlaying());
    S_BlockSound();
    CHECK(audioBackend.IsPaused());
    S_UnblockSound();
    CHECK(!audioBackend.IsPaused());
    music.Pause(true);
    CHECK(!music.IsPlaying());
    music.Pause(false);
    CHECK(music.IsPlaying());
}

void OwnersProgress(cse::Mpg123MusicPlayer& music) {
    const int before = SNDDMA_GetDMAPos();
    const Uint64 deadline = SDL_GetTicks() + kProgressTimeoutMilliseconds;
    while (SNDDMA_GetDMAPos() == before && SDL_GetTicks() < deadline) {
        SDL_Delay(kPollMilliseconds);
    }
    CHECK(SNDDMA_GetDMAPos() != before && music.IsPlaying());
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
}

void ResetCapture() {
    capture.stream = nullptr;
    capture.bytes = 0;
    capture.nonzero = 0;
    capture.pcm.fill(0);
}

void DrainCapture(const char* output, unsigned cycle) {
#if A3D_CAPTURE
    std::printf("capture_probe stream=%p bytes=%zu nonzero=%zu recursion=%d\n",
        static_cast<void*>(capture.stream.load()), capture.bytes.load(), capture.nonzero.load(),
        static_cast<int>(capture.recursion.load()));
    CHECK(!capture.recursion && capture.bytes > 0 && capture.nonzero > 0);
    CHECK(capture.input.format == SDL_AUDIO_S16 && capture.input.channels == 2 && capture.input.freq == static_cast<int>(kSampleRate));
    char path[PATH_MAX]{};
    std::snprintf(path, sizeof(path), "%s/cycle-%u-preconversion-s16-stereo-22050.pcm", output, cycle);
    FILE* file = std::fopen(path, "wb");
    CHECK(file);
    const std::size_t count = std::min(capture.bytes.load(), capture.pcm.size());
    CHECK(std::fwrite(capture.pcm.data(), 1, count, file) == count);
    CHECK(std::fclose(file) == 0);
    std::printf("capture cycle=%u bytes=%zu stored=%zu nonzero_samples=%zu input=%d,%d,%u conversion=%d,%d,%u device=%d,%d,%u name=%s\n",
        cycle, capture.bytes.load(), count, capture.nonzero.load(), capture.input.freq, capture.input.channels,
        capture.input.format, capture.conversion.freq, capture.conversion.channels, capture.conversion.format,
        capture.device.freq, capture.device.channels, capture.device.format, capture.name.data());
#else
    (void)output;
    (void)cycle;
#endif
}
}

int main(int argc, char** argv) {
    CHECK(argc == 6 || argc == 7);
    const bool dummy = std::strcmp(argv[1], "--dummy") == 0;
    const bool live = std::strcmp(argv[1], "--root-authorized-live-default") == 0;
    CHECK(dummy || live);
    const char* driver = std::getenv("SDL_AUDIO_DRIVER");
    if (dummy) {
        CHECK(driver && std::strcmp(driver, "dummy") == 0);
    } else {
        CHECK(!driver || std::strcmp(driver, "dummy") != 0);
        CHECK(std::getenv("CSE_ROOT_LIVE_OUTPUT_AUTHORIZED") &&
            std::strcmp(std::getenv("CSE_ROOT_LIVE_OUTPUT_AUTHORIZED"), "1") == 0);
    }
    const bool fault = argc == 7 && std::strcmp(argv[6], "--inject-put-failure") == 0;
    CHECK(argc == 6 || fault);
    CHECK(!fault || A3D_CAPTURE);
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    std::printf("mode=%s instrumented=%d guid_selected_public_api=OPEN physical_audibility=UNQUALIFIED\n",
        dummy ? "dummy" : "actual-default-output", A3D_CAPTURE);
    for (unsigned cycle = 0; cycle < kCycles; ++cycle) {
        ResetCapture();
        cse::Mpg123MusicPlayer music;
        StartOwners(music, argv[4]);
        if (dummy) {
            CHECK(std::strcmp(SDL_GetCurrentAudioDriver(), "dummy") == 0);
        }
        {
            Module module(argv[2], argv[3]);
            auto* sound = module.Provider();
            sound->Release();
            ResetCapture();
            auto* api = module.Api();
            std::printf("backend=%s cycle=%u\n", SDL_GetCurrentAudioDriver(), cycle);
            auto* buffer = Source(api);
            CHECK(buffer->Play(A3D_LOOPED) == S_OK);
            CHECK(api->Flush() == S_OK);
            ObserveProgress(buffer, "initial");
            SDL_Delay(kObserveMilliseconds);
            CHECK(buffer->Stop() == S_OK);
            const DWORD paused = Cursor(buffer);
            SDL_Delay(kObserveMilliseconds);
            CHECK(Cursor(buffer) == paused);
            CHECK(buffer->Play(A3D_LOOPED) == S_OK);
            CHECK(api->Flush() == S_OK);
            ObserveProgress(buffer, "resumed");
            OwnersProgress(music);
            if (cycle == 1) {
                // Reverse teardown: real engine/music owners retire while A3D continues.
                music.Shutdown();
                music.Shutdown();
                SNDDMA_Shutdown();
                SNDDMA_Shutdown();
                CHECK(SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
                ObserveProgress(buffer, "after_other_owners_shutdown");
            }
            if (fault) {
                capture.injectFailure = true;
                const Uint64 deadline = SDL_GetTicks() + kProgressTimeoutMilliseconds;
                while (capture.injectFailure && SDL_GetTicks() < deadline) {
                    SDL_Delay(kPollMilliseconds);
                }
                CHECK(!capture.injectFailure);
            }
            CHECK(buffer->Stop() == S_OK);
            buffer->Release();
            api->Release();
            CHECK(module.unload() == S_OK);
            // Close joined/quiesced output: callback bytes may now be read.
            DrainCapture(argv[5], cycle);
        }
        if (cycle != 1) {
            OwnersProgress(music);
        }
        music.Shutdown();
        music.Shutdown();
        SNDDMA_Shutdown();
        SNDDMA_Shutdown();
        CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
        std::printf("cycle=%u unload_absent=1 owner_shutdown_balanced=1\n", cycle);
    }
    CHECK(!capture.recursion);
    CHECK(!fault || capture.failures == kCycles);
    std::printf("PASS cycles=%u injected_transport_failures=%u hardware_removal=UNRUN public_restore=UNSUPPORTED\n",
        kCycles, capture.failures.load());
}
