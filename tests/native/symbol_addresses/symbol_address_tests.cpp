#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
#if !defined(_WIN32)
#include <dlfcn.h>
#endif
#include "../../../GoldSrc-SDK/engine/pr_dlls.h"

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../GoldSrc-SDK/common/const.h"
#include "../../../GoldSrc-SDK/engine/progdefs.h"
#include "../../../GoldSrc-SDK/engine/edict.h"
#include "../../../GoldSrc-SDK/engine/eiface.h"
#include "../strings/high_offset_buffer.h"

static_assert(sizeof(functiontable_t::pFunction) == sizeof(std::uintptr_t), "Export addresses must be native width");
static_assert(std::is_same<decltype(enginefuncs_t::pfnFunctionFromName), std::uintptr_t (*)(const char *)>::value,
    "Engine symbol lookup must return the full native address");
static_assert(std::is_same<decltype(enginefuncs_t::pfnNameForFunction), const char *(*)(std::uintptr_t)>::value,
    "Engine reverse lookup must accept the full native address");

extensiondll_t g_rgextdll[MAX_EXT_DLLS]{};
int g_iextdllMac = 0;

namespace {
unsigned int diagnostics = 0;
void Con_Printf(const char *, ...) {
    ++diagnostics;
}

#if defined(_WIN32)
std::vector<char> temporary;
void FS_GetLocalCopy(const char *) {}
void Con_DPrintf(const char *, ...) {}
void *Hunk_TempAlloc(int bytes) {
    temporary.resize(static_cast<std::size_t>(bytes));
    return temporary.data();
}
void *Mem_Malloc(std::size_t bytes) {
    return std::malloc(bytes);
}
#endif
} // namespace

#include "../../../GoldSrc-SDK/engine/symbol_lookup_impl.h"
#if defined(_WIN32)
#include "../../../GoldSrc-SDK/engine/export_table_impl.h"
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Symbol address check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (false)

namespace {
bool CheckNames() {
    char output[64]{};
    if (!cse::symbols::CopyExportName("?Think@OwnedSymbolFixture@@SAHH@Z", output, sizeof(output))
        || std::strcmp(output, "Think@OwnedSymbolFixture") != 0
        || !cse::symbols::CopyItaniumName("_ZN18OwnedSymbolFixture5ThinkEi", output, sizeof(output))
        || std::strcmp(output, "Think@OwnedSymbolFixture") != 0) {
        return false;
    }
    for (const char *name : {"", "?", "?Think", "?Think@Class", "?@Class@@"}) {
        if (cse::symbols::CopyExportName(name, output, sizeof(output))) {
            return false;
        }
    }
    for (const char *name : {"", "_Z", "_ZN", "_ZN0A0B", "_ZN99999999999999999999999A1B", "_ZN1A99B"}) {
        if (cse::symbols::CopyItaniumName(name, output, sizeof(output))) {
            return false;
        }
    }
    char exact[sizeof("Think@OwnedSymbolFixture")];
    return cse::symbols::CopyExportName("?Think@OwnedSymbolFixture@@SAHH@Z", exact, sizeof(exact))
        && !cse::symbols::CopyExportName("?Think@OwnedSymbolFixture@@SAHH@Z", exact, sizeof(exact) - 1)
        && cse::symbols::CopyItaniumName("_ZN18OwnedSymbolFixture5ThinkEi", exact, sizeof(exact))
        && !cse::symbols::CopyItaniumName("_ZN18OwnedSymbolFixture5ThinkEi", exact, sizeof(exact) - 1);
}
} // namespace

int main(int argc, char **argv) {
#if defined(_WIN32)
    if (argc == 3 && std::strcmp(argv[1], "--table") == 0) {
        extensiondll_t dll{};
        dll.lDLLHandle = LoadLibraryA(argv[2]);
        CHECK(dll.lDLLHandle);
        BuildExportTable(&dll, argv[2]);
        CHECK(dll.functionCount > 0);
        for (int index = 0; index < dll.functionCount; ++index) {
            CHECK(dll.functionTable[index].pFunction != 0);
            std::puts(dll.functionTable[index].pFunctionName);
        }
        std::free(dll.functionTable);
        CHECK(FreeLibrary(dll.lDLLHandle));
        return 0;
    }
#endif
    CHECK(argc == 2);
    CHECK(CheckNames());
#if defined(_WIN32)
    const auto library = LoadLibraryA(argv[1]);
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    CHECK(library);
    auto &dll = g_rgextdll[0];
    dll.lDLLHandle = library;
    g_iextdllMac = 1;
    const auto address = FindNameInTable(&dll, "CseSymbolFixtureFunction");
    CHECK(address != 0);
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(address > UINT32_MAX);
    }
    using Function = int (*)(int);
    CHECK(reinterpret_cast<Function>(address)(40) == 41);
    enginefuncs_t callbacks{};
    callbacks.pfnFunctionFromName = FunctionFromName;
    callbacks.pfnNameForFunction = NameForFunction;
#if defined(_WIN32)
    BuildExportTable(&dll, argv[1]);
    CHECK(dll.functionCount == 1);
    const auto method = callbacks.pfnFunctionFromName("Think@OwnedSymbolFixture");
    CHECK(method != 0);
    CHECK(reinterpret_cast<Function>(method)(40) == 42);
    CHECK(std::strcmp(callbacks.pfnNameForFunction(method), "Think@OwnedSymbolFixture") == 0);
    CHECK(callbacks.pfnFunctionFromName("_ZN18OwnedSymbolFixture5ThinkEi") == method);
    std::free(dll.functionTable);
    // Distinct addresses with identical low 32 bits must never alias.
    HighOffsetBuffer high(nullptr);
    CHECK(high.get());
    auto first = reinterpret_cast<CseSymbolAddress>(high.get());
    auto second = first;
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        second += static_cast<CseSymbolAddress>(UINT64_C(1) << 32);
    } else {
        ++second;
    }
    char firstName[] = "First@Owned";
    char secondName[] = "Second@Owned";
    functiontable_t table[] = {{first, firstName}, {second, secondName}};
    dll.functionTable = table;
    dll.functionCount = 2;
    CHECK(callbacks.pfnFunctionFromName(firstName) == first);
    CHECK(callbacks.pfnFunctionFromName(secondName) == second);
    CHECK(callbacks.pfnNameForFunction(first) == firstName);
    CHECK(callbacks.pfnNameForFunction(second) == secondName);
#else
    CHECK(callbacks.pfnFunctionFromName("CseSymbolFixtureFunction") == address);
    CHECK(std::strcmp(callbacks.pfnNameForFunction(address), "CseSymbolFixtureFunction") == 0);
#endif
    CHECK(FindNameInTable(&dll, "CseMissingSymbol") == 0);
    CHECK(callbacks.pfnFunctionFromName("Missing@Owned") == 0);
    CHECK(callbacks.pfnNameForFunction(0) == nullptr);
    CHECK(diagnostics == 2);
    dll.functionTable = nullptr;
    dll.functionCount = 0;
    g_iextdllMac = 0;
    CHECK(callbacks.pfnNameForFunction(address) == nullptr);
#if defined(_WIN32)
    CHECK(FreeLibrary(library));
#else
    CHECK(dlclose(library) == 0);
#endif
    std::puts("Native symbol addresses, actual export calls and canonical names passed");
    return 0;
}
