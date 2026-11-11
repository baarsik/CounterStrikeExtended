// Explicit dummy-only, test-process interposition at the real acquisition operations.
// No provider selection, physical IDs, product exports or factory are replaced.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

extern "C" {
bool SDL_InitSubSystem(std::uint32_t);
void SDL_QuitSubSystem(std::uint32_t);
std::uint32_t* SDL_GetAudioPlaybackDevices(int*);
const char* SDL_GetAudioDeviceName(std::uint32_t);
void* SDL_malloc(std::size_t);
void SDL_free(void*);
}
namespace {
const char* scenario = nullptr;
unsigned observed = 0;
unsigned allocations = 0;
unsigned initBalance = 0;
std::array<void*, 128> owned{};

bool Is(const char* value) {
    return scenario && std::strcmp(scenario, value) == 0;
}
bool ProductCaller(void* address) {
    Dl_info image{};
    return dladdr(address, &image) && image.dli_fname && std::strstr(image.dli_fname, "/a3dapi.dylib");
}
void Record(void* pointer) {
    if (!pointer) {
        return;
    }
    for (auto& slot : owned) {
        if (!slot) {
            slot = pointer;
            return;
        }
    }
    std::abort();
}
extern "C" bool ObserveInit(std::uint32_t flags) {
    if (Is("init")) {
        ++observed;
        return false;
    }
    const bool result = SDL_InitSubSystem(flags);
    if (scenario && result && ProductCaller(__builtin_return_address(0))) {
        ++initBalance;
    }
    return result;
}
extern "C" void ObserveQuit(std::uint32_t flags) {
    if (scenario && ProductCaller(__builtin_return_address(0))) {
        if (!initBalance) {
            std::abort();
        }
        --initBalance;
    }
    SDL_QuitSubSystem(flags);
}
extern "C" std::uint32_t* ObserveDevices(int* count) {
    if (Is("devices")) {
        ++observed;
        *count = 0;
        return nullptr;
    }
    auto* result = SDL_GetAudioPlaybackDevices(count);
    if (Is("negative")) {
        ++observed;
        *count = -1;
    } else if (Is("empty")) {
        ++observed;
        SDL_free(result);
        result = static_cast<std::uint32_t*>(SDL_malloc(sizeof(std::uint32_t)));
        if (!result) {
            std::abort();
        }
        *result = 0;
        *count = 0;
    }
    if (scenario) {
        Record(result);
    }
    return result;
}
extern "C" const char* ObserveName(std::uint32_t id) {
    const char* actual = SDL_GetAudioDeviceName(id);
    if (!scenario) {
        return actual;
    }
    if (Is("name")) {
        ++observed;
        return nullptr;
    }
    if (Is("malformed")) {
        ++observed;
        return "\xc0\xaf"; // Forbidden overlong encoding.
    }
    if (Is("surrogate")) {
        ++observed;
        return "\xed\xa0\x80";
    }
    if (Is("truncated")) {
        ++observed;
        return "\xf0\x9f";
    }
    if (Is("out_of_range")) {
        ++observed;
        return "\xf4\x90\x80\x80";
    }
    if (Is("continuation")) {
        ++observed;
        return "\xe2x\xac";
    }
    if (Is("empty_name")) {
        ++observed;
        return "";
    }
    if (Is("multibyte")) {
        ++observed;
        return "Głośnik 🎧";
    }
    if (Is("long")) {
        static std::array<char, 4097> label{};
        label.fill('x');
        label.back() = 0;
        ++observed;
        return label.data();
    }
    return actual;
}
extern "C" void* ObserveMalloc(std::size_t bytes) {
    const bool acquisition = scenario && ProductCaller(__builtin_return_address(0));
    if (acquisition) {
        ++allocations;
        const unsigned failAt = Is("alloc1") ? 1 : Is("alloc2") ? 2 : Is("alloc3") ? 3 : 0;
        if (allocations == failAt) {
            ++observed;
            return nullptr;
        }
    }
    void* result = SDL_malloc(bytes);
    if (acquisition) {
        Record(result);
    }
    return result;
}
extern "C" void ObserveFree(void* pointer) {
    for (auto& slot : owned) {
        if (slot == pointer) {
            slot = nullptr;
        }
    }
    SDL_free(pointer);
}
#define INTERPOSE(replacement, original) \
    __attribute__((used)) static const struct { const void* replacement; const void* original; } \
    interpose_##original __attribute__((section("__DATA,__interpose"))) = { \
        reinterpret_cast<const void*>(&replacement), reinterpret_cast<const void*>(&original) }
INTERPOSE(ObserveInit, SDL_InitSubSystem);
INTERPOSE(ObserveQuit, SDL_QuitSubSystem);
INTERPOSE(ObserveDevices, SDL_GetAudioPlaybackDevices);
INTERPOSE(ObserveName, SDL_GetAudioDeviceName);
INTERPOSE(ObserveMalloc, SDL_malloc);
INTERPOSE(ObserveFree, SDL_free);
#undef INTERPOSE
}
extern "C" void CseEnumerationFaultArm(const char* label) {
    const char* driver = std::getenv("SDL_AUDIO_DRIVER");
    if (!driver || std::strcmp(driver, "dummy") || scenario) {
        std::abort();
    }
    scenario = label;
    observed = 0;
    allocations = 0;
    initBalance = 0;
}
extern "C" unsigned CseEnumerationFaultFinish() {
    // Called after all enumerator references are released, including successful label cases.
    for (const auto* pointer : owned) {
        if (pointer) {
            std::abort();
        }
    }
    if (initBalance) {
        std::abort();
    }
    scenario = nullptr;
    return observed;
}
