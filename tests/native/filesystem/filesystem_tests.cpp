#include "basefilesystem.h"
#include "../public/native_module.h"
#include "../../../src/engine/map_catalog.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#if defined(__APPLE__)
extern IFileSystem* g_pFileSystem;
void FS_Rename(const char* originalName, const char* newName);
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void WriteFile(const std::filesystem::path& path, const std::string& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Require(output.good(), "write owned fixture file");
}

void Write32(std::ofstream& output, std::uint32_t value) {
    constexpr int bitsPerByte = 8;
    constexpr std::size_t diskIntegerBytes = 4;
    unsigned char bytes[diskIntegerBytes] = {};
    for (std::size_t index = 0; index < diskIntegerBytes; ++index) {
        bytes[index] = static_cast<unsigned char>(value >> (index * bitsPerByte));
    }
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void WritePack(const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string>>& entries) {
    constexpr std::uint32_t headerBytes = 12;
    constexpr std::uint32_t directoryRecordBytes = 64;
    constexpr std::size_t diskNameBytes = 56;
    std::uint32_t directoryOffset = headerBytes;
    for (const auto& entry : entries) {
        directoryOffset += static_cast<std::uint32_t>(entry.second.size());
    }
    std::ofstream output(path, std::ios::binary);
    output.write("PACK", 4);
    Write32(output, directoryOffset);
    Write32(output, static_cast<std::uint32_t>(entries.size()) * directoryRecordBytes);
    for (const auto& entry : entries) {
        output.write(entry.second.data(), static_cast<std::streamsize>(entry.second.size()));
    }
    std::uint32_t position = headerBytes;
    for (const auto& entry : entries) {
        char name[diskNameBytes] = {};
        Require(entry.first.size() < sizeof(name), "fixture pack name fits unchanged disk record");
        std::memcpy(name, entry.first.data(), entry.first.size());
        output.write(name, sizeof(name));
        Write32(output, position);
        Write32(output, static_cast<std::uint32_t>(entry.second.size()));
        position += static_cast<std::uint32_t>(entry.second.size());
    }
    Require(output.good(), "write literal little-endian PACK fixture");
}

void IgnoreWarning(const char*, ...) {}

class Module {
public:
    explicit Module(const char* path)
        : module_(cse_platform::LoadModule(path, cse_platform::ModuleFallback::None)), path_(path) {
        Require(module_ != nullptr, "load actual filesystem module");
        const auto factory = reinterpret_cast<CreateInterfaceFn>(
            cse_platform::GetSymbol(module_, cse_platform::InterfaceFactorySymbol));
        Require(factory != nullptr, "actual module exports CreateInterface");
        int code = IFACE_FAILED;
        filesystem = static_cast<IFileSystem*>(factory("VFileSystem009", &code));
        Require(filesystem && code == IFACE_OK, "get actual filesystem interface");
        Require(factory("VFileSystemThatDoesNotExist", &code) == nullptr && code == IFACE_FAILED,
            "factory rejects unknown interface version");
        filesystem->SetWarningFunc(IgnoreWarning);
        filesystem->Mount();
    }

    ~Module() {
        filesystem->RemoveAllSearchPaths();
        filesystem->Unmount();
        cse_platform::UnloadModule(module_);
        Require(cse_platform::FindLoadedModule(path_) == nullptr, "actual filesystem module unloads after final release");
    }

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    IFileSystem* filesystem = nullptr;

private:
    void* module_;
    const char* path_;
};

std::string Read(IFileSystem* filesystem, const char* name, const char* pathID = nullptr) {
    FileHandle_t file = filesystem->Open(name, "rb", pathID);
    Require(file != nullptr, "open virtual file");
    if (sizeof(void*) == 8) {
        Require(reinterpret_cast<std::uintptr_t>(file) > UINT32_MAX, "file handle retains native high address");
    }
    const unsigned int length = filesystem->Size(file);
    std::string bytes(length, '\0');
    Require(filesystem->Read(bytes.data(), static_cast<int>(bytes.size()), file) == static_cast<int>(bytes.size()),
        "read complete virtual file");
    filesystem->Close(file);
    return bytes;
}

void TestSearch(IFileSystem* filesystem, const std::filesystem::path& root) {
    const auto game = root / "game";
    const auto fallback = root / "fallback";
    WriteFile(game / "same.txt", "game");
    WriteFile(fallback / "same.txt", "fallback");
    WriteFile(fallback / "fallback.txt", "fallback-only");
    filesystem->AddSearchPath(game.string().c_str(), "GAME");
    filesystem->AddSearchPath(fallback.string().c_str(), "FALLBACK");
    filesystem->AddSearchPath(game.string().c_str(), "GAME");
    Require(Read(filesystem, "same.txt") == "game", "first path wins; duplicate insertion is harmless");
    Require(Read(filesystem, "same.txt", "FALLBACK") == "fallback", "explicit ID selects fallback");
    Require(Read(filesystem, "fallback.txt") == "fallback-only", "missing game file falls through to fallback");
    Require(filesystem->Open("fallback.txt", "rb", "GAME") == nullptr, "GAME ID excludes fallback paths");
    Require(filesystem->RemoveSearchPath(game.string().c_str()), "remove game search path");
    Require(Read(filesystem, "same.txt") == "fallback", "removing first path exposes fallback");
    filesystem->RemoveAllSearchPaths();
    Require(!filesystem->FileExists("same.txt"), "remove all paths clears lookup");
}

void TestFiles(IFileSystem* filesystem, const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    filesystem->AddSearchPath(root.string().c_str(), "GAME");
    filesystem->CreateDirHierarchy("created/sub", "GAME");
    FileHandle_t file = filesystem->Open("created/sub/output.txt", "wb", "GAME");
    Require(file != nullptr, "write through game search path");
    Require(filesystem->Write("first", 5, file) == 5, "write returns byte count");
    filesystem->Flush(file);
    filesystem->Close(file);
    file = filesystem->Open("created/sub/output.txt", "ab", "GAME");
    Require(file && filesystem->Write("-last", 5, file) == 5, "append through VFS");
    filesystem->Close(file);
    Require(Read(filesystem, "created/sub/output.txt") == "first-last", "append retains original bytes");
    file = filesystem->Open("created/sub/output.txt", "rb", "GAME");
    filesystem->Seek(file, 2, FILESYSTEM_SEEK_HEAD);
    Require(filesystem->Tell(file) == 2, "seek from head");
    filesystem->Seek(file, 1, FILESYSTEM_SEEK_CURRENT);
    Require(filesystem->Tell(file) == 3, "seek from current position");
    filesystem->Seek(file, -4, FILESYSTEM_SEEK_TAIL);
    char bytes[4] = {};
    Require(filesystem->Read(bytes, sizeof(bytes), file) == static_cast<int>(sizeof(bytes))
        && std::string(bytes, sizeof(bytes)) == "last", "seek from end returns expected bytes");
    filesystem->Close(file);
    filesystem->RemoveFile("created/sub/output.txt", "GAME");
    Require(!filesystem->FileExists("created/sub/output.txt"), "remove file through VFS");
}

void TestPack(IFileSystem* filesystem, const std::filesystem::path& root) {
    static_assert(sizeof(CBaseFileSystem::packfile_t) == 64);
    static_assert(sizeof(CBaseFileSystem::packheader_t) == 12);
    static_assert(sizeof(CBaseFileSystem::packfile64_t) == 128);
    static_assert(sizeof(CBaseFileSystem::packheader64_t) == 20);
    std::filesystem::create_directories(root);
    WritePack(root / "pak0.pak", {{"same.txt", "pak-zero"}, {"only-zero.txt", "zero"}});
    WritePack(root / "pak1.pak", {{"same.txt", "pak-one"}, {"only-one.txt", "one"}});
    WriteFile(root / "same.txt", "loose");
    filesystem->AddSearchPath(root.string().c_str(), "GAME");
    Require(Read(filesystem, "same.txt") == "loose", "source policy: loose files precede automatic packs");
    Require(Read(filesystem, "only-zero.txt") == "zero", "read literal 32-bit PACK directory");
    Require(Read(filesystem, "only-one.txt") == "one", "later-numbered automatic pack loads");
    std::filesystem::remove(root / "same.txt");
    Require(Read(filesystem, "same.txt") == "pak-one", "higher-numbered pack precedes earlier pack");
    Require(filesystem->Open("only-one.txt", "rb", "GAME") == nullptr,
        "automatic packs retain their existing empty path ID");
    FileHandle_t firstView = filesystem->Open("same.txt", "rb", nullptr);
    FileHandle_t secondView = filesystem->Open("only-one.txt", "rb", nullptr);
    Require(firstView && secondView, "two pack views share the mounted archive stream");
    filesystem->Close(firstView);
    filesystem->Seek(secondView, 0, FILESYSTEM_SEEK_HEAD);
    char remaining[3] = {};
    Require(filesystem->Read(remaining, sizeof(remaining), secondView) == static_cast<int>(sizeof(remaining))
        && std::string(remaining, sizeof(remaining)) == "one", "closing one view retains the peer's pack stream");
    filesystem->Close(secondView);
    filesystem->RemoveAllSearchPaths();
    Require(!filesystem->FileExists("only-one.txt"), "removing mounted archives clears their virtual entries");
    filesystem->AddSearchPath(root.string().c_str(), "GAME");
    Require(Read(filesystem, "only-zero.txt") == "zero", "archive can reopen after all prior views and mounts detach");
}

void TestFind(IFileSystem* filesystem, const std::filesystem::path& root) {
    const auto first = root / "first";
    const auto second = root / "second";
    WriteFile(first / "folder/a.txt", "a");
    WriteFile(first / "folder/same.txt", "first");
    WriteFile(second / "folder/b.txt", "b");
    WriteFile(second / "folder/same.txt", "second");
    filesystem->AddSearchPath(first.string().c_str(), "GAME");
    filesystem->AddSearchPath(second.string().c_str(), "FALLBACK");
    FileFindHandle_t handle = -1;
    std::vector<std::string> names;
    const char* name = filesystem->FindFirst("folder/*.txt", &handle, nullptr);
    while (name) {
        names.emplace_back(name);
        name = filesystem->FindNext(handle);
    }
    filesystem->FindClose(handle);
    for (const auto& found : names) {
        std::printf("Find result: %s\n", found.c_str());
    }
#if !defined(_WIN32)
    Require(names == std::vector<std::string>({"same.txt", "a.txt", "same.txt", "b.txt"}),
        "preserve POSIX reverse-alphabetic traversal and duplicate results across paths");
#else
    std::vector<std::string> expected;
    for (const auto& directory : {first, second}) {
        WIN32_FIND_DATAA data = {};
        const std::string wildcard = (directory / "folder/*.txt").string();
        HANDLE native = ::FindFirstFileA(wildcard.c_str(), &data);
        Require(native != INVALID_HANDLE_VALUE, "enumerate independent native Windows fixture baseline");
        do {
            expected.emplace_back(data.cFileName);
        } while (::FindNextFileA(native, &data));
        ::FindClose(native);
    }
    Require(names == expected && std::count(names.begin(), names.end(), "same.txt") == 2,
        "preserve native Windows order and duplicate results across search paths");
#endif
    name = filesystem->FindFirst("folder/*.txt", &handle, "GAME");
    Require(name != nullptr, "begin path-ID limited search");
    filesystem->FindClose(handle);

    WriteFile(first / "save/level.v2.Hl1", "state");
    WriteFile(first / "save/level.v2.Hl1.bak", "backup");
    name = filesystem->FindFirst("save/*.HL?", &handle, "GAME");
    Require(name && std::string(name) == "level.v2.Hl1", "save enumeration includes dotted map basenames");
    Require(!filesystem->FindNext(handle), "save enumeration excludes backup extensions");
    filesystem->FindClose(handle);
}

void TestCase(IFileSystem* filesystem, const std::filesystem::path& root) {
    WriteFile(root / "MixedDir/Upper.TXT", "mixed");
    WriteFile(root / "CaseTwin.txt", "UPPER-ONLY");
    WriteFile(root / "casetwin.txt", "lower");
    const bool caseSensitive = std::filesystem::file_size(root / "CaseTwin.txt") == 10;
    std::printf("Underlying fixture case-sensitive: %s\n", caseSensitive ? "yes" : "no");
#if defined(__APPLE__)
    Require(caseSensitive, "macOS case qualification requires actual case-sensitive fixture volume");
#endif
    filesystem->AddSearchPath(root.string().c_str(), "GAME");
    Require(Read(filesystem, "MixedDir/Upper.TXT") == "mixed", "exact spelling works");
    char localPath[4096]{};
    Require(filesystem->GetLocalPath("mixeddir/upper.txt", localPath, sizeof(localPath)) != nullptr,
        "resolve the existing loose file using its VFS spelling");
    std::ifstream localFile(localPath, std::ios::binary);
    std::string localContents;
    localFile >> localContents;
    Require(localContents == "mixed", "native consumers can open the path returned by the VFS");
    Require(Read(filesystem, "mixeddir/upper.txt") == "mixed", "existing POSIX fallback descends directories");
    if (caseSensitive) {
        Require(Read(filesystem, "CaseTwin.txt") == "UPPER-ONLY", "exact upper-case file wins before folding");
        Require(Read(filesystem, "casetwin.txt") == "lower", "exact lower-case file remains distinct");
    }
#if defined(__APPLE__)
    WriteFile(root / "CStrike/Maps/de_mixed.BSP", "mixed-case map");
    WritePack(root / "CStrike/pak0.pak", {{"maps/de_packed.bsp", "packed map"}});
    WriteFile(root / "cstrike_other_mod/maps/de_other.bsp", "other mod map");
    const auto activeGamePath = (root / "cstrike").string();
    const auto otherGamePath = (root / "cstrike_other_mod").string();
    filesystem->AddSearchPath(activeGamePath.c_str(), "GAME");
    filesystem->AddSearchPathNoWrite(otherGamePath.c_str(), "GAME_FALLBACK");
    Require(filesystem->GetLocalPath("maps/de_mixed.BSP", localPath, sizeof(localPath)) != nullptr &&
        cse::maps::IsGameMapPath(localPath, root.c_str(), "cstrike", "english"),
        "catalog accepts the mounted game's correctly resolved mixed-case map");
    Require(filesystem->GetLocalPath("maps/de_other.bsp", localPath, sizeof(localPath)) != nullptr &&
        !cse::maps::IsGameMapPath(localPath, root.c_str(), "cstrike", "english"),
        "catalog rejects a fallback game whose directory shares the active name");
    Require(filesystem->GetLocalPath("maps/de_packed.bsp", localPath, sizeof(localPath)) != nullptr &&
        cse::maps::IsGameMapPath(localPath, root.c_str(), "cstrike", "english"),
        "catalog accepts the synthesized map path from the active game's pack");

    WriteFile(root / "cstrike/maps/de_exact.bsp", "exact-case map");
    const auto twinGamePath = (root / "CStrike").string();
    filesystem->AddSearchPathNoWrite(twinGamePath.c_str(), "GAME_FALLBACK");
    Require(filesystem->GetLocalPath("maps/de_mixed.BSP", localPath, sizeof(localPath)) != nullptr &&
        !cse::maps::IsGameMapPath(localPath, root.c_str(), "cstrike", "english"),
        "catalog rejects a distinct case-sensitive sibling of an exact game directory");

    WriteFile(root / "save/quick.SAV", "previous slot");
    WriteFile(root / "custom.hp2", "temporary archive");
    g_pFileSystem = filesystem;
    FS_Rename("SAVE/quick.sav", "SAVE/quick01.sav");
    FS_Rename("custom.hp2", "custom.hpk");
    g_pFileSystem = nullptr;
    Require(!std::filesystem::exists(root / "save/quick.SAV"), "rotate the selected save");
    Require(Read(filesystem, "save/quick01.sav") == "previous slot", "backup uses the actual save directory");
    Require(Read(filesystem, "custom.hpk") == "temporary archive", "rename a root-level temporary HPAK");
#endif
    Require(filesystem->Open("missing.txt", "rb", nullptr) == nullptr, "missing file fails safely");
}

void TestLong(IFileSystem* filesystem, const std::filesystem::path& root) {
    std::filesystem::path directory = root;
#if defined(_WIN32)
    constexpr int segments = 2;
#else
    constexpr int segments = 8;
#endif
    for (int index = 0; index < segments; ++index) {
        directory /= "segment-012345678901234567890123456789";
    }
    WriteFile(directory / "long.txt", "long");
#if !defined(_WIN32)
    Require(directory.string().size() > 260, "POSIX fixture exceeds legacy Windows path size");
#endif
    filesystem->AddSearchPath(directory.string().c_str(), "GAME");
    Require(Read(filesystem, "long.txt") == "long", "open through long search path");
    char local[4096] = {};
    Require(filesystem->GetLocalPath("long.txt", local, sizeof(local)) != nullptr,
        "resolve local path through actual VFS");
    Require(std::filesystem::equivalent(directory / "long.txt", local), "local-path result retains the actual file");
}

void TestHandles(IFileSystem* filesystem, const std::filesystem::path& root) {
    WriteFile(root / "handle.txt", "owned");
    filesystem->AddSearchPath(root.string().c_str(), "GAME");
    FileHandle_t file = filesystem->Open("handle.txt", "rb", nullptr);
    Require(file != nullptr, "open owned native handle");
    filesystem->Close(file);
    filesystem->Close(nullptr);
}

void TestUnloadFind(IFileSystem* filesystem, const std::filesystem::path& root) {
    WriteFile(root / "find/a.txt", "a");
    WriteFile(root / "find/b.txt", "b");
    WriteFile(root / "find/c.txt", "c");
    filesystem->AddSearchPath(root.string().c_str(), "GAME");
    FileFindHandle_t handle = -1;
    Require(filesystem->FindFirst("find/*.txt", &handle, nullptr) != nullptr,
        "module owns an active enumeration when its consumer detaches");
}

}

int main(int argc, char** argv) {
    Require(argc == 4, "module, owned fixture directory and scenario are required");
    const std::filesystem::path root = argv[2];
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const std::string scenario = argv[3];
    if (scenario == "lifecycle") {
        constexpr int repetitions = 64;
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            Module module(argv[1]);
            TestFiles(module.filesystem, root);
        }
    } else if (scenario == "mount-lifecycle") {
        Module module(argv[1]);
        constexpr int repetitions = 64;
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            TestFiles(module.filesystem, root);
            module.filesystem->RemoveAllSearchPaths();
            module.filesystem->Unmount();
            module.filesystem->Mount();
        }
    } else {
        Module module(argv[1]);
        if (scenario == "module") {
            char version[32] = {};
            module.filesystem->GetInterfaceVersion(version, sizeof(version));
            Require(std::strcmp(version, "Stdio") == 0, "actual module identifies stdio backend");
        } else if (scenario == "search") {
            TestSearch(module.filesystem, root);
        } else if (scenario == "files") {
            TestFiles(module.filesystem, root);
        } else if (scenario == "pack") {
            TestPack(module.filesystem, root);
        } else if (scenario == "find") {
            TestFind(module.filesystem, root);
        } else if (scenario == "case") {
            TestCase(module.filesystem, root);
        } else if (scenario == "long") {
            TestLong(module.filesystem, root);
        } else if (scenario == "handles") {
            TestHandles(module.filesystem, root);
        } else if (scenario == "unload-find") {
            TestUnloadFind(module.filesystem, root);
        } else {
            Require(false, "unknown filesystem scenario");
        }
    }
    std::printf("PASS: filesystem %s\n", scenario.c_str());
    return EXIT_SUCCESS;
}
