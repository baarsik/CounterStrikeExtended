#include "native_software_lifetime.h"
#include "native_executable_image.h"
#include "../common/TextEntryClipboard.h"
#include <dlfcn.h>
#include <mutex>
#include <limits>
#include <pthread.h>

namespace cse::software_lifetime {
namespace {
enum class Phase { Unbound, Open, Closing, Retired };
std::mutex gate;
Phase phase = Phase::Unbound;
Binding owner{};
pthread_t ownerThread{};
unsigned int calls = 0;
bool retiring = false;

Result Bind(const Binding* binding) noexcept {
    std::lock_guard<std::mutex> lock(gate);
    Dl_info image{};
    if (!pthread_main_np() || !binding || binding->structBytes != sizeof(Binding) || binding->version != Version ||
        binding->epoch == 0 || !binding->context || binding->reserved != 0 ||
        (binding->mode != Mode::Client && binding->mode != Mode::Dedicated) ||
        dladdr(reinterpret_cast<const void*>(&Bind), &image) == 0 || binding->imageBase != image.dli_fbase) {
        return Result::Invalid;
    }
    const void* main = MainExecutableImage(reinterpret_cast<const void*>(binding->exclusive));
    if (!main || MainExecutableImage(reinterpret_cast<const void*>(binding->networkLog)) != main) {
        return Result::Invalid;
    }
    if (phase != Phase::Unbound) {
        return phase == Phase::Retired ? Result::Retired : Result::Busy;
    }
    owner = *binding;
    ownerThread = pthread_self();
    phase = Phase::Open;
    return Result::Ready;
}

bool Exclusive() noexcept {
    return owner.exclusive && owner.exclusive(owner.context, owner.imageBase, owner.epoch);
}

#if defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) && CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1
bool RegistryQuiescent(void* context, const void* identity) {
    std::lock_guard<std::mutex> lock(gate);
    return context == &owner && identity == cse_textentry_clipboard::NativePasteFrameIdentity() &&
           phase == Phase::Closing && retiring && calls == 0 && pthread_equal(ownerThread, pthread_self()) &&
           Exclusive();
}
#endif

Result Retire() noexcept {
    {
        std::lock_guard<std::mutex> lock(gate);
        if (phase == Phase::Unbound) {
            return Result::MissingOwner;
        }
        if (!pthread_equal(ownerThread, pthread_self())) {
            return Result::WrongThread;
        }
        if (phase == Phase::Retired) {
            return Result::Retired;
        }
        if (calls != 0 || retiring) {
            return Result::Busy;
        }
        phase = Phase::Closing;
        if (!Exclusive()) {
            return Result::MissingOwner;
        }
        retiring = true;
    }
    Result result = Result::ResourcesLive;
    try {
        if (RetireResources(owner.mode)) {
            result = Result::Retired;
#if defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) && CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1
            using namespace cse_textentry_clipboard;
            constexpr std::uint32_t QuiescenceVersion = 1;
            NativeFrameQuiescence proof{sizeof(NativeFrameQuiescence), QuiescenceVersion, &owner,
                                        NativePasteFrameIdentity(), RegistryQuiescent};
            if (RetireNativePasteFrames(proof) != NativeFrameRetireResult::Retired) {
                result = Result::RegistryFailure;
            }
#endif
        }
    } catch (...) {
        // Cleanup may be retried; the host must retain the image and the binding on failure.
    }
    std::lock_guard<std::mutex> lock(gate);
    retiring = false;
    if (result == Result::Retired) {
        phase = Phase::Retired;
    }
    return result;
}
} // namespace

bool Enter(Mode mode, EntryKind kind) noexcept {
    std::lock_guard<std::mutex> lock(gate);
    if (phase != Phase::Open || owner.mode != mode || (kind == EntryKind::Outermost && calls != 0) ||
        !pthread_equal(ownerThread, pthread_self()) || calls == (std::numeric_limits<unsigned int>::max)()) {
        return false;
    }
    ++calls;
    return true;
}

bool EnterCurrent() noexcept {
    std::lock_guard<std::mutex> lock(gate);
    if (phase != Phase::Open || !pthread_equal(ownerThread, pthread_self()) ||
        calls == (std::numeric_limits<unsigned int>::max)()) {
        return false;
    }
    ++calls;
    return true;
}

void Leave() noexcept {
    std::lock_guard<std::mutex> lock(gate);
    if (calls != 0 && pthread_equal(ownerThread, pthread_self())) {
        --calls;
    }
}

bool RetirementInProgress() noexcept {
    std::lock_guard<std::mutex> lock(gate);
    return phase == Phase::Closing && retiring && calls == 0 && pthread_equal(ownerThread, pthread_self()) &&
           Exclusive();
}

FSteamNetworkingSocketsDebugOutput HostNetworkLog() noexcept {
    std::lock_guard<std::mutex> lock(gate);
    return phase == Phase::Open ? owner.networkLog : nullptr;
}
} // namespace cse::software_lifetime

extern "C" __attribute__((visibility("default"))) const cse::software_lifetime::Api CseNativeSoftwareLifetimeV1 = {
    sizeof(cse::software_lifetime::Api), cse::software_lifetime::Version, cse::software_lifetime::Contract,
    cse::software_lifetime::Bind, cse::software_lifetime::Retire};
