#include <cstdlib>

#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" FIXTURE_EXPORT bool SteamAPI_Init() {
    // Binding may discover this address; an incomplete provider must never be initialized.
    std::abort();
}
