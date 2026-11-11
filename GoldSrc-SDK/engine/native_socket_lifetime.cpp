#include "native_socket_lifetime.h"
#include <atomic>
#include <cerrno>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mutex>
#include <pthread.h>
#include <unistd.h>

bool NET_NativeSocketsQuiescent();

namespace cse::socket_lifetime {
namespace {
enum class Phase { Unbound, Open, Closing, Retired };
std::mutex gate;
std::atomic<pid_t> origin{0};
Binding owner{};
Phase phase = Phase::Unbound;
unsigned int calls = 0;
bool retiring = false;

bool SameImage(const void* address, const void* base) noexcept {
    Dl_info image{};
    return address && dladdr(address, &image) && image.dli_fbase == base;
}

Status Bind(const Binding* binding) noexcept {
    if (!pthread_main_np()) {
        return Status::WrongThread;
    }
    if (!binding || binding->size != sizeof(Binding) || binding->version != Version ||
        binding->reserved || !binding->epoch || !binding->context) {
        return Status::Invalid;
    }
    if (binding->originPID != getpid() || (origin.load() && origin.load() != getpid())) {
        return Status::WrongProcess;
    }
    if (!SameImage(reinterpret_cast<const void*>(&Bind), binding->imageBase)) {
        return Status::Invalid;
    }
    const auto* canonical = static_cast<const HostTable*>(dlsym(RTLD_MAIN_ONLY, HostSymbol));
    const void* main = _dyld_get_image_header(0);
    if (!SameImage(canonical, main) || canonical->size != sizeof(HostTable) || canonical->version != Version ||
        canonical->contract != HostContract || !SameImage(reinterpret_cast<const void*>(canonical->owns), main) ||
        !SameImage(reinterpret_cast<const void*>(canonical->createUdp), main) ||
        !SameImage(reinterpret_cast<const void*>(canonical->closeUdp), main) ||
        !SameImage(reinterpret_cast<const void*>(canonical->quiescent), main) ||
        binding->owns != canonical->owns || binding->createUdp != canonical->createUdp ||
        binding->closeUdp != canonical->closeUdp || binding->quiescent != canonical->quiescent) {
        return Status::Invalid;
    }
    {
        std::lock_guard<std::mutex> lock(gate);
        if (phase != Phase::Unbound) {
            return phase == Phase::Retired ? Status::Retired : Status::Busy;
        }
    }
    // All four roles are admitted before this first foreign invocation; no producer lock is held.
    if (!canonical->owns(binding->context, binding->imageBase, binding->epoch)) {
        return Status::MissingOwner;
    }
    std::lock_guard<std::mutex> lock(gate);
    owner = *binding;
    origin.store(getpid());
    phase = Phase::Open;
    return Status::Ready;
}

class Call {
public:
    Call() noexcept {
        // Child rejection must precede touching inherited mutexes or callback contexts.
        if (origin.load() != getpid() || !pthread_main_np()) {
            return;
        }
        std::lock_guard<std::mutex> lock(gate);
        if (phase == Phase::Open && calls == 0 && !retiring) {
            ++calls;
            entered_ = true;
        }
    }
    ~Call() noexcept {
        if (entered_) {
            std::lock_guard<std::mutex> lock(gate);
            --calls;
        }
    }
    explicit operator bool() const noexcept {
        return entered_;
    }
private:
    bool entered_ = false;
};
} // namespace

bool Admitted() noexcept {
    Call call;
    return static_cast<bool>(call);
}

int CreateUdp() noexcept {
    Call call;
    if (!call) {
        errno = EPERM;
        return -1;
    }
    CreateResult result{-1, 0};
    const Status status = owner.createUdp(owner.context, owner.imageBase, owner.epoch, &result);
    if (status != Status::Ready) {
        errno = result.nativeError ? result.nativeError : EPERM;
        return -1;
    }
    return result.descriptor;
}

CloseResult CloseUdp(int descriptor) noexcept {
    CloseResult result{CloseDisposition::NotAttempted, 0};
    Call call;
    if (!call) {
        result.nativeError = EPERM;
        return result;
    }
    const Status status = owner.closeUdp(owner.context, owner.imageBase, owner.epoch, descriptor, &result);
    if (status != Status::Ready) {
        errno = result.nativeError ? result.nativeError : EPERM;
    }
    return result;
}

Status Retire() noexcept {
    if (origin.load() != getpid()) {
        return origin.load() ? Status::WrongProcess : Status::MissingOwner;
    }
    if (!pthread_main_np()) {
        return Status::WrongThread;
    }
    {
        std::lock_guard<std::mutex> lock(gate);
        if (phase == Phase::Retired) {
            return Status::Retired;
        }
        if (calls || retiring) {
            return Status::Busy;
        }
        retiring = true;
    }
    const Status result = NET_NativeSocketsQuiescent()
        ? owner.quiescent(owner.context, owner.imageBase, owner.epoch) : Status::LiveSockets;
    std::lock_guard<std::mutex> lock(gate);
    retiring = false;
    if (result == Status::Ready) {
        phase = Phase::Retired;
        return Status::Retired;
    }
    // A live-socket rejection must still permit the actual shutdown path to close it and retry.
    return result;
}
} // namespace cse::socket_lifetime

extern "C" __attribute__((visibility("default"))) const cse::socket_lifetime::API CseNativeSocketLifetimeV1 = {
    sizeof(cse::socket_lifetime::API), cse::socket_lifetime::Version, cse::socket_lifetime::Contract,
    cse::socket_lifetime::Bind, cse::socket_lifetime::Retire};
