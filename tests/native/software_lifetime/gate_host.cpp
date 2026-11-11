#include "public/native_software_lifetime.h"
#include <cassert>
#include <cstdio>
#include <dlfcn.h>
#include <thread>
using namespace cse::software_lifetime;
struct Owner {
    void* anchor = nullptr;
    const void* imageBase = nullptr;
    bool admissionClosed = false;
    unsigned int calls = 0;
};
static bool Exclusive(void* context, const void* image, std::uint64_t epoch) noexcept {
    const auto& owner = *static_cast<Owner*>(context);
    return owner.anchor && owner.imageBase == image && epoch == 1 && owner.admissionClosed && owner.calls == 0;
}
static void NetworkLog(ESteamNetworkingSocketsDebugOutputType, const char*) {}
int main(int argc, char** argv) {
    assert(argc == 2);
    Owner owner;
    owner.anchor = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    assert(owner.anchor);
    const auto* api = static_cast<const Api*>(dlsym(owner.anchor, Symbol));
    assert(api && api->version == Version && api->contract == Contract && api->structBytes == sizeof(Api));
    Dl_info image{};
    assert(dladdr(api, &image));
    owner.imageBase = image.dli_fbase;
    const auto enter = reinterpret_cast<bool (*)()>(dlsym(owner.anchor, "EnterClient"));
    const auto leave = reinterpret_cast<void (*)()>(dlsym(owner.anchor, "LeaveClient"));
    const auto ready = reinterpret_cast<void (*)(bool)>(dlsym(owner.anchor, "SetResourcesReady"));
    const auto frame = reinterpret_cast<bool (*)()>(dlsym(owner.anchor, "StartFrame"));
    const auto endFrame = reinterpret_cast<void (*)()>(dlsym(owner.anchor, "EndFrame"));
    const auto lateFrame = reinterpret_cast<bool (*)()>(dlsym(owner.anchor, "LateFrameAccepted"));
    assert(enter && leave && ready && !enter());
    Binding binding{sizeof(Binding), Version + 1, 1, owner.imageBase, &owner, Exclusive, NetworkLog, Mode::Client, 0};
    assert(api->bind(&binding) == Result::Invalid);
    binding.version = Version;
    binding.imageBase = &owner;
    assert(api->bind(&binding) == Result::Invalid);
    binding.imageBase = owner.imageBase;
    binding.exclusive = nullptr;
    assert(api->bind(&binding) == Result::Invalid);
    binding.exclusive = Exclusive;
    binding.networkLog = nullptr;
    assert(api->bind(&binding) == Result::Invalid);
    binding.networkLog = NetworkLog;
    binding.mode = static_cast<Mode>(0);
    assert(api->bind(&binding) == Result::Invalid);
    binding.mode = Mode::Client;
    assert(api->bind(&binding) == Result::Ready);
    if (frame) {
        assert(endFrame && lateFrame && frame());
    }
    assert(enter());
    owner.calls = 1;
    assert(api->retire() == Result::Busy);
    leave();
    owner.calls = 0;
    std::thread foreign([&]() { assert(api->retire() == Result::WrongThread); });
    foreign.join();
    assert(api->retire() == Result::MissingOwner);
    assert(!enter());
    owner.admissionClosed = true;
    assert(api->retire() == Result::ResourcesLive);
    ready(true);
    if (frame) {
        assert(api->retire() == Result::RegistryFailure && !enter());
        endFrame();
    }
    assert(api->retire() == Result::Retired);
    if (lateFrame) {
        assert(!lateFrame());
    }
    assert(api->retire() == Result::Retired && !enter());
    assert(dlclose(owner.anchor) == 0);
    owner.anchor = nullptr;
    std::puts(
        "PASS actual typed lifetime gate: real anchor, busy/thread/proof/resource rejection, retry, terminal entry");
}
