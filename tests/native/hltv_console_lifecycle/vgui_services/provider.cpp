#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int panels = 0;
bool embedded = false;

void Event(const char* name) {
    if (std::strcmp(name, "panel-acquired") == 0) {
        ++panels;
    } else if (std::strcmp(name, "panel-destroyed") == 0) {
        --panels;
    } else if (std::strcmp(name, "panel-embedded") == 0) {
        embedded = true;
    } else if (std::strcmp(name, "panel-detached") == 0) {
        embedded = false;
    }
    std::fprintf(stderr, "VGUI %s panels=%d embedded=%d\n", name, panels, embedded);
}

bool SchemeAvailable() {
    return std::getenv("CSE_CONSOLE_VGUI_FAIL_SCHEME") == nullptr;
}

struct Services {
    void (*event)(const char* name);
    bool (*schemeAvailable)();
} services{Event, SchemeAvailable};

__attribute__((constructor)) void Loaded() {
    Event("provider-load");
}

__attribute__((destructor)) void Unloaded() {
    Event("provider-unload");
}

}

extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char* name, int*) {
    return std::strcmp(name, "ControlledVGuiServices") == 0 ? &services : nullptr;
}
