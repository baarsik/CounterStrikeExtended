#include "linked_owner.h"
#include "engine/native_software_lifetime.h"
#include "public/native_dedicated_lifetime.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <mach-o/dyld.h>
using namespace cse::software_lifetime;
static bool nativeSoftwareClientEntered = false;
static bool identityReady = false;
static bool listenReady = false;
static bool audioReady = false;
static bool fontReady = false;
static bool callbacksReady = false;
static bool remoteReady = false;
static bool host_initialized = false;
static void* videomode = nullptr;
static int identityCalls = 0;
static int listenCalls = 0;
static int audioCalls = 0;
static int fontCalls = 0;
static int callbackCalls = 0;
static int remoteCalls = 0;
namespace cse {
bool InitializeNativeGameUIHost() {
    ++identityCalls;
    return identityReady;
}
bool FinishNativeListenShutdown() {
    ++listenCalls;
    return listenReady;
}
} // namespace cse
struct Dedicated {
    bool ready = false;
    int calls = 0;
    void ShutdownResources() {
        ++calls;
    }
    unsigned int Retire(unsigned int version) {
        assert(version == CSE_NATIVE_DEDICATED_LIFETIME_VERSION);
        return ready ? CSE_DEDICATED_RETIRE_READY : CSE_DEDICATED_RETIRE_SESSION_ACTIVE;
    }
};
static Dedicated nativeDedicatedServer;
static bool CDAudio_ShutdownForModuleUnload() {
    ++audioCalls;
    return audioReady;
}
namespace cse_vgui1_native {
enum class Result { Closed, Busy };
Result CloseEmbedded() {
    ++fontCalls;
    return fontReady ? Result::Closed : Result::Busy;
}
} // namespace cse_vgui1_native
static bool CseSteamRetireModuleCallbacks() {
    ++callbackCalls;
    return callbacksReady;
}
namespace cse::software_lifetime {
bool RetireRemoteAccess() noexcept {
    ++remoteCalls;
    return remoteReady;
}
} // namespace cse::software_lifetime
#include "terminal_adapter.inc"
extern "C" const Api CseNativeSoftwareLifetimeV1;
static bool exclusive = false;
static bool Exclusive(void*, const void*, std::uint64_t) noexcept {
    return exclusive;
}
static void NetworkLog(ESteamNetworkingSocketsDebugOutputType, const char*) {}
int main(int argc, char** argv) {
    LinkedSocketOwner socketOwner;
    assert(argc == 2);
    const bool dedicated = std::strcmp(argv[1], "dedicated") == 0;
    nativeSoftwareClientEntered = std::strcmp(argv[1], "client-run") == 0;
    const Mode mode = dedicated ? Mode::Dedicated : Mode::Client;
    Binding binding{sizeof(Binding), Version, 1, _dyld_get_image_header(0), &exclusive, Exclusive, NetworkLog, mode, 0};
    assert(CseNativeSoftwareLifetimeV1.bind(&binding) == Result::Ready);
    assert(!RetireResources(mode) && identityCalls == 0 && nativeDedicatedServer.calls == 0);
    exclusive = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive);
    assert(audioCalls == 0 && fontCalls == 0 && callbackCalls == 0 && remoteCalls == 0);
    if (dedicated) {
        assert(nativeDedicatedServer.calls == 1 && identityCalls == 0 && listenCalls == 0);
        nativeDedicatedServer.ready = true;
    } else {
        assert(identityCalls == (nativeSoftwareClientEntered ? 0 : 1));
        identityReady = true;
        assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive && audioCalls == 0);
        listenReady = true;
    }
    host_initialized = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive && audioCalls == 0);
    host_initialized = false;
    videomode = &exclusive;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive && audioCalls == 0);
    videomode = nullptr;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive && fontCalls == 0);
    audioReady = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive && callbackCalls == 0);
    fontReady = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive && remoteCalls == 0);
    callbacksReady = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive);
    remoteReady = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::Retired);
    assert(!Enter(mode));
    const int prior = remoteCalls;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::Retired && remoteCalls == prior);
    std::printf(
        "PASS actual staged terminal adapter %s: each retained owner blocks later cleanup; retry and rejection\n",
        argv[1]);
}
