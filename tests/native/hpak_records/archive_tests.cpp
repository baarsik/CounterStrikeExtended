#include "hashpak.h"
#include "common.h"
#include "conprint.h"
#include "mem.h"
#include "sys.h"
#include "cmd.h"
#include "cvar.h"
#include "crc.h"

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

extern char* HPAK_GetItem(int item);

namespace {
constexpr int PayloadBytes = 1024;
constexpr int HeaderBytes = 12;
constexpr int EntryBytes = 144;
constexpr int DirectoryOffset = HeaderBytes + PayloadBytes;
constexpr int RecordOffset = DirectoryOffset + 4;
constexpr int ExpectedArchiveBytes = RecordOffset + EntryBytes;
constexpr int DiskLumpOffset = 136;
constexpr int DiskLumpSizeOffset = 140;

static_assert(sizeof(void*) != 4 || sizeof(resource_t) == 136, "Reference Win32 resource width changed");
static_assert(offsetof(resource_t, type) == 64, "Reference resource type offset changed");
static_assert(offsetof(resource_t, rgucMD5_hash) == 77, "Reference resource digest offset changed");
static_assert(offsetof(resource_t, pNext) == 128, "Reference resource pointer offset changed");
static_assert(sizeof(void*) != 4 || offsetof(resource_t, pPrev) == 132, "Win32 previous-pointer offset changed");

struct File {
    std::vector<byte>* bytes;
    std::size_t position;
    bool writable;
    std::string name;
};

std::map<std::string, std::vector<byte>> files;
std::set<File*> handles;
std::set<void*> allocations;
std::vector<std::string> command;
std::string console;
int writeCalls = 0;
int shortWriteCall = 0;
int shortReadOffset = -1;
int shortReadCall = 0;
int readCalls = 0;
std::size_t sparseDirectoryGap = 0;
int checks = 0;
bool requireHighAddress = false;

void Require(bool condition, const char* expression, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "line %d: %s\n", line, expression);
        std::abort();
    }
}
#define CHECK(expression) Require(!!(expression), #expression, __LINE__)

File& Access(FileHandle_t handle) {
    File* file = static_cast<File*>(handle);
    CHECK(handles.count(file) == 1);
    return *file;
}

void PutInteger(std::vector<byte>& bytes, std::size_t offset, std::uint32_t value) {
    CHECK(offset + sizeof(value) <= bytes.size());
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<byte>(value >> (index * 8));
    }
}

std::vector<byte> Load(const char* name) {
    std::ifstream input(name, std::ios::binary);
    CHECK(input.good());
    std::vector<byte> bytes;
    char value;
    while (input.get(value)) {
        bytes.push_back(static_cast<byte>(value));
    }
    CHECK(input.eof());
    return bytes;
}

void Save(const std::string& name, const std::vector<byte>& bytes) {
    std::ofstream output(name, std::ios::binary);
    CHECK(output.good());
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    CHECK(output.good());
}

void Reset(const std::vector<byte>& authored) {
    CHECK(handles.empty());
    CHECK(allocations.empty());
    files.clear();
    files["fixture.hpk"] = authored;
    shortWriteCall = 0;
    shortReadOffset = -1;
    shortReadCall = 0;
    readCalls = 0;
    sparseDirectoryGap = 0;
    writeCalls = 0;
    console.clear();
    command = {"hpk", "fixture", "all"};
}

resource_t Resource(const std::vector<byte>& authored) {
    resource_t resource{};
    std::memcpy(resource.szFileName, authored.data() + RecordOffset, sizeof(resource.szFileName));
    resource.type = t_decal;
    resource.nIndex = 2;
    resource.nDownloadSize = PayloadBytes;
    resource.ucFlags = RES_CUSTOM;
    std::memcpy(resource.rgucMD5_hash, authored.data() + RecordOffset + 77, MD5_DIGEST_BYTES);
    resource.playernum = 5;
    for (std::size_t index = 0; index < sizeof(resource.rguc_reserved); ++index) {
        resource.rguc_reserved[index] = static_cast<byte>(index);
    }
    return resource;
}

void CheckResource(const resource_t& actual, const resource_t& expected, bool diskPointers = true) {
    CHECK(std::memcmp(actual.szFileName, expected.szFileName, sizeof(actual.szFileName)) == 0);
    CHECK(actual.type == expected.type);
    CHECK(actual.nIndex == expected.nIndex);
    CHECK(actual.nDownloadSize == expected.nDownloadSize);
    CHECK(actual.ucFlags == expected.ucFlags);
    CHECK(std::memcmp(actual.rgucMD5_hash, expected.rgucMD5_hash, MD5_DIGEST_BYTES) == 0);
    CHECK(actual.playernum == expected.playernum);
    CHECK(std::memcmp(actual.rguc_reserved, expected.rguc_reserved, sizeof(actual.rguc_reserved)) == 0);
    if (diskPointers) {
        CHECK(actual.pNext == nullptr && actual.pPrev == nullptr);
    }
}

void CheckPayload(resource_t& resource, const std::vector<byte>& payload, bool queued = false) {
    byte* buffer = nullptr;
    int size = 0;
    CHECK(HPAK_GetDataPointer("fixture", &resource, &buffer, &size));
    CHECK(buffer && size == static_cast<int>(payload.size()));
    CHECK(std::memcmp(buffer, payload.data(), payload.size()) == 0);
    Mem_Free(buffer);
    CHECK(handles.empty());
    CHECK(queued || allocations.empty());
}

void CheckRejected(const std::vector<byte>& malformed, resource_t& resource) {
    files["fixture.hpk"] = malformed;
    byte* buffer = reinterpret_cast<byte*>(static_cast<std::uintptr_t>(1));
    int size = -1;
    CHECK(!HPAK_GetDataPointer("fixture", &resource, &buffer, &size));
    CHECK(!buffer && size == 0);
    resource_t output{};
    CHECK(!HPAK_ResourceForHash("fixture", resource.rgucMD5_hash, &output));
    CHECK(!HPAK_ResourceForIndex("fixture", 1, &output));
    CHECK(handles.empty());
    CHECK(allocations.empty());
}
}

cmd_source_t cmd_source = src_command;
cvar_t hpk_maxsize{};
const char com_empty_string[] = "";
// Bind unrelated CRC/file helpers without supplying behavior to the HPAK paths.
// decltype preserves the pinned SDK's declaration in both old and updated trees.
decltype(com_gamedir) com_gamedir = {};
decltype(LittleLong) LittleLong = nullptr;

FileHandle_t FS_OpenPathID(const char*, const char*, const char*) {
    std::abort();
}

int FS_EndOfFile(FileHandle_t) {
    std::abort();
}

int FS_IsOk(FileHandle_t) {
    std::abort();
}

FileHandle_t FS_Open(const char* name, const char* mode) {
    const bool writable = std::strchr(mode, 'w') != nullptr;
    if (!writable && !files.count(name)) {
        return nullptr;
    }
    if (writable) {
        files[name].clear();
    }
    File* file = new File{&files[name], 0, writable, name};
    handles.insert(file);
    return file;
}

void FS_Close(FileHandle_t handle) {
    File& file = Access(handle);
    handles.erase(&file);
    delete &file;
}

int FS_Read(void* output, int size, int count, FileHandle_t handle) {
    File& file = Access(handle);
    CHECK(size >= 0 && count == 1);
    ++readCalls;
    std::size_t physicalPosition = file.position;
    if (file.name == "fixture.hpk" && sparseDirectoryGap && file.position >= DirectoryOffset) {
        CHECK(file.position >= DirectoryOffset + sparseDirectoryGap);
        physicalPosition -= sparseDirectoryGap;
    }
    CHECK(physicalPosition <= file.bytes->size());
    std::size_t bytes = (std::min)(static_cast<std::size_t>(size), file.bytes->size() - physicalPosition);
    if (((file.name == "fixture.hpk" && static_cast<int>(file.position) == shortReadOffset)
        || readCalls == shortReadCall) && bytes) {
        --bytes;
    }
    if (bytes) {
        std::memcpy(output, file.bytes->data() + physicalPosition, bytes);
    }
    file.position += bytes;
    // The engine's FS_Read forwards IFileSystem::Read's byte count, not fread's item count.
    return static_cast<int>(bytes);
}

int FS_Write(const void* input, int size, int count, FileHandle_t handle) {
    File& file = Access(handle);
    CHECK(file.writable && size >= 0 && count == 1);
    ++writeCalls;
    const std::size_t bytes = static_cast<std::size_t>(size - (writeCalls == shortWriteCall && size > 0 ? 1 : 0));
    if (file.bytes->size() < file.position + bytes) {
        file.bytes->resize(file.position + bytes);
    }
    if (bytes) {
        std::memcpy(file.bytes->data() + file.position, input, bytes);
    }
    file.position += bytes;
    return static_cast<int>(bytes);
}

void FS_Seek(FileHandle_t handle, int offset, FileSystemSeek_t origin) {
    File& file = Access(handle);
    CHECK(origin == FILESYSTEM_SEEK_HEAD && offset >= 0);
    const std::size_t gap = file.name == "fixture.hpk" ? sparseDirectoryGap : 0;
    CHECK(static_cast<std::size_t>(offset) <= file.bytes->size() + gap);
    file.position = static_cast<std::size_t>(offset);
}

unsigned int FS_Tell(FileHandle_t handle) {
    return static_cast<unsigned int>(Access(handle).position);
}

unsigned int FS_Size(FileHandle_t handle) {
    File& file = Access(handle);
    const std::size_t gap = file.name == "fixture.hpk" ? sparseDirectoryGap : 0;
    return static_cast<unsigned int>(file.bytes->size() + gap);
}

void FS_Flush(FileHandle_t handle) {
    Access(handle);
}

void FS_Unlink(const char* name) {
    for (const File* file : handles) {
        CHECK(file->name != name);
    }
    files.erase(name);
}

void FS_RemoveFile(const char* name, const char*) {
    FS_Unlink(name);
}

void FS_Rename(const char* oldName, const char* newName) {
    CHECK(files.count(oldName) && !files.count(newName));
    files[newName] = files[oldName];
    FS_Unlink(oldName);
}

void* Mem_Malloc(std::size_t size) {
    void* memory = std::malloc(size ? size : 1);
    CHECK(memory);
    if (requireHighAddress) {
        CHECK(reinterpret_cast<std::uintptr_t>(memory) > UINT32_MAX);
    }
    CHECK(allocations.insert(memory).second);
    return memory;
}

void* Mem_ZeroMalloc(std::size_t size) {
    void* memory = Mem_Malloc(size);
    std::memset(memory, 0, size);
    return memory;
}

void Mem_Free(void* memory) {
    if (memory) {
        CHECK(allocations.erase(memory) == 1);
        std::free(memory);
    }
}

char* Mem_Strdup(const char* text) {
    const std::size_t size = std::strlen(text) + 1;
    char* copy = static_cast<char*>(Mem_Malloc(size));
    std::memcpy(copy, text, size);
    return copy;
}

void Q_memset(void* output, int value, int bytes) {
    CHECK(bytes >= 0);
    std::memset(output, value, static_cast<std::size_t>(bytes));
}

void Q_memcpy(void* output, const void* input, int bytes) {
    CHECK(bytes >= 0);
    if (bytes) {
        std::memcpy(output, input, static_cast<std::size_t>(bytes));
    }
}

int Q_memcmp(const void* first, const void* second, int bytes) {
    CHECK(bytes >= 0);
    return std::memcmp(first, second, static_cast<std::size_t>(bytes));
}

void Q_strcpy(char* output, const char* input) {
    std::strcpy(output, input);
}

void Q_strncpy(char* output, const char* input, int count) {
    CHECK(count >= 0);
    std::strncpy(output, input, static_cast<std::size_t>(count));
}

int Q_strncmp(const char* first, const char* second, int count) {
    CHECK(count >= 0);
    return std::strncmp(first, second, static_cast<std::size_t>(count));
}

int Q_strcasecmp(const char* first, const char* second) {
    for (; *first && *second; ++first, ++second) {
        const int a = *first >= 'A' && *first <= 'Z' ? *first + ('a' - 'A') : static_cast<byte>(*first);
        const int b = *second >= 'A' && *second <= 'Z' ? *second + ('a' - 'A') : static_cast<byte>(*second);
        if (a != b) {
            return a - b;
        }
    }
    return static_cast<byte>(*first) - static_cast<byte>(*second);
}

int Q_atoi(const char* text) {
    return std::atoi(text);
}

void COM_DefaultExtension(char* path, char* extension) {
    if (!std::strrchr(path, '.')) {
        std::strcat(path, extension);
    }
}

void COM_FixSlashes(char* path) {
    for (; *path; ++path) {
        if (*path == '\\') {
            *path = '/';
        }
    }
}

void COM_StripExtension(const char* input, char* output) {
    std::strcpy(output, input);
    char* dot = std::strrchr(output, '.');
    if (dot) {
        *dot = '\0';
    }
}

char* COM_FileBase(const char* input, char* output) {
    const char* slash = std::strrchr(input, '/');
    std::strcpy(output, slash ? slash + 1 : input);
    char* dot = std::strrchr(output, '.');
    if (dot) {
        *dot = '\0';
    }
    return output;
}

void COM_CreatePath(char*) {
}

void COM_CopyFileChunk(FileHandle_t output, FileHandle_t input, int size) {
    // Only preserved reference code calls this. Match its ignored-result, 1024-byte
    // copy and flush behavior; no assertion converts an I/O failure into success.
    constexpr int CopyBufferBytes = 1024;
    CHECK(size >= 0);
    byte buffer[CopyBufferBytes] = {};
    while (size > CopyBufferBytes) {
        FS_Read(buffer, sizeof(buffer), 1, input);
        FS_Write(buffer, sizeof(buffer), 1, output);
        size -= CopyBufferBytes;
    }
    FS_Read(buffer, size, 1, input);
    FS_Write(buffer, size, 1, output);
    FS_Flush(input);
    FS_Flush(output);
}

void Con_Printf(const char* format, ...) {
    char buffer[4096];
    va_list arguments;
    va_start(arguments, format);
    const int size = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    CHECK(size >= 0 && size < static_cast<int>(sizeof(buffer)));
    console += buffer;
}

void Log_Printf(const char*, ...) {
}

[[noreturn]] void Sys_Error(const char*, ...) {
    std::abort();
}

void Cvar_Set(const char*, const char*) {
    std::abort();
}

void Cvar_DirectSet(cvar_t*, const char*) {
    std::abort();
}

void Cmd_AddCommand(char*, xcommand_t) {
}

int Cmd_Argc() {
    return static_cast<int>(command.size());
}

const char* Cmd_Argv(int index) {
    CHECK(index >= 0 && static_cast<std::size_t>(index) < command.size());
    return command[index].c_str();
}

int main(int argc, char** argv) {
    CHECK(argc == 4);
    const bool reference = std::strcmp(argv[3], "reference") == 0;
    requireHighAddress = sizeof(void*) > sizeof(std::uint32_t);
    const std::vector<byte> authored = Load(argv[1]);
    CHECK(authored.size() == ExpectedArchiveBytes);
    resource_t resource = Resource(authored);
    const std::vector<byte> payload(authored.begin() + HeaderBytes, authored.begin() + DirectoryOffset);
    Reset(authored);
    CheckPayload(resource, payload);
    resource_t output{};
    CHECK(HPAK_ResourceForHash("fixture", resource.rgucMD5_hash, &output));
    CheckResource(output, resource);
    CHECK(HPAK_ResourceForIndex("fixture", 1, &output));
    CheckResource(output, resource);
    HPAK_List_f();
    CHECK(console.find("1:      decal 1.00K {authored}") != std::string::npos);
    command = {"hpkval", "fixture"};
    HPAK_Validate_f();
    CHECK(console.find(" OK\n") != std::string::npos);
    command = {"hpkextract", "fixture", "all"};
    HPAK_Extract_f();
    CHECK(files["hpklmps/lmp0000.wad"] == payload);
    files["custom.hpk"] = authored;
    const std::string item = HPAK_GetItem(0);
    CHECK(item == std::string("!MD5") + MD5_Print(resource.rgucMD5_hash));
    HPAK_ValidatePak("fixture.hpk");
    CHECK(files["fixture.hpk"] == authored);
    CHECK(handles.empty() && allocations.empty());

    Reset(authored);
    HPAK_CreatePak("created", &resource, const_cast<byte*>(payload.data()), nullptr);
    CHECK(files["created.hpk"] == authored);
    Save(std::string(argv[2]) + "/created.hpk", files["created.hpk"]);
    files["fixture.hpk"] = files["created.hpk"];
    CheckPayload(resource, payload);

    std::vector<byte> secondPayload = payload;
    secondPayload[0] ^= 0xff;
    resource_t second = resource;
    second.nIndex = -123;
    second.playernum = 31;
    second.ucFlags = 0xa5;
    MD5Context_t digest{};
    MD5Init(&digest);
    MD5Update(&digest, secondPayload.data(), static_cast<unsigned int>(secondPayload.size()));
    MD5Final(second.rgucMD5_hash, &digest);
    HPAK_AddLump(FALSE, "fixture", &second, secondPayload.data(), nullptr);
    CHECK(files["fixture.hpk"].size() == HeaderBytes + PayloadBytes * 2 + 4 + EntryBytes * 2);
    CheckPayload(resource, payload);
    CheckPayload(second, secondPayload);
    CHECK(HPAK_ResourceForHash("fixture", second.rgucMD5_hash, &output));
    CheckResource(output, second);
    const std::vector<byte> twoEntries = files["fixture.hpk"];
    HPAK_AddLump(FALSE, "fixture", &second, secondPayload.data(), nullptr);
    CHECK(files["fixture.hpk"] == twoEntries);
    Save(std::string(argv[2]) + "/two-entries.hpk", twoEntries);
    HPAK_RemoveLump("fixture", &second);
    CHECK(files["fixture.hpk"] == authored);
    HPAK_RemoveLump("fixture", &resource);
    CHECK(!files.count("fixture.hpk"));
    CHECK(handles.empty() && allocations.empty());

    Reset(authored);
    HPAK_AddLump(TRUE, "fixture", &second, secondPayload.data(), nullptr);
    CheckPayload(second, secondPayload, true); // Queue owns allocations until the flush.
    HPAK_FlushHostQueue();
    CheckPayload(second, secondPayload);
    CHECK(files["fixture.hpk"] == twoEntries);

    Reset(authored);
    HPAK_AddLump(TRUE, "fixture", &second, secondPayload.data(), nullptr);
    HPAK_AddLump(TRUE, "fixture", &second, secondPayload.data(), nullptr);
    HPAK_AddLump(TRUE, "other", &resource, const_cast<byte*>(payload.data()), nullptr);
    HPAK_FlushHostQueue();
    CHECK(files["fixture.hpk"] == twoEntries);
    CHECK(files["other.hpk"] == authored);
    CHECK(handles.empty() && allocations.empty());

    // Keep the existing console-owner contract visible: RemoveLump selects
    // Cmd_Argv(1), and a one-entry archive is removed before hash lookup.
    Reset(authored);
    files["fixture.hpk"] = twoEntries;
    files["other.hpk"] = twoEntries;
    HPAK_RemoveLump("other", &second);
    CHECK(files["fixture.hpk"] == authored && files["other.hpk"] == twoEntries);
    HPAK_RemoveLump("fixture", &second);
    CHECK(!files.count("fixture.hpk"));
    CHECK(handles.empty() && allocations.empty());

    struct OwnedLump {
        resource_t resource;
        std::vector<byte> payload;
    };
    std::vector<OwnedLump> ordered;
    for (int index = 0; index < 4; ++index) {
        OwnedLump lump{resource, payload};
        lump.payload[0] ^= static_cast<byte>(index);
        lump.resource.nIndex = index;
        MD5Init(&digest);
        MD5Update(&digest, lump.payload.data(), static_cast<unsigned int>(lump.payload.size()));
        MD5Final(lump.resource.rgucMD5_hash, &digest);
        ordered.push_back(lump);
    }
    std::sort(ordered.begin(), ordered.end(), [](const OwnedLump& first, const OwnedLump& secondLump) {
        return std::memcmp(first.resource.rgucMD5_hash, secondLump.resource.rgucMD5_hash, MD5_DIGEST_BYTES) < 0;
    });
    Reset(authored);
    HPAK_CreatePak("fixture", &ordered[1].resource, ordered[1].payload.data(), nullptr);
    for (const int index : {0, 3, 2}) { // Exercise insertion at the beginning, end and middle.
        HPAK_AddLump(FALSE, "fixture", &ordered[index].resource, ordered[index].payload.data(), nullptr);
    }
    CHECK(files["fixture.hpk"].size() == HeaderBytes + PayloadBytes * 4 + 4 + EntryBytes * 4);
    for (int index = 0; index < 4; ++index) {
        CHECK(HPAK_ResourceForIndex("fixture", index + 1, &output));
        CheckResource(output, ordered[index].resource);
        CheckPayload(ordered[index].resource, ordered[index].payload);
    }
    const std::vector<byte> fourEntries = files["fixture.hpk"];
    Save(std::string(argv[2]) + "/four-entries.hpk", fourEntries);
    for (const int removed : {0, 1, 3}) {
        Reset(authored);
        files["fixture.hpk"] = fourEntries;
        HPAK_RemoveLump("fixture", &ordered[removed].resource);
        CHECK(files["fixture.hpk"].size() == HeaderBytes + PayloadBytes * 3 + 4 + EntryBytes * 3);
        int entry = 0;
        for (int index = 0; index < 4; ++index) {
            if (index != removed) {
                CHECK(HPAK_ResourceForIndex("fixture", ++entry, &output));
                CheckResource(output, ordered[index].resource);
                CheckPayload(ordered[index].resource, ordered[index].payload);
            }
        }
        CHECK(!HPAK_ResourceForHash("fixture", ordered[removed].resource.rgucMD5_hash, &output));
        CHECK(handles.empty() && allocations.empty());
    }

    Reset(authored);
    files["source.bin"] = secondPayload;
    FileHandle_t source = FS_Open("source.bin", "rb");
    HPAK_AddLump(FALSE, "fixture", &second, nullptr, source);
    FS_Close(source);
    CHECK(files["fixture.hpk"] == twoEntries);
    CHECK(handles.empty() && allocations.empty());

    // A representable signed directory offset can have a table tail above2GiB.
    // Sparse logical positions characterize the reader, without allocating/copying that gap.
    Reset(authored);
    sparseDirectoryGap = static_cast<std::size_t>((std::numeric_limits<int>::max)()) - DirectoryOffset;
    PutInteger(files["fixture.hpk"], 8, static_cast<std::uint32_t>(DirectoryOffset + sparseDirectoryGap));
    CheckPayload(resource, payload);
    CHECK(HPAK_ResourceForHash("fixture", resource.rgucMD5_hash, &output));
    CheckResource(output, resource);

    if (!reference) {
        Reset(authored);
        files["fixture.hpk"] = twoEntries;
        resource_t absent = resource;
        std::memset(absent.rgucMD5_hash, 0, sizeof(absent.rgucMD5_hash));
        HPAK_RemoveLump("fixture", &absent);
        CHECK(files["fixture.hpk"] == twoEntries && !files.count("fixture.hp2"));
        CHECK(handles.empty() && allocations.empty());
        Reset(authored);
        std::vector<byte> duplicateHashes = twoEntries;
        const std::size_t twoRecordOffset = HeaderBytes + PayloadBytes * 2 + 4;
        for (std::size_t index = 0; index < 2; ++index) {
            std::memcpy(duplicateHashes.data() + twoRecordOffset + index * EntryBytes + 77,
                resource.rgucMD5_hash, sizeof(resource.rgucMD5_hash));
        }
        files["fixture.hpk"] = duplicateHashes;
        HPAK_RemoveLump("fixture", &resource);
        CHECK(files["fixture.hpk"] == duplicateHashes && !files.count("fixture.hp2"));
        CHECK(writeCalls == 1 && handles.empty() && allocations.empty());

        Reset(authored);
        constexpr int SharedLumpBytes = 128 * 1024;
        constexpr int SharedEntries = 16386;
        const int sharedDirectoryOffset = HeaderBytes + SharedLumpBytes;
        const std::size_t sharedRecordOffset = sharedDirectoryOffset + 4;
        std::vector<byte> sharedRanges(sharedRecordOffset + SharedEntries * EntryBytes);
        std::copy(authored.begin(), authored.begin() + HeaderBytes, sharedRanges.begin());
        PutInteger(sharedRanges, 8, sharedDirectoryOffset);
        PutInteger(sharedRanges, sharedDirectoryOffset, SharedEntries);
        for (int index = 0; index < SharedEntries; ++index) {
            const std::size_t offset = sharedRecordOffset + static_cast<std::size_t>(index) * EntryBytes;
            std::copy(authored.begin() + RecordOffset, authored.end(), sharedRanges.begin() + offset);
            PutInteger(sharedRanges, offset + 72, SharedLumpBytes);
            PutInteger(sharedRanges, offset + DiskLumpSizeOffset, SharedLumpBytes);
            if (index) {
                std::memcpy(sharedRanges.data() + offset + 77, second.rgucMD5_hash, sizeof(second.rgucMD5_hash));
            }
        }
        files["fixture.hpk"] = sharedRanges;
        HPAK_RemoveLump("fixture", &resource);
        CHECK(files["fixture.hpk"] == sharedRanges && !files.count("fixture.hp2"));
        CHECK(writeCalls == 1 && handles.empty() && allocations.empty());
        for (std::size_t length = 0; length < authored.size(); ++length) {
            Reset(authored);
            CheckRejected(std::vector<byte>(authored.begin(), authored.begin() + length), resource);
        }
        const std::pair<std::size_t, std::uint32_t> mutations[] = {
            {4, 2}, {8, 0}, {8, 0xffffffffu}, {8, 0x7fffffffu}, {8, ExpectedArchiveBytes},
            {DirectoryOffset, 0}, {DirectoryOffset, 32769}, {DirectoryOffset, 0xffffffffu},
            {DirectoryOffset, 2}, {RecordOffset + 64, 0xffffffffu}, {RecordOffset + 64, 0x7fffffffu},
            {RecordOffset + DiskLumpOffset, 0}, {RecordOffset + DiskLumpOffset, 0xffffffffu},
            {RecordOffset + DiskLumpOffset, DirectoryOffset + 1},
            {RecordOffset + DiskLumpSizeOffset, 0xffffffffu},
            {RecordOffset + DiskLumpSizeOffset, 131073}, {RecordOffset + DiskLumpSizeOffset, PayloadBytes + 1}
        };
        for (const auto& mutation : mutations) {
            Reset(authored);
            std::vector<byte> bytes = authored;
            PutInteger(bytes, mutation.first, mutation.second);
            CheckRejected(bytes, resource);
        }
        for (const int offset : {0, DirectoryOffset, RecordOffset, HeaderBytes}) {
            Reset(authored);
            shortReadOffset = offset;
            byte* buffer = nullptr;
            int size = -1;
            CHECK(!HPAK_GetDataPointer("fixture", &resource, &buffer, &size));
            CHECK(!buffer && size == 0 && handles.empty() && allocations.empty());
        }
        for (int write = 1; write <= 5; ++write) {
            Reset(authored);
            shortWriteCall = write;
            HPAK_CreatePak("created", &resource, const_cast<byte*>(payload.data()), nullptr);
            CHECK(!files.count("created.hpk"));
            CHECK(handles.empty() && allocations.empty());
        }
        for (int write = 1; write <= 7; ++write) {
            Reset(authored);
            shortWriteCall = write;
            HPAK_AddLump(FALSE, "fixture", &second, secondPayload.data(), nullptr);
            CHECK(files["fixture.hpk"] == authored);
            CHECK(!files.count("fixture.hp2"));
            CHECK(handles.empty() && allocations.empty());
        }
        for (int write = 1; write <= 5; ++write) {
            Reset(authored);
            files["fixture.hpk"] = twoEntries;
            shortWriteCall = write;
            HPAK_RemoveLump("fixture", &second);
            CHECK(files["fixture.hpk"] == twoEntries);
            CHECK(!files.count("fixture.hp2"));
            CHECK(handles.empty() && allocations.empty());
        }
        Reset(authored);
        files["fixture.hpk"] = twoEntries;
        shortReadOffset = HeaderBytes;
        HPAK_RemoveLump("fixture", &second);
        CHECK(files["fixture.hpk"] == twoEntries);
        CHECK(!files.count("fixture.hp2"));
        CHECK(handles.empty() && allocations.empty());
        for (const bool queued : {false, true}) {
            for (int failedRead = 1; failedRead <= (queued ? 2 : 7); ++failedRead) {
                if (!queued && failedRead != 1 && failedRead != 7) {
                    continue;
                }
                Reset(authored);
                files["source.bin"] = secondPayload;
                source = FS_Open("source.bin", "rb");
                shortReadCall = failedRead;
                HPAK_AddLump(queued, "fixture", &second, nullptr, source);
                FS_Close(source);
                HPAK_FlushHostQueue();
                CHECK(files["fixture.hpk"] == authored);
                CHECK(handles.empty() && allocations.empty());
            }
        }
        for (const int failedRead : {1, 2}) {
            Reset(authored);
            files["source.bin"] = secondPayload;
            source = FS_Open("source.bin", "rb");
            shortReadCall = failedRead;
            HPAK_CreatePak("created", &second, nullptr, source);
            FS_Close(source);
            CHECK(!files.count("created.hpk"));
            CHECK(handles.empty() && allocations.empty());
        }
        Reset(authored);
        sparseDirectoryGap = static_cast<std::size_t>((std::numeric_limits<int>::max)())
            - DirectoryOffset;
        PutInteger(files["fixture.hpk"], 8, static_cast<std::uint32_t>(DirectoryOffset + sparseDirectoryGap));
        const std::vector<byte> sparseInput = files["fixture.hpk"];
        HPAK_AddLump(FALSE, "fixture", &second, secondPayload.data(), nullptr);
        CHECK(files["fixture.hpk"] == sparseInput && writeCalls == 0);
        CHECK(!files.count("fixture.hp2"));
        CHECK(handles.empty() && allocations.empty());
        Reset(authored);
        resource.pNext = &second;
        resource.pPrev = &resource;
        HPAK_CreatePak("created", &resource, const_cast<byte*>(payload.data()), nullptr);
        CHECK(files["created.hpk"] == authored);
        std::vector<byte> oldPointers = authored;
        constexpr byte DiscardedPointerAndPaddingBits = 0xa5;
        std::fill(oldPointers.begin() + RecordOffset + 126, oldPointers.begin() + RecordOffset + 136,
            DiscardedPointerAndPaddingBits);
        files["fixture.hpk"] = oldPointers;
        CHECK(HPAK_ResourceForHash("fixture", resource.rgucMD5_hash, &output));
        CheckResource(output, resource);
        for (int type = t_sound; type <= t_world; ++type) {
            PutInteger(oldPointers, RecordOffset + 64, static_cast<std::uint32_t>(type));
            files["fixture.hpk"] = oldPointers;
            CHECK(HPAK_ResourceForHash("fixture", resource.rgucMD5_hash, &output));
            CHECK(output.type == type);
        }
        CHECK(!HPAK_ResourceForIndex("fixture", 0, &output));
        CHECK(!HPAK_ResourceForIndex("fixture", 2, &output));
        CHECK(handles.empty() && allocations.empty());
    }
    std::printf("HPAK complete owner: checks=%d, resource=%zu, high_addresses=%d, mode=%s\n",
        checks, sizeof(resource_t), requireHighAddress, reference ? "reference-valid" : "current-full");
    return 0;
}
