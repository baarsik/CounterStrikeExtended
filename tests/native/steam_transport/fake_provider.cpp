#include <cstdlib>
#include <cstdint>
#include <array>
#if defined(_WIN32)
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif
namespace {
int creates;
int releases;
int shutdowns;
constexpr std::size_t MaxFakePipes = 4;
std::array<std::int32_t, MaxFakePipes> livePipes{};
int liveCount;
int failCreate;
int failRelease;
#if !defined(CSE_INCOMPLETE_TRANSPORT_PROVIDER)
constexpr std::int32_t TestPipe = 123;
#endif
int client;
}
EXPORT void* CseTransportFixtureClient() {
    return &client;
}
EXPORT int CseTransportFixtureValue(int index) {
    const int values[] = {creates, releases, shutdowns, liveCount};
    if (index < 0 || index >= static_cast<int>(sizeof(values) / sizeof(values[0]))) {
        std::abort();
    }
    return values[index];
}
EXPORT void CseTransportFixtureFailure(int create, int release) {
    failCreate = create;
    failRelease = release;
}
EXPORT std::int32_t SteamAPI_ISteamClient_CreateSteamPipe(void* object) {
#if defined(CSE_INCOMPLETE_TRANSPORT_PROVIDER)
    (void)object;
    std::abort();
#else
    if (object != &client || liveCount == static_cast<int>(livePipes.size())) {
        std::abort();
    }
    ++creates;
    if (failCreate) {
        return 0;
    }
    for (std::int32_t& slot : livePipes) {
        if (!slot) {
            slot = TestPipe + creates;
            ++liveCount;
            return slot;
        }
    }
    std::abort();
#endif
}
EXPORT bool SteamAPI_ISteamClient_BReleaseSteamPipe(void* object, std::int32_t pipe) {
    if (object != &client || pipe == 0) {
        std::abort();
    }
    for (std::int32_t& slot : livePipes) {
        if (slot == pipe) {
            ++releases;
            if (failRelease) {
                return false;
            }
            slot = 0;
            --liveCount;
            return true;
        }
    }
    std::abort();
}
#if !defined(CSE_INCOMPLETE_TRANSPORT_PROVIDER)
EXPORT bool SteamAPI_ISteamClient_BShutdownIfAllPipesClosed(void* object) {
    if (object != &client) {
        std::abort();
    }
    ++shutdowns;
    return liveCount == 0;
}
#endif
