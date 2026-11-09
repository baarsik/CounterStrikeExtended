#include <cstdio>
#include <dlfcn.h>

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        return 2;
    }
    for (int cycle = 0; cycle < 3; ++cycle) {
        std::printf("leaf load %d\n", cycle);
        std::fflush(stdout);
        void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!module) {
            std::fprintf(stderr, "%s\n", dlerror());
            return 3;
        }
        void* consumers[2]{};
        if (argc == 4) {
            for (int index = 0; index < 2; ++index) {
                consumers[index] = dlopen(argv[index + 2], RTLD_NOW | RTLD_LOCAL);
                if (!consumers[index]) {
                    return 6;
                }
                auto read = reinterpret_cast<int (*)()>(dlsym(consumers[index], "CseFixtureConsumerValue"));
                if (!read || read() != 17) {
                    return 7;
                }
            }
        }
        auto value = reinterpret_cast<int (*)()>(dlsym(module, "CseFixtureLeafValue"));
        if (!value || value() != 17) {
            return 4;
        }
        if (argc == 4 && (dlclose(consumers[0]) != 0 || dlclose(consumers[1]) != 0)) {
            return 8;
        }
        if (dlclose(module) != 0) {
            return 9;
        }
    }
    std::puts("leaf reload passed");
}
