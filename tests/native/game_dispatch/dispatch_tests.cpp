#include "pr_dlls.h"
#include "native_module.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extensiondll_t g_rgextdll[MAX_EXT_DLLS]{};
int g_iextdllMac = 0;

#include "actual_dispatch.inc"

namespace {
void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(EXIT_FAILURE);
    }
}
using CountFunction = int(*)();
CountFunction Counter(CseGameLibraryHandle module) {
    return reinterpret_cast<CountFunction>(cse_platform::GetSymbol(module, "EntityCalls"));
}
}

int main(int count, char** paths) {
    Check(count == 3, "Two owned modules required");
    Check(GetDispatch("Dispatch") == nullptr && GetEntityInit("Entity") == nullptr, "Empty lookup differs");
    CseGameLibraryHandle first = static_cast<CseGameLibraryHandle>(cse_platform::LoadModule(paths[1]));
    CseGameLibraryHandle second = static_cast<CseGameLibraryHandle>(cse_platform::LoadModule(paths[2]));
    Check(first && second && first != second, "Owned module load failed");
    g_rgextdll[0].lDLLHandle = first;
    g_rgextdll[1].lDLLHandle = second;
    g_iextdllMac = 2;
    const DISPATCHFUNCTION dispatch = GetDispatch("Dispatch");
    const ENTITYINIT entity = GetEntityInit("Entity");
    Check(dispatch && entity && Counter(first) && Counter(second), "Known exports missing");
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        // dlopen handles may be small opaque tokens. Callable addresses are the
        // actual high-address boundary, rather than the OS's handle encoding.
        Check(reinterpret_cast<std::uintptr_t>(dispatch) > UINT32_MAX
            && reinterpret_cast<std::uintptr_t>(entity) > UINT32_MAX, "High callable addresses required");
    }
    int result = 0;
    dispatch(nullptr, &result);
    entity(nullptr);
    Check(result == 100 && Counter(first)() == 1 && Counter(second)() == 0, "First-module precedence differs");
    Check(GetDispatch("missing") == nullptr && GetEntityInit("missing") == nullptr, "Missing symbol accepted");
    GetDispatch("OnlySecond")(nullptr, &result);
    Check(result == 300, "Missing first export did not fall through to second module");
    g_rgextdll[0].lDLLHandle = second;
    g_rgextdll[1].lDLLHandle = first;
    GetDispatch("Dispatch")(nullptr, &result);
    GetEntityInit("Entity")(nullptr);
    Check(result == 500 && Counter(first)() == 1 && Counter(second)() == 1, "Reordered-module lookup differs");
    g_iextdllMac = 1;
    g_rgextdll[0].lDLLHandle = first;
    GetEntityInit("Entity")(nullptr);
    Check(Counter(first)() == 2 && Counter(second)() == 1, "Bounded module count ignored");
    g_iextdllMac = 0;
    Check(GetEntityInit("Entity") == nullptr, "Cleared registry still resolves entities");
    cse_platform::UnloadModule(second);
    cse_platform::UnloadModule(first);
    std::puts("PASS actual typed entity/dispatch lookup, ordered owned modules and high addresses");
}
