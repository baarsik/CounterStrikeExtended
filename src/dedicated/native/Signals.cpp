#include "Signals.h"
#include <cerrno>
namespace cse::dedicated {
volatile sig_atomic_t ShutdownRequested = 0;
namespace {
constexpr int ManagedSignals[] = {SIGINT, SIGTERM, SIGPIPE};
void Stop(int) {
    ShutdownRequested = 1;
}
} // namespace
bool SignalScope::Install() noexcept {
    if (active_) {
        return true;
    }
    if (!installer_) {
        errno = EINVAL;
        return false;
    }
    active_ = true;
    previousRequest_ = ShutdownRequested;
    ShutdownRequested = 0;
    for (int signal : ManagedSignals) {
        struct sigaction action{};
        action.sa_handler = signal == SIGPIPE ? SIG_IGN : Stop;
        sigemptyset(&action.sa_mask);
        if (installer_(signal, &action, &previous_[installed_]) != 0) {
            const int error = errno;
            Restore();
            errno = error;
            return false;
        }
        ++installed_;
    }
    return true;
}
void SignalScope::Restore() noexcept {
    if (!active_) {
        return;
    }
    const int error = errno;
    while (installed_ > 0) {
        --installed_;
        installer_(ManagedSignals[installed_], &previous_[installed_], nullptr);
    }
    ShutdownRequested = previousRequest_;
    active_ = false;
    errno = error;
}
} // namespace cse::dedicated
