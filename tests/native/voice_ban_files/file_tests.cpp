#include "voice_banmgr.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr std::size_t IdBytes = 16;
constexpr unsigned int ByteValues = 256;
constexpr unsigned int ByteMask = ByteValues - 1;
const std::vector<unsigned char> VersionOne{1, 0, 0, 0};
using Id = std::array<char, IdBytes>;
std::vector<Id>* collected = nullptr;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(17);
    }
}

std::vector<unsigned char> Read(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    Require(file != nullptr && std::fseek(file, 0, SEEK_END) == 0, "Read authored file");
    const long length = std::ftell(file);
    Require(length >= 0 && std::fseek(file, 0, SEEK_SET) == 0, "Measure authored file");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    Require(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size(), "Read complete authored file");
    Require(std::fclose(file) == 0, "Close authored input");
    return bytes;
}

void Write(const std::string& path, const std::vector<unsigned char>& bytes) {
    FILE* file = std::fopen(path.c_str(), "wb");
    Require(file != nullptr, "Create owned fixture file");
    Require(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(), "Write owned fixture file");
    Require(std::fclose(file) == 0, "Close authored output");
}

std::vector<Id> Ids() {
    constexpr unsigned int PairSeedStep = 17;
    std::vector<Id> result;
    for (unsigned int value = 0; value < ByteValues; ++value) {
        Id id{};
        for (std::size_t pair = 0; pair < id.size() / 2; ++pair) {
            const auto first = static_cast<unsigned char>((value + PairSeedStep * static_cast<unsigned int>(pair)) & ByteMask);
            id[pair * 2] = static_cast<char>(first);
            id[pair * 2 + 1] = static_cast<char>(static_cast<unsigned char>(0u - first));
        }
        result.push_back(id);
    }
    return result;
}

std::vector<unsigned char> Encode(const std::vector<Id>& ids) {
    std::vector<unsigned char> result = VersionOne;
    for (const auto& id : ids) {
        result.insert(result.end(), reinterpret_cast<const unsigned char*>(id.data()),
            reinterpret_cast<const unsigned char*>(id.data() + id.size()));
    }
    return result;
}

void Collect(char* source) {
    Id id{};
    std::memcpy(id.data(), source, id.size());
    collected->push_back(id);
}

void Check(CVoiceBanMgr& manager, const std::vector<Id>& expected) {
    std::vector<Id> actual;
    collected = &actual;
    manager.ForEachBannedPlayer(&Collect);
    collected = nullptr;
    Require(actual == expected, "Persistent IDs/order/count differ");
    for (const auto& id : expected) {
        Require(manager.GetPlayerBan(id.data()), "Loaded persistent ID missing");
    }
}

}

int main(int argc, char** argv) {
    Require(argc >= 3, "Owned directory and mode required");
    const std::string directory = argv[1];
    const auto marker = Read(directory + "/owner.marker");
    Require(std::string(marker.begin(), marker.end()) == "CSE voice-ban authored fixture V1", "Wrong owner marker");
    constexpr std::size_t FilenameCapacity = 512;
    Require(directory.size() + sizeof("/voice_ban.dt") < FilenameCapacity, "Fixture path exceeds retained owner capacity");
    const std::string file = directory + "/voice_ban.dt";
    const std::string mode = argv[2];
    auto ids = Ids(); // Every ID hashes to zero, so legacy traversal retains authored insertion order.
    const auto canonical = Encode(ids);
    CVoiceBanMgr manager;
    if (mode == "import") {
        Require(argc == 4 && Read(argv[3]) == canonical, "Cross-architecture fixture bytes differ");
        Write(file, Read(argv[3]));
        Require(manager.Init(directory.c_str()), "Import legacy persistent bans");
        Check(manager, ids);
        manager.SaveState(directory.c_str());
        Require(Read(file) == canonical, "Imported persistent file changed bytes");
        std::puts("PASS imported 256 complete legacy IDs; exact re-encoding");
        return 0;
    }
    Require(mode == "valid" || mode == "full", "Unknown fixture mode");
    Write(file, canonical);
    Require(manager.Init(directory.c_str()), "Read legacy persistent bans");
    Check(manager, ids);
    manager.SaveState(directory.c_str());
    Require(Read(file) == canonical, "Legacy version/16-byte record envelope changed");
    Write(directory + "/canonical-256.bin", Read(file));
    manager.SetPlayerBan(ids.front().data(), true);
    Check(manager, ids);
    manager.SetPlayerBan(ids.front().data(), false);
    ids.erase(ids.begin());
    Check(manager, ids);
    manager.SaveState(directory.c_str());
    Require(Read(file) == Encode(ids), "Mutated persistent IDs changed encoding");
    ids.push_back(ids.back());
    Write(file, Encode(ids));
    Require(manager.Init(directory.c_str()), "Import duplicate legacy records");
    Check(manager, ids);
    manager.SaveState(directory.c_str());
    Require(Read(file) == Encode(ids), "Legacy duplicate records lost");
    if (mode == "full") {
        for (std::size_t length = 0; length < VersionOne.size(); ++length) {
            Write(file, std::vector<unsigned char>(VersionOne.begin(), VersionOne.begin() + length));
            Require(manager.Init(directory.c_str()), "Malformed header must not disable voice");
            Check(manager, {});
        }
    }
    auto trailing = VersionOne;
    trailing.insert(trailing.end(), IdBytes - 1, 0xff);
    Write(file, trailing);
    Require(manager.Init(directory.c_str()), "Ignore incomplete trailing legacy record");
    Check(manager, {});
    Write(file, {2, 0, 0, 0});
    Require(manager.Init(directory.c_str()), "Ignore unsupported legacy version");
    Check(manager, {});
    Require(std::remove(file.c_str()) == 0, "Remove only owned fixture file");
    Require(manager.Init(directory.c_str()), "Missing ban file must keep voice usable");
    Check(manager, {});
    std::puts("PASS legacy voice-ban envelope, 256 raw IDs, persistence/order/mutation/duplicates and invalid inputs");
}
