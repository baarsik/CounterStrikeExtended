#include "../a3d_module/enumeration_caller.h"
#include "sdl3_audio_backend.h"
#include "mpg123_music.h"
#include "production_dma_record.h"
#include "capture.h"
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
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
#include <string>
#include <vector>

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
constexpr int kNoUniqueMatch = -1; // Missing and ambiguous labels both fail selection.
const auto expected = ExpectedA3dEnumerationDescriptor();
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
    const void* moduleOpen = dlsym(module, "SDL_OpenAudioDeviceStream");
    const void* modulePut = dlsym(module, "SDL_PutAudioStreamData");
#if A3D_CAPTURE
    CHECK(moduleOpen == reinterpret_cast<const void*>(realOpen) ||
        moduleOpen == reinterpret_cast<const void*>(A3dLiveObservedOpen()));
    CHECK(modulePut == reinterpret_cast<const void*>(realPut) ||
        modulePut == reinterpret_cast<const void*>(A3dLiveObservedPut()));
#else
    CHECK(moduleOpen == reinterpret_cast<const void*>(realOpen));
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

std::string JsonString(const std::string& text) {
    constexpr unsigned char kJsonControlLimit = 0x20;
    constexpr std::size_t kJsonEscapeBytes = 7; // Backslash-u, four hex digits, terminator.
    std::string result = "\"";
    for (unsigned char value : text) {
        if (value == '"' || value == '\\') {
            result += '\\';
            result += static_cast<char>(value);
        } else if (value < kJsonControlLimit) {
            char escaped[kJsonEscapeBytes]{};
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(value));
            result += escaped;
        } else {
            result += static_cast<char>(value);
        }
    }
    return result + "\"";
}

// Exact complete UTF-8 label comparison, with ambiguity rejected even when a
// snapshot happens to repeat a friendly label. No token representation is read.
int UniqueMatch(const std::vector<std::string>& names, const std::string& requested) {
    int selected = kNoUniqueMatch;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (names[index] == requested) {
            if (selected >= 0) {
                return kNoUniqueMatch;
            }
            selected = static_cast<int>(index);
        }
    }
    return selected;
}

independent::Factory Admit(void* handle, const cse::abi::Descriptor& wanted,
    const cse::abi::Descriptor* offered, const char* exportName) {
    if (cse::abi::Check(wanted, offered) != cse::abi::Compatibility::Compatible) {
        return nullptr;
    }
    return reinterpret_cast<independent::Factory>(dlsym(handle, exportName));
}

bool RejectMutation(void* handle, const char* label, const cse::abi::Descriptor& wanted,
    const cse::abi::Descriptor* offered, const char* exportName) {
    const bool rejected = !Admit(handle, wanted, offered, exportName);
    std::printf("admission_negative=%s rejected=%d\n", label, rejected);
    return rejected;
}

struct Module {
    void* handle = nullptr;
    Factory factory = nullptr;
    CanUnload unload = nullptr;
    const char* path = nullptr;
    independent::Enumerator* enumeration = nullptr;
    independent::DeviceInfo selected{};
    std::string selectedName;
    bool selecting = false;

    Module(const char* modulePath, const char* provider, const char* requested, bool first) : path(modulePath) {
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            std::fprintf(stderr, "%s\n", dlerror());
        }
        CHECK(handle);
        auto describe = reinterpret_cast<cse::abi::GetModuleAbi>(dlsym(handle, "CseGetModuleAbi"));
        CHECK(describe);
        const auto* offered = describe();
        const auto enumerate = Admit(handle, expected, offered, independent::FactoryName);
        CHECK(enumerate);
        CHECK(CheckEnumerationAdmissionMutations(*offered, RejectMutation, handle));
        std::printf("abi_admitted=1 enumeration_profile=1 descriptor_version=%u runtime_version=%u tables=%u\n",
            offered->descriptorVersion, offered->runtimeVersion, offered->tableCount);
        for (std::uint32_t table = 0; table < offered->tableCount; ++table) {
            std::printf("abi_table index=%u identifier=%u signature=%016llx\n", table,
                offered->tables[table].identifier, static_cast<unsigned long long>(offered->tables[table].signature));
        }
        factory = reinterpret_cast<Factory>(dlsym(handle, "DllGetClassObject"));
        unload = reinterpret_cast<CanUnload>(dlsym(handle, "DllCanUnloadNow"));
        CHECK(factory && unload && unload() == S_OK);
        VerifyProvider(handle, provider);
        selecting = requested || first;
        if (selecting) {
            CHECK(enumerate(&enumeration) == S_OK && enumeration);
            CHECK(unload() == S_FALSE);
            DWORD count = 0;
            CHECK(enumeration->GetCount(&count) == S_OK);
            std::vector<independent::DeviceInfo> records;
            std::vector<std::string> names;
            for (DWORD index = 0; index < count; ++index) {
                independent::DeviceInfo record{};
                DWORD required = 0;
                CHECK(enumeration->GetInfo(index, independent::Version, sizeof(record),
                    &record, nullptr, 0, &required) == S_FALSE && required > 0);
                std::vector<char> name(required);
                CHECK(enumeration->GetInfo(index, independent::Version, sizeof(record),
                    &record, name.data(), required, &required) == S_OK);
                CHECK(record.structureBytes == sizeof(record) && record.version == independent::Version &&
                    record.reserved == 0 && record.nameBytes == required && name.back() == '\0');
                names.emplace_back(name.data());
                records.push_back(record);
                std::printf("enumeration index=%u factory=%s record_version=%u name=%s\n", index,
                    independent::FactoryName, record.version, JsonString(names.back()).c_str());
            }
            CHECK(!names.empty());
            const std::string wanted = first ? names.front() : requested;
            const int index = UniqueMatch(names, wanted);
            CHECK(index >= 0); // Unknown/truncated/ambiguous labels fail before any owner opens.
            selected = records[static_cast<std::size_t>(index)];
            selectedName = names[static_cast<std::size_t>(index)];
            std::printf("selection_json={\"label\":%s,\"index\":%d,\"record_version\":%u}\n",
                JsonString(selectedName).c_str(), index, selected.version);
            std::printf("selected_opaque_token=");
            const auto* bytes = reinterpret_cast<const unsigned char*>(&selected.device);
            for (std::size_t byte = 0; byte < sizeof(selected.device); ++byte) {
                std::printf("%02x", static_cast<unsigned>(bytes[byte]));
            }
            std::printf("\n");
        }
    }

    void ReleaseEnumeration() {
        if (enumeration) {
            CHECK(enumeration->Release() == 0);
            enumeration = nullptr;
        }
    }

    ~Module() {
        ReleaseEnumeration();
        CHECK(unload() == S_OK);
        CHECK(dlclose(handle) == 0);
        void* residual = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
        CHECK(!residual);
    }

    void VerifySelection(const char* stage) {
#if A3D_CAPTURE
        if (selecting) {
            CHECK(capture.stream && capture.requestedDevice != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK &&
                capture.requestedDevice != 0 && capture.name == selectedName);
            std::printf("selected_open stage=%s explicit=1 requested=%u logical=%u label=%s\n", stage,
                capture.requestedDevice, capture.logicalDevice, JsonString(capture.name).c_str());
        }
#else
        (void)stage;
#endif
    }

    IA3d3* Api() {
        IClassFactory* object = nullptr;
        CHECK(factory(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&object)) == S_OK);
        IA3d3* api = nullptr;
        CHECK(object->CreateInstance(nullptr, IID_IA3d3, reinterpret_cast<void**>(&api)) == S_OK);
        object->Release();
        capture.admitting = true;
        const HRESULT result = api->Init(selecting ? &selected.device : nullptr, kDirectFeature, 0);
        capture.admitting = false;
        CHECK(result == S_OK);
        VerifySelection("source");
        // Source now owns an SDL reference. Preserve the anchor across the earlier
        // low-level close and this Init so its copied token never loses context.
        ReleaseEnumeration();
        CHECK(api->SetResourceManagerMode(A3D_RESOURCE_MODE_OFF) == S_OK);
        return api;
    }

    IDirectSound* Provider() {
        IClassFactory* object = nullptr;
        CHECK(factory(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&object)) == S_OK);
        IDirectSound* sound = nullptr;
        CHECK(object->CreateInstance(nullptr, IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
        object->Release();
        auto* device = Query<independent::Device>(sound, independent::DeviceIid);
        DWORD features = 0;
        capture.admitting = true;
        const HRESULT result = device->InitializeDevice(selecting ? &selected.device : nullptr,
            kDirectFeature, kNativeInitializationMode, &features);
        capture.admitting = false;
        CHECK(result == S_OK && features == kDirectFeature);
        VerifySelection("provider");
        if (selecting) {
            std::printf("public_guid_selected_api=PASS\n");
        }
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
    CHECK(music.Init());
    capture.admittingMusic = true;
    const bool opened = music.Open(track, true);
    capture.admittingMusic = false;
    CHECK(opened && music.Start());
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

void OwnersProgress(cse::Mpg123MusicPlayer& music, const char* stage) {
    const int before = SNDDMA_GetDMAPos();
    const std::size_t musicBefore = capture.music.bytes.load();
    const Uint64 deadline = SDL_GetTicks() + kProgressTimeoutMilliseconds;
    while ((SNDDMA_GetDMAPos() == before ||
        (A3D_CAPTURE && capture.music.bytes.load() <= musicBefore)) && SDL_GetTicks() < deadline) {
        SDL_Delay(kPollMilliseconds);
    }
    CHECK(SNDDMA_GetDMAPos() != before && music.IsPlaying());
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
#if A3D_CAPTURE
    CHECK(capture.music.stream && capture.music.bytes.load() > musicBefore && capture.music.nonzero > 0);
    std::printf("owners stage=%s dma_progress=1 music_state=playing music_pcm_progress=PASS before=%zu after=%zu\n",
        stage, musicBefore, capture.music.bytes.load());
#else
    std::printf("owners stage=%s dma_progress=1 music_state=playing music_pcm_progress=UNOBSERVED\n", stage);
    (void)musicBefore;
#endif
}

void ResetCapture(A3dStreamCapture& target) {
    // Caller must have joined the producer/destroyed its stream before resetting.
    target.stream = nullptr;
    target.bytes = 0;
    target.nonzero = 0;
    target.pcm.fill(0);
}

void StopOwners(cse::Mpg123MusicPlayer& music, const char* output, unsigned cycle) {
    music.Shutdown();
    music.Shutdown();
#if A3D_CAPTURE
    CHECK(capture.music.bytes > 0 && capture.music.nonzero > 0);
    const std::string path = std::string(output) + "/cycle-" + std::to_string(cycle) + "-music-successful.pcm";
    FILE* file = std::fopen(path.c_str(), "wb");
    CHECK(file);
    const std::size_t count = std::min(capture.music.bytes.load(), capture.music.pcm.size());
    CHECK(std::fwrite(capture.music.pcm.data(), 1, count, file) == count);
    CHECK(std::fclose(file) == 0);
    std::printf("music_capture cycle=%u successful_bytes=%zu stored=%zu nonzero_bytes=%zu\n", cycle,
        capture.music.bytes.load(), count, capture.music.nonzero.load());
#else
    (void)output;
    (void)cycle;
#endif
    ResetCapture(capture.music);
    SNDDMA_Shutdown();
    SNDDMA_Shutdown();
}

void DrainCapture(const char* output, unsigned cycle) {
#if A3D_CAPTURE
    std::printf("capture_probe stream=%p bytes=%zu nonzero=%zu recursion=%d\n",
        static_cast<void*>(capture.stream.load()), capture.bytes.load(), capture.nonzero.load(),
        static_cast<int>(capture.recursion.load()));
    CHECK(!capture.recursion && capture.bytes > 0 && capture.nonzero > 0);
    CHECK(capture.input.format == SDL_AUDIO_S16 && capture.input.channels == 2 &&
        capture.input.freq == static_cast<int>(kSampleRate));
    char path[PATH_MAX]{};
    std::snprintf(path, sizeof(path), "%s/cycle-%u-preconversion-s16-stereo-22050.pcm", output, cycle);
    FILE* file = std::fopen(path, "wb");
    CHECK(file);
    const std::size_t count = std::min(capture.bytes.load(), capture.pcm.size());
    CHECK(std::fwrite(capture.pcm.data(), 1, count, file) == count);
    CHECK(std::fclose(file) == 0);
    std::printf("capture cycle=%u bytes=%zu stored=%zu nonzero_bytes=%zu input=%d,%d,%u "
        "conversion=%d,%d,%u device=%d,%d,%u name=%s\n",
        cycle, capture.bytes.load(), count, capture.nonzero.load(), capture.input.freq, capture.input.channels,
        capture.input.format, capture.conversion.freq, capture.conversion.channels, capture.conversion.format,
        capture.device.freq, capture.device.channels, capture.device.format, capture.name.c_str());
#else
    (void)output;
    (void)cycle;
#endif
}
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    if (argc == 2 && std::strcmp(argv[1], "--self-test") == 0) {
        CHECK(UniqueMatch({"full label", "full label suffix"}, "full label") == 0);
        CHECK(UniqueMatch({"full label"}, "full") == kNoUniqueMatch);
        CHECK(UniqueMatch({"full label"}, "missing") == kNoUniqueMatch);
        CHECK(UniqueMatch({"full label", "full label"}, "full label") == kNoUniqueMatch);
        CHECK(UniqueMatch({"UTF-8 ł label"}, "UTF-8 ł label") == 0);
        auto wrongProfile = expected;
        ++wrongProfile.tables[0].signature;
        CHECK(cse::abi::Check(expected, &wrongProfile) != cse::abi::Compatibility::Compatible);
#if A3D_CAPTURE
        // Device-less SDL stream: prove real failed Put calls cannot advance the
        // music success counters. This does not initialize/open any audio device.
        const SDL_AudioSpec format{SDL_AUDIO_S16, 1, static_cast<int>(kSampleRate)};
        SDL_AudioStream* stream = SDL_CreateAudioStream(&format, &format);
        CHECK(stream);
        capture.music.stream = stream;
        const std::int16_t samples[] = {kSignalAmplitude, -kSignalAmplitude};
        CHECK(!A3dLiveObservedPut()(stream, nullptr, sizeof(samples)));
        CHECK(capture.music.bytes == 0 && capture.music.nonzero == 0);
        CHECK(A3dLiveObservedPut()(stream, samples, sizeof(samples)));
        CHECK(capture.music.bytes == sizeof(samples) && capture.music.nonzero > 0);
        SDL_DestroyAudioStream(stream);
        ResetCapture(capture.music);
        CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
        std::printf("PASS music_success_only failed_put_bytes=0 successful_put_bytes=%zu\n", sizeof(samples));
#endif
        std::printf("PASS selection_missing_ambiguous_prefix_utf8 profile_rejection no_device_actions\n");
        return 0;
    }
    CHECK(argc >= 6);
    const bool dummy = std::strcmp(argv[1], "--dummy") == 0;
    const bool liveDefault = std::strcmp(argv[1], "--root-authorized-live-default") == 0;
    const bool liveSelected = std::strcmp(argv[1], "--root-authorized-live-selected") == 0;
    CHECK(dummy || liveDefault || liveSelected);
    bool fault = false;
    bool first = false;
    bool sourceOnly = false;
    const char* requested = nullptr;
    for (int argument = 6; argument < argc; ++argument) {
        if (std::strcmp(argv[argument], "--inject-put-failure") == 0) {
            CHECK(!fault);
            fault = true;
        } else if (std::strcmp(argv[argument], "--select-first-enumerated") == 0) {
            CHECK(!first);
            first = true;
        } else if (std::strcmp(argv[argument], "--selected-source-only") == 0) {
            CHECK(!sourceOnly);
            sourceOnly = true;
        } else if (std::strcmp(argv[argument], "--select-device-name") == 0) {
            CHECK(!requested && ++argument < argc);
            requested = argv[argument];
        } else {
            CHECK(false);
        }
    }
    const bool selected = requested || first;
    CHECK(!(requested && first) && (!first || dummy));
    CHECK(!liveSelected || requested);
    CHECK(!liveDefault || !selected);
    CHECK(!sourceOnly || selected);
    CHECK(!fault || A3D_CAPTURE);
    const char* driver = std::getenv("SDL_AUDIO_DRIVER");
    if (dummy) {
        CHECK(driver && std::strcmp(driver, "dummy") == 0);
    } else {
        CHECK(!driver || std::strcmp(driver, "dummy") != 0);
        CHECK(std::getenv("CSE_ROOT_LIVE_OUTPUT_AUTHORIZED") &&
            std::strcmp(std::getenv("CSE_ROOT_LIVE_OUTPUT_AUTHORIZED"), "1") == 0);
    }
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    SDL_SetMainReady();
    std::printf("mode=%s instrumented=%d selected_source_only=%d physical_audibility=UNQUALIFIED\n",
        argv[1], A3D_CAPTURE, sourceOnly);
    for (unsigned cycle = 0; cycle < kCycles; ++cycle) {
        ResetCapture(capture);
        cse::Mpg123MusicPlayer music;
        {
            // Exact-name admission precedes default DMA/music opens as well.
            Module module(argv[2], argv[3], requested, first);
            if (!sourceOnly) {
                StartOwners(music, argv[4]);
            }
            auto* sound = module.Provider();
            sound->Release();
            ResetCapture(capture);
            auto* api = module.Api();
            if (dummy) {
                CHECK(std::strcmp(SDL_GetCurrentAudioDriver(), "dummy") == 0);
            }
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
            if (!sourceOnly) {
                OwnersProgress(music, "coexistence");
            }
            if (!sourceOnly && cycle == 1) {
                StopOwners(music, argv[5], cycle);
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
            DrainCapture(argv[5], cycle);
        }
        if (!sourceOnly && cycle != 1) {
            OwnersProgress(music, "after_a3d_teardown");
            StopOwners(music, argv[5], cycle);
        }
        CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
        std::printf("cycle=%u unload_absent=1 owner_shutdown_balanced=1\n", cycle);
    }
#if A3D_CAPTURE
    if (sourceOnly) {
        constexpr unsigned kOpensPerCycle = 2; // Low-level provider, then public source provider.
        CHECK(capture.defaultOpens == 0 && capture.explicitOpens == kCycles * kOpensPerCycle);
        std::printf("selected_source_only explicit_opens=%u default_opens=0 dma_music_owners=UNOPENED\n",
            capture.explicitOpens);
    }
#endif
    CHECK(!capture.recursion);
    CHECK(!fault || capture.failures == kCycles);
    std::printf("PASS cycles=%u injected_transport_failures=%u hardware_removal=UNRUN public_restore=UNSUPPORTED\n",
        kCycles, capture.failures.load());
}
