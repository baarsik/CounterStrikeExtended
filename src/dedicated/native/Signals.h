#pragma once
#include <csignal>

namespace cse::dedicated {
extern volatile sig_atomic_t ShutdownRequested;
class SignalScope {
public:
    using Installer = decltype(&::sigaction);
    explicit SignalScope(Installer installer = ::sigaction) noexcept : installer_(installer) {}
    ~SignalScope() {
        Restore();
    }
    SignalScope(const SignalScope&) = delete;
    SignalScope& operator=(const SignalScope&) = delete;
    bool Install() noexcept;
    void Restore() noexcept;

private:
    static constexpr int SignalCount = 3;
    Installer installer_;
    struct sigaction previous_[SignalCount]{};
    int installed_ = 0;
    sig_atomic_t previousRequest_ = 0;
    bool active_ = false;
};
} // namespace cse::dedicated
