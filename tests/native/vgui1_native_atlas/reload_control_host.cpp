#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <cstdio>
#include <cstring>

namespace {
bool IsMapped(const char* path) {
    for (unsigned int index = 0; index < _dyld_image_count(); ++index) {
        if (std::strcmp(path, _dyld_get_image_name(index)) == 0) {
            return true;
        }
    }
    return false;
}
}

int main(int argc, char** argv) {
    if (argc != 2 || IsMapped(argv[1])) {
        return 1;
    }
    constexpr int CycleCount = 4;
    constexpr int ExpectedValue = 73;
    for (int cycle = 0; cycle < CycleCount; ++cycle) {
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!image || !IsMapped(argv[1])) {
            return 2;
        }
        const auto value = reinterpret_cast<int (*)()>(dlsym(image, "ReloadControlValue"));
        if (!value || value() != ExpectedValue || dlclose(image) != 0 || IsMapped(argv[1])) {
            return 3;
        }
        std::printf("PASS independent virtual-class actual unload cycle=%d\n", cycle + 1);
        std::fflush(stdout);
    }
    return 0;
}
