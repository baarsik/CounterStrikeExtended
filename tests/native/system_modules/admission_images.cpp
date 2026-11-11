#include "fixture.hpp"
int main(int argc, char** argv) {
    if (argc != 2) { return 1; }
    void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image) { return 2; }
    auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(image, "CreateInterface"));
    auto setMode = reinterpret_cast<void (*)(int)>(dlsym(image, "SetMode"));
    auto calls = reinterpret_cast<int (*)()>(dlsym(image, "CreateCalls"));
    if (!factory || !setMode || !calls) { return 3; }
    for (int mode = 0; mode <= 10; ++mode) {
        setMode(mode);
        if ((cse::modules::Admit(factory) != nullptr) != (mode == 0)) { return 4; }
        if (calls() != 0) { return 5; }
        if (mode == 0 && !cse::modules::AdmitConsumer(factory)) { return 6; }
        if ((mode == 1 || (mode >= 4 && mode <= 7) || mode == 10)
            && cse::modules::AdmitConsumer(factory)) { return 7; }
    }
    dlclose(image);
    return 0;
}
