#include <windows.h>
#include <cstdio>
#include <cstdlib>
static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "module-check:%d\n", line);
        std::abort();
    }
}
#define CHECK(condition) Check((condition), __LINE__)
static void Cycle(const char *path) {
    HMODULE module = LoadLibraryA(path);
    CHECK(module);
    auto start = reinterpret_cast<bool (*)()>(GetProcAddress(module, "CseStart"));
    auto stop = reinterpret_cast<bool (*)()>(GetProcAddress(module, "CseStop"));
    auto reads = reinterpret_cast<unsigned int (*)()>(GetProcAddress(module, "CseReads"));
    CHECK(start && stop && reads && start());
    const ULONGLONG begin = GetTickCount64();
    while (reads() == 0 && GetTickCount64() - begin < 2000) {
        SwitchToThread();
    }
    CHECK(reads() != 0 && stop() && stop());
    CHECK(FreeLibrary(module));
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    Cycle(argv[1]); // Warm the process runtime before checking handle ownership.
    DWORD before = 0;
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &before));
    for (int index = 0; index < 20; ++index) {
        Cycle(argv[1]);
    }
    DWORD after = 0;
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
    CHECK(before == after);
    std::puts("module-load-start-stop-unload=21 handles-retained=0 fake-cursor-only");
}
