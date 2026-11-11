#pragma once
#include "../../public/native_software_lifetime.h"
#include <atomic>
#include <cstdio>
#include <cassert>
#include <dlfcn.h>
#include <limits>
#include <pthread.h>

namespace cse::software_lifetime {
// Main executable owner. Admission must check the complete cohort before Open.
// This independent real dlopen anchor is not the VGUI font-root witness.
class ImageOwner {
public:
    ImageOwner() = default;
    ImageOwner(const ImageOwner&) = delete;
    ImageOwner& operator=(const ImageOwner&) = delete;
    // The embedding host already owns retry/teardown. It must not destroy this
    // callback context until Retire succeeds; no destructor unload or success fallback.
    ~ImageOwner() noexcept {
        assert(!anchor_ && calls_ == 0);
    }

    class Call {
    public:
        explicit Call(ImageOwner* owner) noexcept : owner_(owner), entered_(!owner || owner->Enter()) {}
        ~Call() noexcept {
            if (owner_ && entered_) {
                owner_->Leave();
            }
        }
        explicit operator bool() const noexcept {
            return entered_;
        }
        Call(const Call&) = delete;
        Call& operator=(const Call&) = delete;

    private:
        ImageOwner* owner_;
        bool entered_;
    };

    bool Open(void* admittedImage, const char* path, Mode mode) noexcept {
        if (anchor_ || !admittedImage || !path || !pthread_main_np()) {
            return false;
        }
        const auto* api = static_cast<const Api*>(dlsym(admittedImage, Symbol));
        const void* factory = dlsym(admittedImage, "CreateInterface");
        Dl_info image{};
        if (!api || !factory || !dladdr(factory, &image) || !SameImage(api, image.dli_fbase) ||
            api->structBytes != sizeof(Api) || api->version != Version || api->contract != Contract ||
            !SameImage(reinterpret_cast<const void*>(api->bind), image.dli_fbase) ||
            !SameImage(reinterpret_cast<const void*>(api->retire), image.dli_fbase)) {
            return false;
        }
        void* anchor = dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
        if (!anchor) {
            return false;
        }
        if (dlsym(anchor, Symbol) != api) {
            dlclose(anchor);
            return false;
        }
        // Monotonic process-owner epoch. Exhaustion never wraps into an old binding.
        static std::atomic<std::uint64_t> next{0};
        auto current = next.load();
        do {
            if (current == (std::numeric_limits<std::uint64_t>::max)()) {
                dlclose(anchor);
                return false;
            }
        } while (!next.compare_exchange_weak(current, current + 1));
        anchor_ = anchor;
        base_ = image.dli_fbase;
        epoch_ = current + 1;
        thread_ = pthread_self();
        closing_ = false;
        const Binding binding{sizeof(Binding), Version, epoch_, base_, this, Exclusive, NetworkLog, mode, 0};
        if (api->bind(&binding) != Result::Ready) {
            dlclose(anchor_);
            anchor_ = nullptr;
            base_ = nullptr;
            return false;
        }
        api_ = api;
        return true;
    }

    bool Enter() noexcept {
        if (!anchor_ || closing_ || !pthread_equal(thread_, pthread_self()) ||
            calls_ == (std::numeric_limits<unsigned int>::max)()) {
            return false;
        }
        ++calls_;
        return true;
    }

    void Leave() noexcept {
        if (anchor_ && calls_ && pthread_equal(thread_, pthread_self())) {
            --calls_;
        }
    }

    bool Retire() noexcept {
        if (!anchor_) {
            return true;
        }
        if (!pthread_equal(thread_, pthread_self()) || calls_ != 0) {
            return false;
        }
        closing_ = true;
        const Result result = api_->retire();
        if (result != Result::Retired) {
            std::fprintf(stderr, "Software terminal retirement rejected (%u); owner retained\n",
                         static_cast<unsigned int>(result));
            return false;
        }
        // Final release is in the main image, after the foreign retirement returns.
        if (dlclose(anchor_) != 0) {
            return false;
        }
        anchor_ = nullptr;
        api_ = nullptr;
        base_ = nullptr;
        return true;
    }

    bool Active() const noexcept {
        return anchor_ != nullptr;
    }

private:
    static bool SameImage(const void* address, const void* base) noexcept {
        Dl_info image{};
        return address && dladdr(address, &image) && image.dli_fbase == base;
    }

    static bool Exclusive(void* context, const void* image, std::uint64_t epoch) noexcept {
        const auto& owner = *static_cast<const ImageOwner*>(context);
        return owner.anchor_ && owner.api_ && owner.base_ == image && owner.epoch_ == epoch && owner.closing_ &&
               owner.calls_ == 0 && pthread_equal(owner.thread_, pthread_self());
    }

    static void NetworkLog(ESteamNetworkingSocketsDebugOutputType, const char* message) {
        // Steam may invoke this on its service thread. It retains no engine code or context.
        if (message) {
            std::fprintf(stderr, "[Steam networking] %s\n", message);
        }
    }

    void* anchor_ = nullptr;
    const void* base_ = nullptr;
    const Api* api_ = nullptr;
    std::uint64_t epoch_ = 0;
    pthread_t thread_{};
    unsigned int calls_ = 0;
    bool closing_ = false;
};
} // namespace cse::software_lifetime
