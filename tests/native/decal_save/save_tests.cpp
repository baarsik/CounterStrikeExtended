#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using byte = unsigned char;
using vec3_t = float[3];
using FileHandle_t = void*;
constexpr int DECAL_NAME_BYTES = 16;
constexpr int SAVEFILE_HEADER = 0x564c4156;
constexpr int SAVEGAME_VERSION = 0x71;
struct DECALLIST {
    char* name;
    short entityIndex;
    byte depth;
    byte flags;
    vec3_t position;
};
struct { float value = 1; } r_decals;
struct { int max_edicts = 128; } cl;
namespace {
std::vector<byte> fileBytes;
std::size_t cursor = 0;
int applications = 0;
int mapReads = 0;
std::array<char, DECAL_NAME_BYTES> name{'d', 'e', 'c', 'a', 'l'};
constexpr unsigned int LegacyDecalRecordBytes = 32;
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}
int DecalListCreate(DECALLIST* output) {
    output[0] = {name.data(), 7, 2, 4, {1.25f, -2.5f, 3.75f}};
    return 1;
}
FileHandle_t FS_OpenPathID(const char*, const char*, const char*) {
    fileBytes.clear();
    cursor = 0;
    return &cursor;
}
FileHandle_t FS_Open(const char*, const char*) {
    cursor = 0;
    return &cursor;
}
void FS_Close(FileHandle_t) {}
void FS_SetVBuf(FileHandle_t, void*, int, std::size_t) {}
unsigned int FS_Size(FileHandle_t) { return static_cast<unsigned int>(fileBytes.size()); }
unsigned int FS_Tell(FileHandle_t) { return static_cast<unsigned int>(cursor); }
int FS_Write(const void* input, int size, int, FileHandle_t) {
    const auto* bytes = static_cast<const byte*>(input);
    fileBytes.insert(fileBytes.end(), bytes, bytes + size);
    return size;
}
int FS_Read(void* output, int size, int, FileHandle_t) {
    const auto count = std::min(static_cast<std::size_t>(size), fileBytes.size() - cursor);
    std::memcpy(output, fileBytes.data() + cursor, count);
    cursor += count;
    return static_cast<int>(count);
}
int Draw_DecalIndexFromName(char* value) {
    Require(std::strcmp(value, name.data()) == 0, "fixed name record preserved");
    return 3;
}
int Draw_DecalIndex(int index) { return index; }
void R_DecalShoot(int texture, int entity, int model, float* position, int flags) {
    Require(texture == 3 && entity == 7 && model == 0 && flags == 4, "decoded decal identifiers preserved");
    Require(position[0] == 1.25f && position[1] == -2.5f && position[2] == 3.75f,
        "decoded decal float bits preserved");
    ++applications;
}
int MSG_ReadByte() { ++mapReads; return 0; }
const char* MSG_ReadString() { return "unused"; }
template<class... Args> void Con_Printf(const char*, Args...) {}
}
#include "decal_io.inc"

int main() {
    CL_Save("save.HL2");
    constexpr byte Expected[] = {
        0x56, 0x41, 0x4c, 0x56, 0x71, 0, 0, 0, 1, 0, 0, 0,
        'd', 'e', 'c', 'a', 'l', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        7, 0, 2, 4, 0, 0, 0xa0, 0x3f, 0, 0, 0x20, 0xc0, 0, 0, 0x70, 0x40
    };
    Require(fileBytes.size() == sizeof(Expected), "client decal save has fixed Win32 record width");
    Require(std::memcmp(fileBytes.data(), Expected, sizeof(Expected)) == 0, "exact legacy bytes on native host");
    CL_Restore("save.HL2");
    Require(applications == 1 && mapReads == 1, "actual writer/reader round trip and wire tail consumed");
    const auto complete = fileBytes;
    for (std::size_t length = 0; length < complete.size(); ++length) {
        fileBytes.assign(complete.begin(), complete.begin() + length);
        const int previousMapReads = mapReads;
        CL_Restore("truncated.HL2");
        Require(applications == 1 && mapReads == previousMapReads + 1,
            "truncated file never reaches renderer and still consumes the wire tail");
    }
    fileBytes = complete;
    constexpr std::size_t FirstEntityOffset = 12 + DECAL_NAME_BYTES;
    fileBytes[FirstEntityOffset] = 0xff;
    fileBytes[FirstEntityOffset + 1] = 0x7f;
    CL_Restore("invalid-entity.HL2");
    Require(applications == 1, "out-of-range saved entity cannot index native arrays");
    fileBytes = complete;
    fileBytes[8] = 0xff;
    fileBytes[9] = 0xff;
    fileBytes[10] = 0xff;
    fileBytes[11] = 0x7f;
    CL_Restore("invalid-count.HL2");
    Require(applications == 1, "oversized decal count rejected before record traversal");
}
