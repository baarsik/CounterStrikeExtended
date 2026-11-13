#include "basefilesystem.h"
#include "native_path_prefix.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/mount.h>

namespace {
constexpr long ActualCapability = -2;
constexpr long UnknownCapability = -1;
constexpr long CaseInsensitive = 0;
constexpr long CaseSensitive = 1;
long capability = CaseInsensitive;
int capabilityError = ENOTSUP;
bool forbidEnumeration = true;
std::vector<std::string> queries;
std::vector<std::string> enumerations;
std::string sensitiveDirectory;
std::string ownedHome;
}

long FixturePathconf(const char* path, int name);
DIR* FixtureOpendir(const char* path);
char* FixtureGetenv(const char* name);

// Compile the complete production owner. Only the OS capability query and
// enumeration entry point are controlled; actual file syscalls stay native.
#define pathconf FixturePathconf
#define opendir FixtureOpendir
#define getenv FixtureGetenv
#include "../../../src/filesystem/pathmatch.cpp"
#undef getenv
#undef opendir
#undef pathconf

long FixturePathconf(const char* path, int name) {
    assert(name == _PC_CASE_SENSITIVE);
    queries.emplace_back(path);
    if (capability == UnknownCapability) {
        errno = capabilityError;
        return UnknownCapability;
    }
    const long actual = pathconf(path, name);
    if (actual == UnknownCapability || capability == ActualCapability) {
        return actual;
    }
    if (!sensitiveDirectory.empty()) {
        char* resolved = realpath(path, nullptr);
        const bool sensitive = resolved && sensitiveDirectory == resolved;
        std::free(resolved);
        return sensitive ? CaseSensitive : CaseInsensitive;
    }
    return capability;
}

DIR* FixtureOpendir(const char* path) {
    enumerations.emplace_back(path);
    if (forbidEnumeration) {
        errno = EACCES;
        return nullptr;
    }
    return opendir(path);
}

char* FixtureGetenv(const char* name) {
    if (std::strcmp(name, "HOME") == 0) {
        return const_cast<char*>(ownedHome.c_str());
    }
    return nullptr;
}

namespace {
void Write(const std::filesystem::path& path) {
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file);
    std::fputs("fixture", file);
    std::fclose(file);
}

void Missing(const std::filesystem::path& path, int expectedError = ENOENT) {
    struct stat info{};
    errno = 0;
    assert(__wrap_stat(path.c_str(), &info) == -1);
    assert(errno == expectedError);
}

void RequireNoEnumeration() {
    if (!enumerations.empty()) {
        std::fprintf(stderr, "unexpected ancestor enumeration: %s\n", enumerations.front().c_str());
    }
    assert(enumerations.empty());
}
}

int main(int argc, char** argv) {
    assert(argc == 3);
    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    const std::string scenario = argv[2];
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Contents/MacOS");
    ownedHome = (root / "isolated-home").string();
    const auto parent = root / "Contents/MacOS";
    const auto missing = parent / "pak0.pak";
    if (scenario == "missing-pack") {
        Missing(missing);
        RequireNoEnumeration();
        assert(queries == std::vector<std::string>{parent.string()});
        // The resolver does not claim existence and does not fabricate a match.
        char buffer[FS_PATH_MATCH_BUFFER_SIZE]{};
        char* matched = buffer;
        assert(ResolveCaseInsensitivePath(missing.c_str(), &matched, false, buffer, sizeof(buffer)) == kPathUnchanged);
        assert(matched == nullptr);
        char finalPath[FS_MAX_PATH_LENGTH]{};
        auto original = missing.string();
        assert(SteamRealPath(original.data(), finalPath, sizeof(finalPath)) == nullptr);
        assert(errno == ENOENT);
        RequireNoEnumeration();
    } else if (scenario == "create-rename") {
        const auto original = parent / "MixedBasename.TXT";
        FILE* created = __wrap_fopen(original.c_str(), "wb");
        assert(created);
        std::fclose(created);
        const auto renamed = parent / "RenamedCase.DAT";
        assert(__wrap_rename(original.c_str(), renamed.c_str()) == 0);
        assert(std::filesystem::directory_iterator(parent)->path().filename() == "RenamedCase.DAT");
        RequireNoEnumeration();
        assert(queries == std::vector<std::string>({parent.string(), parent.string()}));
    } else if (scenario == "sensitive-fallback" || scenario == "unknown-fallback" || scenario == "denied-capability") {
        capability = scenario == "sensitive-fallback" ? CaseSensitive : UnknownCapability;
        capabilityError = scenario == "denied-capability" ? EACCES : ENOTSUP;
        Missing(missing);
        assert(!enumerations.empty());
        assert(queries == std::vector<std::string>{parent.string()});
    } else if (scenario == "nested-missing" || scenario == "not-directory") {
        std::filesystem::path unresolvableParent;
        int expectedError = ENOENT;
        if (scenario == "nested-missing") {
            unresolvableParent = parent / "absent/child";
        } else {
            Write(parent / "regular-file");
            unresolvableParent = parent / "regular-file/child";
            expectedError = ENOTDIR;
        }
        Missing(unresolvableParent / "pak0.pak", expectedError);
        assert(!enumerations.empty());
        // Never query a distant existing ancestor and infer a different volume.
        assert(queries == std::vector<std::string>{unresolvableParent.string()});
    } else if (scenario == "symlink-capability") {
        std::filesystem::create_directories(root / "sensitive-volume");
        sensitiveDirectory = std::filesystem::canonical(root / "sensitive-volume").string();
        std::filesystem::create_directory_symlink(root / "sensitive-volume", root / "parent-link");
        Missing(root / "parent-link/pak0.pak");
        assert(!enumerations.empty());
        assert(queries == std::vector<std::string>{(root / "parent-link").string()});
        enumerations.clear();
        queries.clear();
        Missing(missing);
        RequireNoEnumeration();
        assert(queries == std::vector<std::string>{parent.string()});
    } else if (scenario == "dot-parent-volume") {
        std::filesystem::create_directories(root / "other-volume/container");
        std::filesystem::create_directories(root / "other-volume/Contents/MacOS");
        sensitiveDirectory = std::filesystem::canonical(root / "other-volume/Contents/MacOS").string();
        std::filesystem::create_directory_symlink(root / "other-volume/container", root / "parent-link");
        const auto traversedParent = root / "parent-link/../Contents/MacOS";
        Missing(traversedParent / "pak0.pak");
        assert(!enumerations.empty());
        assert(queries == std::vector<std::string>{traversedParent.string()});
    } else if (scenario == "capability-change") {
        Missing(missing);
        RequireNoEnumeration();
        capability = CaseSensitive;
        Missing(missing);
        assert(!enumerations.empty());
        assert(queries == std::vector<std::string>({parent.string(), parent.string()}));
        Write(missing);
        struct stat info{};
        assert(__wrap_stat(missing.c_str(), &info) == 0);
        assert(info.st_size == static_cast<off_t>(std::strlen("fixture")));
    } else if (scenario == "fallback-create") {
        capability = CaseSensitive;
        forbidEnumeration = false;
        const auto original = parent / "FallbackNew.TXT";
        FILE* created = __wrap_fopen(original.c_str(), "wb");
        assert(created);
        std::fclose(created);
        assert(std::filesystem::directory_iterator(parent)->path().filename() == "FallbackNew.TXT");
        assert(!enumerations.empty());
        assert(queries == std::vector<std::string>{parent.string()});
    } else if (scenario == "relative-parent") {
        const auto savedDirectory = std::filesystem::current_path();
        std::filesystem::current_path(parent);
        Missing("pak0.pak");
        RequireNoEnumeration();
        assert(queries == std::vector<std::string>{"."});
        std::filesystem::current_path(savedDirectory);
    } else if (scenario == "native-errors") {
        std::filesystem::create_symlink("absent-target", parent / "DanglingLink");
        struct stat info{};
        assert(__wrap_lstat((parent / "DanglingLink").c_str(), &info) == 0 && S_ISLNK(info.st_mode));
        Missing(parent / "DanglingLink");
        std::filesystem::create_symlink("Loop", parent / "Loop");
        Missing(parent / "Loop", ELOOP);
        Write(parent / "regular-file");
        Missing(parent / "regular-file/leaf", ENOTDIR);
        RequireNoEnumeration();
    } else if (scenario == "exact-priority") {
        Write(parent / "ExactCase.TXT");
        capability = CaseSensitive;
        struct stat info{};
        assert(__wrap_stat((parent / "ExactCase.TXT").c_str(), &info) == 0);
        assert(queries.empty());
        RequireNoEnumeration();
    } else if (scenario == "host-volume") {
        capability = ActualCapability;
        const long actual = pathconf(parent.c_str(), _PC_CASE_SENSITIVE);
        std::printf("actual Darwin parent capability=%ld\n", actual);
        Missing(missing);
        if (actual == CaseInsensitive) {
            RequireNoEnumeration();
        } else {
            assert(!enumerations.empty());
        }
        assert(queries == std::vector<std::string>{parent.string()});
    } else {
        assert(false);
    }
    ClearPathMatchCache();
    std::filesystem::remove_all(root);
    std::printf("PASS actual pathmatch owner: %s queries=%zu enumerations=%zu\n", scenario.c_str(),
        queries.size(), enumerations.size());
}
