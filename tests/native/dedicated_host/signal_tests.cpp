#include "dedicated/native/Signals.h"
#include <cerrno>
#include <csignal>
static int calls = 0;
static int ControlledInstall(int signal, const struct sigaction* action, struct sigaction* previous) {
    if (++calls == 2) {
        errno = EINVAL;
        return -1;
    }
    return ::sigaction(signal, action, previous);
}
int main() {
    struct sigaction before{};
    struct sigaction after{};
    sigaction(SIGINT, nullptr, &before);
    cse::dedicated::ShutdownRequested = 7;
    {
        cse::dedicated::SignalScope scope(ControlledInstall);
        if (scope.Install()) { return 1; }
    }
    sigaction(SIGINT, nullptr, &after);
    if (before.sa_handler != after.sa_handler || before.sa_flags != after.sa_flags
        || cse::dedicated::ShutdownRequested != 7) { return 2; }
    {
        cse::dedicated::SignalScope scope;
        if (!scope.Install()) { return 3; }
        raise(SIGTERM);
        if (!cse::dedicated::ShutdownRequested) { return 4; }
    }
    return cse::dedicated::ShutdownRequested == 7 ? 0 : 5;
}
