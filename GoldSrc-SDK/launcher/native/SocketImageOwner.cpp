#include "SocketImageOwner.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace cse::socket_lifetime {
struct Registry {
    static std::atomic_flag gate;
    static std::atomic<pid_t> origin;
    static std::atomic<bool> installed;
    static SocketImageOwner* owners;
    static SocketImageOwner* descriptors[FD_SETSIZE];
    static std::uint64_t epoch;

    static void Lock() noexcept {
        while (gate.test_and_set(std::memory_order_acquire)) {
        }
    }

    static void Unlock() noexcept {
        gate.clear(std::memory_order_release);
    }

    static void Prepare() noexcept {
        Lock();
    }

    static void Parent() noexcept {
        Unlock();
    }

    static void Child() noexcept {
        const int saved = errno;
        // Only permanent POD and raw owned descriptors. Never touch owner nodes or engine memory here.
        for (int descriptor = 1; descriptor < FD_SETSIZE; ++descriptor) {
            if (descriptors[descriptor]) {
                ::close(descriptor);
                descriptors[descriptor] = nullptr;
            }
        }
        owners = nullptr;
        Unlock();
        errno = saved;
    }

    static SocketImageOwner* Find(void* context, const void* image, std::uint64_t identity) noexcept {
        for (SocketImageOwner* owner = owners; owner; owner = owner->next_) {
            // Compare the opaque label before accessing only known registered executable-owned nodes.
            if (owner == context && owner->base_ == image && owner->epoch_ == identity &&
                owner->pid_ == origin.load() && owner->anchor_ && owner->api_) {
                return owner;
            }
        }
        return nullptr;
    }

    static bool Empty(SocketImageOwner* owner) noexcept {
        for (const auto* descriptorOwner : descriptors) {
            if (descriptorOwner == owner) {
                return false;
            }
        }
        return true;
    }

    static void Remove(SocketImageOwner* owner) noexcept {
        SocketImageOwner** position = &owners;
        while (*position && *position != owner) {
            position = &(*position)->next_;
        }
        if (*position) {
            *position = owner->next_;
        }
        owner->next_ = nullptr;
    }
};
std::atomic_flag Registry::gate = ATOMIC_FLAG_INIT;
std::atomic<pid_t> Registry::origin{0};
std::atomic<bool> Registry::installed{false};
SocketImageOwner* Registry::owners = nullptr;
SocketImageOwner* Registry::descriptors[FD_SETSIZE]{};
std::uint64_t Registry::epoch = 0;
static_assert(std::atomic<pid_t>::is_always_lock_free);

namespace {
class Transaction {
public:
    Transaction() noexcept {
        valid_ = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &previous_) == 0;
        if (valid_) {
            AcquireGate();
        }
    }
    ~Transaction() noexcept {
        if (valid_) {
            ReleaseGate();
            pthread_setcancelstate(previous_, nullptr);
        }
    }
    explicit operator bool() const noexcept {
        return valid_;
    }
    void ReleaseGate() noexcept {
        if (locked_) {
            Registry::Unlock();
            locked_ = false;
        }
    }
    void AcquireGate() noexcept {
        Registry::Lock();
        locked_ = true;
    }
private:
    bool locked_ = false;
    int previous_ = PTHREAD_CANCEL_ENABLE;
    bool valid_ = false;
};

Status CurrentProcess() noexcept {
    const pid_t origin = Registry::origin.load();
    if (!origin) {
        return Status::MissingOwner;
    }
    if (origin != getpid()) {
        return Status::WrongProcess;
    }
    if (!Registry::installed.load()) {
        return Status::MissingOwner;
    }
    return pthread_main_np() ? Status::Ready : Status::WrongThread;
}

bool SameImage(const void* address, const void* base) noexcept {
    Dl_info image{};
    return address && dladdr(address, &image) && image.dli_fbase == base;
}
}

bool InitializeProcess() noexcept {
    if (!pthread_main_np()) {
        return false;
    }
    const pid_t origin = Registry::origin.load();
    if (origin) {
        return origin == getpid() && Registry::installed.load();
    }
    // Publish process identity before registration: a concurrent child must never mint new entitlement.
    // Admission still requires the successful registration witness, not this in-progress identity.
    Registry::origin.store(getpid());
    // Never hold our gate across registration's libc handler lock.
    if (pthread_atfork(Registry::Prepare, Registry::Parent, Registry::Child) != 0) {
        Registry::origin.store(0);
        return false;
    }
    Registry::installed.store(true);
    return true;
}

bool HostOwns(void* context, const void* image, std::uint64_t epoch) noexcept {
    if (CurrentProcess() != Status::Ready) {
        return false;
    }
    Transaction transaction;
    return transaction && Registry::Find(context, image, epoch);
}

Status HostCreateUdp(void* context, const void* image, std::uint64_t epoch, CreateResult* output) noexcept {
    if (!output) {
        return Status::Invalid;
    }
    *output = {-1, 0};
    const Status current = CurrentProcess();
    if (current != Status::Ready) {
        return current;
    }
    Transaction transaction;
    if (!transaction) {
        return Status::NativeFailure;
    }
    SocketImageOwner* owner = Registry::Find(context, image, epoch);
    if (!owner) {
        return Status::MissingOwner;
    }
    if (owner->retiring_ || owner->fault_) {
        return owner->fault_ ? Status::NativeFailure : Status::Retired;
    }
    int descriptor = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor < 0) {
        output->nativeError = errno;
        return Status::NativeFailure;
    }
    if (descriptor == 0) {
        descriptor = ::fcntl(0, F_DUPFD, 1);
        const int error = errno;
        if (::close(0) != 0) {
            owner->fault_ = true;
        }
        if (descriptor < 0) {
            output->nativeError = error;
            return Status::NativeFailure;
        }
    }
    int error = 0;
    if (descriptor >= FD_SETSIZE) {
        error = EMFILE;
    } else if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) {
        error = errno;
    } else if (owner->fault_) {
        error = EIO;
    }
    if (error) {
        if (::close(descriptor) != 0) {
            owner->fault_ = true;
        }
        output->nativeError = error;
        return Status::NativeFailure;
    }
    // Publication and creation share the fork gate; no socket-to-registration window exists.
    Registry::descriptors[descriptor] = owner;
    output->descriptor = descriptor;
    return Status::Ready;
}

Status HostCloseUdp(void* context, const void* image, std::uint64_t epoch, std::int32_t descriptor,
                    CloseResult* output) noexcept {
    if (!output) {
        return Status::Invalid;
    }
    *output = {CloseDisposition::NotAttempted, 0};
    const Status current = CurrentProcess();
    if (current != Status::Ready) {
        return current;
    }
    Transaction transaction;
    if (!transaction) {
        return Status::NativeFailure;
    }
    SocketImageOwner* owner = Registry::Find(context, image, epoch);
    if (!owner) {
        return Status::MissingOwner;
    }
    if (owner->retiring_) {
        return Status::Retired;
    }
    if (descriptor <= 0 || descriptor >= FD_SETSIZE || Registry::descriptors[descriptor] != owner) {
        return Status::Invalid;
    }
    const int result = ::close(descriptor);
    const int error = result == 0 ? 0 : errno;
    // Darwin removes a valid unguarded fd before underlying close errors. Cancellation is disabled.
    // Never retain a consumed numeric fd for retry or for an atfork close after descriptor reuse.
    Registry::descriptors[descriptor] = nullptr;
    *output = {CloseDisposition::Consumed, error};
    if (result != 0) {
        owner->fault_ = true;
        return Status::NativeFailure;
    }
    return Status::Ready;
}

Status HostQuiescent(void* context, const void* image, std::uint64_t epoch) noexcept {
    const Status current = CurrentProcess();
    if (current != Status::Ready) {
        return current;
    }
    Transaction transaction;
    if (!transaction) {
        return Status::NativeFailure;
    }
    SocketImageOwner* owner = Registry::Find(context, image, epoch);
    if (!owner) {
        return Status::MissingOwner;
    }
    if (owner->fault_) {
        return Status::NativeFailure;
    }
    return Registry::Empty(owner) ? Status::Ready : Status::LiveSockets;
}

constexpr HostTable CanonicalHost{sizeof(HostTable), Version, HostContract,
                                  HostOwns, HostCreateUdp, HostCloseUdp, HostQuiescent};
static_assert(std::is_same_v<decltype(&HostOwns), OwnsFn>);
static_assert(std::is_same_v<decltype(&HostCreateUdp), CreateUdpFn>);
static_assert(std::is_same_v<decltype(&HostCloseUdp), CloseUdpFn>);
static_assert(std::is_same_v<decltype(&HostQuiescent), QuiescentFn>);
static_assert(CanonicalHost.owns == &HostOwns && CanonicalHost.createUdp == &HostCreateUdp &&
              CanonicalHost.closeUdp == &HostCloseUdp && CanonicalHost.quiescent == &HostQuiescent);

SocketImageOwner::~SocketImageOwner() noexcept {
    assert(!anchor_);
}

bool SocketImageOwner::Active() const noexcept {
    return anchor_ != nullptr;
}

bool SocketImageOwner::Open(void* admittedImage, const char* path) noexcept {
    if (CurrentProcess() != Status::Ready || anchor_ || !admittedImage || !path) {
        return false;
    }
    const auto* api = static_cast<const API*>(dlsym(admittedImage, Symbol));
    const void* factory = dlsym(admittedImage, "CreateInterface");
    Dl_info image{};
    if (!factory || !dladdr(factory, &image) || !SameImage(api, image.dli_fbase) ||
        api->size != sizeof(API) || api->version != Version || api->contract != Contract ||
        !SameImage(reinterpret_cast<const void*>(api->bind), image.dli_fbase) ||
        !SameImage(reinterpret_cast<const void*>(api->retire), image.dli_fbase)) {
        return false;
    }
    void* anchor = dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    if (!anchor) {
        return false;
    }
    if (dlsym(anchor, Symbol) != api || Registry::epoch == (std::numeric_limits<std::uint64_t>::max)()) {
        // Even a rejected, unbound reference remains owned if its checked native close fails.
        anchor_ = anchor;
        Retire();
        return false;
    }
    // Loader verification is complete before the gate. Callbacks inspect only this established state.
    anchor_ = anchor;
    base_ = image.dli_fbase;
    api_ = api;
    epoch_ = ++Registry::epoch;
    pid_ = getpid();
    bound_ = false;
    fault_ = false;
    retiring_ = false;
    {
        Transaction transaction;
        if (!transaction) {
            api_ = nullptr;
        } else {
            next_ = Registry::owners;
            Registry::owners = this;
        }
    }
    const Binding binding{sizeof(Binding), Version, epoch_, base_, this, pid_, 0,
                          CanonicalHost.owns, CanonicalHost.createUdp, CanonicalHost.closeUdp, CanonicalHost.quiescent};
    if (api_ && api_->bind(&binding) == Status::Ready) {
        bound_ = true;
        return true;
    }
    // No binding was published. Retire still owns rollback if the native anchor close fails.
    Retire();
    return false;
}

bool SocketImageOwner::Retire() noexcept {
    if (!anchor_) {
        return true;
    }
    if (CurrentProcess() != Status::Ready || (bound_ && (!api_ || api_->retire() != Status::Retired))) {
        return false;
    }
    Transaction transaction;
    if (!transaction || fault_ || !Registry::Empty(this)) {
        return false;
    }
    retiring_ = true;
    transaction.ReleaseGate();
    // Keep cancellation disabled, but never hold the fork gate across loader code.
    // Another real runtime root remains held by the embedding consumer until this return.
    if (dlclose(anchor_) != 0) {
        return false;
    }
    // Reacquisition cannot fail after the reference is consumed; no stale anchor retry is possible.
    transaction.AcquireGate();
    Registry::Remove(this);
    anchor_ = nullptr;
    api_ = nullptr;
    base_ = nullptr;
    bound_ = false;
    return true;
}
} // namespace cse::socket_lifetime

extern "C" __attribute__((visibility("default"))) const cse::socket_lifetime::HostTable CseNativeSocketHostV1 =
    cse::socket_lifetime::CanonicalHost;
