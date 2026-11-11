#include "SocketImageOwner.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
using namespace cse::socket_lifetime;
static void Require(bool condition, const char* text) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::abort();
    }
}
int main(int argc, char** argv) {
    Require(argc == 2 && InitializeProcess(), "explicit process owner");
    void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(image != nullptr, "actual producer");
    SocketImageOwner owner;
    Require(owner.Open(image, argv[1]), "actual host binding");
    const auto* api = static_cast<const API*>(dlsym(image, Symbol));
    const auto state = reinterpret_cast<void (*)(int, int, int)>(dlsym(image, "TestNetworkState"));
    Require(api && state, "actual source owner-state controls");
    for (unsigned int field = 0; field < 3; ++field) {
        state(field == 0, field == 1, field == 2 ? 1 : 0);
        const Status result = api->retire();
        if (result != Status::LiveSockets) {
            std::fprintf(stderr, "RED network owner field=%u terminal_result=%u expected=LiveSockets\n",
                         field, static_cast<unsigned int>(result));
            state(0, 0, 0);
            Require(owner.Retire(), "normal baseline cleanup");
            Require(dlclose(image) == 0, "normal baseline image release");
            return 1;
        }
    }
    state(0, 0, 0);
    Require(owner.Retire() && dlclose(image) == 0, "actual empty network owner terminal retry");
    std::puts("actual_network_worker_config_descriptor_terminal_negatives=3 passed=1");
}
