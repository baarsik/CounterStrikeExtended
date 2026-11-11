#include "engine/native_software_lifetime.h"
#include "public/native_runtime_host.h"
#include <cassert>
#include <cstdio>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <thread>
using namespace cse::software_lifetime;
static bool resourcesReady = false;
static bool closed = false;
static int opens = 0;
static int closes = 0;
static int invalidations = 0;
static bool finishReady = true;
namespace cse::software_lifetime {
bool RetireResources(Mode) {
    return resourcesReady;
}
} // namespace cse::software_lifetime
namespace cse {
bool InitializeNativeGameUIHost();
bool NativeGameUIIsMainThread();
bool NativeGameUIIsRetirementSafePoint();
void EnterNativeGameUIUse();
void LeaveNativeGameUIUse();
bool EnterNativeGameUIRetirement();
void LeaveNativeGameUIRetirement();
bool FinishNativeListenShutdown() {
    if (!finishReady || !EnterNativeGameUIRetirement()) {
        return false;
    }
    LeaveNativeGameUIRetirement();
    return true;
}
struct Mouse {
    bool Invalidate(void*) {
        ++invalidations;
        return true;
    }
};
Mouse& NativeMouseWindowState() {
    static Mouse mouse;
    return mouse;
}
} // namespace cse
namespace {
std::thread::id nativeGameUIThread;
size_t nativeGameUIUseDepth = 0;
bool nativeGameUIRetiring = false;
} // namespace
#include "game_identity.inc"
namespace cse_vgui1_native {
enum class Result { Ready = 1, Closed = 2, Busy = -1, InvalidArgument = -2 };
struct BindingV2 {
    std::uint64_t epoch;
    const char* enginePath;
    const char* fontPath;
    cse_vgui1_host::HostReleasesV2 host;
};
Result ConfigureEmbedded(const BindingV2&) {
    assert(cse::NativeGameUIIsMainThread());
    ++opens;
    return Result::Ready;
}
Result CloseEmbedded() {
    ++closes;
    return Result::Closed;
}
} // namespace cse_vgui1_native
#include "bootstrap.inc"
extern "C" const Api CseNativeSoftwareLifetimeV1;
static bool Exclusive(void*, const void*, std::uint64_t) noexcept {
    return closed;
}
static void NetworkLog(ESteamNetworkingSocketsDebugOutputType, const char*) {}
int main() {
    using RootResult = cse_vgui1_native::Result;
    Binding binding{sizeof(Binding), Version,      1, _dyld_get_image_header(0), &opens, Exclusive,
                    NetworkLog,      Mode::Client, 0};
    assert(CseNativeSoftwareLifetimeV1.bind(&binding) == Result::Ready);
    cse::native_runtime::BindingV1 root{sizeof(root), cse::native_runtime::BindingVersion, 1, "engine", "font", {}};
    assert(CseNativeRuntimeOpenV1(nullptr) == static_cast<int>(RootResult::InvalidArgument));
    {
        Entry active(Mode::Client);
        assert(active);
        assert(CseNativeRuntimeOpenV1(&root) == static_cast<int>(RootResult::Busy) && opens == 0);
    }
    std::thread foreign([&]() {
        assert(CseNativeRuntimeOpenV1(&root) == static_cast<int>(RootResult::Busy) && opens == 0);
        assert(CseNativeRuntimeCloseV1() == static_cast<int>(RootResult::Busy) && closes == 0);
    });
    foreign.join();
    assert(CseNativeRuntimeOpenV1(&root) == static_cast<int>(RootResult::Ready) && opens == 1);
    cse::EnterNativeGameUIUse();
    assert(CseNativeRuntimeOpenV1(&root) == static_cast<int>(RootResult::Busy) && opens == 1);
    cse::LeaveNativeGameUIUse();
    // No CEngineAPI::Run occurred: explicit admitted Open must establish the reviewed thread identity.
    assert(CseNativeRuntimeCloseV1() == static_cast<int>(RootResult::Closed));
    closes = invalidations = 0;
    closed = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::ResourcesLive);
    assert(CseNativeRuntimeOpenV1(&root) == static_cast<int>(RootResult::Busy) && opens == 1);
    resourcesReady = true;
    assert(CseNativeSoftwareLifetimeV1.retire() == Result::Retired);
    assert(CseNativeRuntimeOpenV1(&root) == static_cast<int>(RootResult::Busy) && opens == 1);
    finishReady = false;
    assert(CseNativeRuntimeCloseV1() == static_cast<int>(RootResult::Busy) && invalidations == 0 && closes == 0);
    finishReady = true;
    assert(CseNativeRuntimeCloseV1() == static_cast<int>(RootResult::Closed) && closes == 1);
    assert(CseNativeRuntimeCloseV1() == static_cast<int>(RootResult::Closed) && closes == 2);
    std::puts("PASS actual bootstrap: nested/wrong-thread/closing/retired Open blocked; reviewed Close still balances");
}
