#include "game_library_name.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    using Platform = cse::GameLibraryPlatform;
    Check(std::strcmp(cse::GameLibraryMetadataKey(Platform::MacOS), "gamedll_osx") == 0, "Wrong macOS key");
    Check(std::strcmp(cse::GameLibraryMetadataKey(Platform::Linux), "gamedll_linux") == 0, "Wrong Linux key");
    struct Case {
        const char* input;
        const char* expected;
        Platform platform;
        bool legacyLinux;
    };
    const Case cases[]{
        {"dlls\\mp.dll", "dlls/mp.dylib", Platform::MacOS, false},
        {"dlls/cs.dylib", "dlls/cs.dylib", Platform::MacOS, false},
        {"DLLS/HL.DLL", "DLLS/HL.dylib", Platform::MacOS, false},
        {"mods/custom_dir/mod_debug.dll", "mods/custom_dir/mod_debug.dylib", Platform::MacOS, false},
        {"dlls/cs_amd64.so", "dlls/cs.so", Platform::Linux, true},
        {"dlls/extra_mod.dll", "dlls/extra_mod.so", Platform::Linux, false},
        {"dlls/hl.so", "dlls/hl.so", Platform::Linux, true},
        {"dlls/archive.v2.dll", "dlls/archive.v2.dylib", Platform::MacOS, false}
    };
    for (const Case& entry : cases) {
        char name[128];
        std::memcpy(name, entry.input, std::strlen(entry.input) + 1);
        Check(cse::NormalizeGameLibraryName(name, sizeof(name), entry.platform, entry.legacyLinux),
            "Valid game library name rejected");
        Check(std::strcmp(name, entry.expected) == 0, "Native library name differs");
    }
    for (const char* input : {"dlls/cs.so", "folder.dll/client", "no_extension", ""}) {
        char name[128];
        char before[128];
        std::memset(name, 0xa5, sizeof(name));
        std::memcpy(name, input, std::strlen(input) + 1);
        std::memcpy(before, name, sizeof(name));
        Check(!cse::NormalizeGameLibraryName(name, sizeof(name), Platform::MacOS, false),
            "Foreign or absent extension accepted");
        Check(std::memcmp(name, before, sizeof(name)) == 0, "Rejected name was mutated");
    }
    char shortName[] = "1234567.dll";
    Check(!cse::NormalizeGameLibraryName(shortName, sizeof(shortName), Platform::MacOS, false),
        "Expansion exceeded capacity");
    Check(std::strcmp(shortName, "1234567.dll") == 0, "Overflow mutated input");
    char unterminated[8];
    std::memset(unterminated, 'a', sizeof(unterminated));
    Check(!cse::NormalizeGameLibraryName(unterminated, sizeof(unterminated), Platform::MacOS, false),
        "Unterminated name accepted");
    Check(!cse::NormalizeGameLibraryName(nullptr, 0, Platform::MacOS, false), "Null buffer accepted");
    cse::GameLibraryFallbacks<2, 32> fallbacks;
    Check(fallbacks.UseFallbacks() && fallbacks.Count() == 0, "Wrong initial metadata state");
    Check(fallbacks.Remember("dlls/one.dll") && fallbacks.Remember("dlls/two.dll"), "Fallback not retained");
    Check(!fallbacks.Remember("dlls/three.dll") && fallbacks.Overflowed(), "Fallback capacity not bounded");
    Check(std::strcmp(fallbacks.Name(0), "dlls/one.dll") == 0
        && std::strcmp(fallbacks.Name(1), "dlls/two.dll") == 0, "Fallback order differs");
    Check(fallbacks.Name(2) == nullptr, "Fallback accessor exceeded count");
    fallbacks.MarkPrimary();
    Check(!fallbacks.UseFallbacks(), "Windows fallback overrides explicit native metadata");
    cse::GameLibraryFallbacks<2, 8> bounded;
    Check(!bounded.Remember("12345678") && bounded.Count() == 0, "Oversize fallback stored");
    std::puts("PASS platform metadata keys, native extensions, Windows fallback names and bounded mutation");
}
