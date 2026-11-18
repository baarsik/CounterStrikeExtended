#include "basefilesystem.h"
#include "../public/localized_filesystem.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

static void Write(const std::filesystem::path& path, const char* text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
}

static void Number(std::ofstream& file, std::uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) {
        file.put(static_cast<char>(value >> (byte * 8)));
    }
}

static void Pack(const std::filesystem::path& path, const char* name, const char* text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    const auto size = static_cast<std::uint32_t>(std::strlen(text));
    file.write("PACK", 4);
    Number(file, 12 + size);
    Number(file, 64);
    file.write(text, size);
    char entry[56]{};
    std::memcpy(entry, name, std::strlen(name));
    file.write(entry, sizeof(entry));
    Number(file, 12);
    Number(file, size);
}

static std::string Read(IFileSystem* filesystem, const char* name, const char* pathId = nullptr) {
    FileHandle_t file = filesystem->Open(name, "rb", pathId);
    assert(file);
    std::string bytes(filesystem->Size(file), '\0');
    assert(filesystem->Read(&bytes[0], static_cast<int>(bytes.size()), file) == static_cast<int>(bytes.size()));
    filesystem->Close(file);
    return bytes;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path directory = std::filesystem::absolute(argv[1]);
    const std::string base = (directory / "base").string();
    const std::string hd = (directory / "hd").string();
    const std::string polish = (directory / "polish").string();
    const std::string russian = (directory / "russian").string();
    const std::string skin = (directory / "skin").string();
    const std::string initialLanguage = (directory / "french").string();
    const std::string interveningRoot = (directory / "root").string();
    Write(directory / "base/token.txt", "english");
    Write(directory / "hd/token.txt", "hd");
    Write(directory / "polish/token.txt", "polski");
    Write(directory / "russian/token.txt", "russian");
    Write(directory / "skin/skin.txt", "skin");
    Write(directory / "french/overlap.txt", "francais");
    Write(directory / "polish/overlap.txt", "polski");
    Write(directory / "root/overlap.txt", "base");
    Pack(directory / "base/pak0.pak", "held.txt", "0123456789");
    Pack(directory / "polish/pak0.pak", "packed.txt", "polish pack");
    auto* filesystem = static_cast<IFileSystem*>(CreateInterface("VFileSystem009", nullptr));
    const auto* extension = cse::localized_filesystem::Query(CreateInterface);
    assert(filesystem && extension);
    filesystem->Mount();
    extension->BeginGroup(1);
    filesystem->AddSearchPathNoWrite(initialLanguage.c_str(), "GAME");
    extension->EndGroup();
    filesystem->AddSearchPath(interveningRoot.c_str(), "BASE");
    filesystem->AddSearchPathNoWrite(hd.c_str(), "GAME");
    filesystem->AddSearchPath(base.c_str(), "GAME");
    filesystem->AddSearchPath(base.c_str(), "GAMECONFIG");
    filesystem->AddSearchPath(skin.c_str(), "SKIN");
    auto* implementation = static_cast<CBaseFileSystem*>(filesystem);
    const int baselineOpenStreams = implementation->m_OpenedFiles.Count();
    FileHandle_t held = filesystem->Open("held.txt", "rb", nullptr);
    assert(held);
    char first = 0;
    assert(filesystem->Read(&first, 1, held) == 1 && first == '0');
    const char* roots[] = {polish.c_str()};
    cse::localized_filesystem::Replacement group{1, hd.c_str(), "GAME", roots, 1, false};
    IFileSystem* view = extension->Prepare(filesystem, &group, 1);
    assert(view);
    assert(Read(filesystem, "token.txt") == "hd");
    assert(Read(view, "token.txt") == "polski");
    assert(Read(view, "packed.txt") == "polish pack");
    assert(Read(view, "overlap.txt") == "polski");
    assert(Read(extension->Baseline(), "token.txt") == "hd");
    assert(Read(view, "held.txt") == "0123456789");
    extension->Commit();
    assert(Read(filesystem, "overlap.txt") == "polski");
    assert(Read(filesystem, "token.txt") == "polski");
    assert(Read(filesystem, "token.txt", "GAMECONFIG") == "english");
    assert(Read(filesystem, "skin.txt", "SKIN") == "skin");
    char remainder[10]{};
    assert(filesystem->Read(remainder, 9, held) == 9);
    assert(std::strcmp(remainder, "123456789") == 0);
    filesystem->Close(held);
    FileHandle_t localizedHeld = filesystem->Open("packed.txt", "rb", nullptr);
    assert(localizedHeld);
    assert(filesystem->Read(&first, 1, localizedHeld) == 1 && first == 'p');
    roots[0] = russian.c_str();
    view = extension->Prepare(filesystem, &group, 1);
    assert(view && Read(view, "token.txt") == "russian");
    extension->Discard();
    assert(Read(filesystem, "token.txt") == "polski");
    group.count = 0;
    view = extension->Prepare(filesystem, &group, 1);
    assert(view && Read(view, "token.txt") == "hd");
    extension->Commit();
    assert(Read(filesystem, "token.txt") == "hd");
    assert(!filesystem->Open("packed.txt", "rb", nullptr));
    assert(filesystem->Tell(localizedHeld) == 1);
    char localizedRemainder[16]{};
    assert(filesystem->Read(localizedRemainder, 10, localizedHeld) == 10);
    assert(std::strcmp(localizedRemainder, "olish pack") == 0);
    filesystem->Close(localizedHeld);
    assert(implementation->m_OpenedFiles.Count() == baselineOpenStreams);
    const std::string missing = (directory / "missing").string();
    group.anchor = missing.c_str();
    assert(!extension->Prepare(filesystem, &group, 1));
    assert(Read(filesystem, "token.txt") == "hd");
    extension->Reset();
    filesystem->RemoveAllSearchPaths();
    filesystem->Unmount();
}
