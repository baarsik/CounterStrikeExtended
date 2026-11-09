#include <dlfcn.h>
#include <cerrno>

namespace {
int failures = 0;
void (*probe)() = nullptr;
}

void FailNextRelease() {
    ++failures;
}

void ProbeDuringRelease(void (*value)()) {
    probe = value;
}

extern "C" int CseFixtureDlclose(void* reference) {
    if (probe) {
        probe();
    }
    if (failures) {
        --failures;
        errno = EAGAIN;
        return -1; // Failure BEFORE real dlclose: ownership is retained, never forged successful.
    }
    return dlclose(reference);
}
