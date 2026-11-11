#include "launcher/native/SocketImageOwner.h"
#include "launcher/native/SoftwareImageOwner.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <exception>

static void Require(bool condition, const char* text) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::abort();
    }
}
int main(int argc, char** argv) {
    Require(cse::socket_lifetime::InitializeProcess(), "permanent executable infrastructure before leaf load");
    void* image = dlopen(CSE_ENTRY_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    Require(image != nullptr, "actual controlled Run/producer leaf");
    cse::socket_lifetime::SocketImageOwner sockets;
    cse::software_lifetime::ImageOwner software;
    Require(sockets.Open(image, CSE_ENTRY_PROVIDER), "real socket anchor and reverse canonical admission");
    Require(software.Open(image, CSE_ENTRY_PROVIDER, cse::software_lifetime::Mode::Client),
            "genuine software executable owner, never a substitute ownership witness");
    const auto run = reinterpret_cast<int (*)(int, char**)>(dlsym(image, "CseTestEntryRun"));
    Require(run != nullptr, "actual extracted Run entry");
    const auto previousTerminate = std::get_terminate();
    int result = 0;
    {
        cse::software_lifetime::ImageOwner::Call call(&software);
        Require(static_cast<bool>(call), "complete host call ownership");
        result = run(argc, argv);
    }
    std::set_terminate(previousTerminate);
    Require(software.Retire() && sockets.Retire(), "real leaf gates retire only after complete call returns");
    Require(dlclose(image) == 0, "normal leaf image release");
    return result;
}
