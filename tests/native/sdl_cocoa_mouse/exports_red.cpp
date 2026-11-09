#include <cstdio>
#include <dlfcn.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        return 18;
    }
    void *provider = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!provider) {
        return 18;
    }
    const char *names[] = {"CseSdl_CheckCocoaMouseOwnedV1", "CseSdl_CheckCocoaRelativeFocusV1",
                           "CseSdl_ReadCocoaMouseAbsoluteV1"};
    for (const char *name : names) {
        if (!dlsym(provider, name)) {
            std::fprintf(stderr, "required-export-absent:%s\n", name);
            dlclose(provider);
            return 17;
        }
    }
    dlclose(provider);
    return 0;
}
