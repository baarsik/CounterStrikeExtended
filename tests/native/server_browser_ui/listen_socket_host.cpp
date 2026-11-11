#include "launcher/native/SocketImageOwner.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}
}

int main(int argc, char** argv) {
    Require(cse::socket_lifetime::InitializeProcess(), "real process installation before controlled leaf load");
    void* image = dlopen(CSE_LISTEN_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    Require(image != nullptr, "actual extracted listen entry and socket producer leaf");
    cse::socket_lifetime::SocketImageOwner owner;
    Require(owner.Open(image, CSE_LISTEN_PROVIDER), "genuine executable anchor and canonical socket admission");
    const auto run = reinterpret_cast<int (*)(int, char**)>(dlsym(image, "CseTestListenRun"));
    Require(run != nullptr, "actual controlled RunListenServer entry");
    const int result = run(argc, argv);
    // The leaf has no actual network worker or sockets; other listen services remain controlled test owners.
    Require(owner.Retire(), "actual socket gate retires after the complete listen call returns");
    Require(dlclose(image) == 0, "normal controlled leaf release");
    return result;
}
