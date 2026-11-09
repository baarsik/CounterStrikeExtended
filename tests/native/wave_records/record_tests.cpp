#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>

namespace {
constexpr std::size_t kRiffHeaderBytes = 44;
constexpr std::size_t kLegacyReadDataOffset = 48;
constexpr std::size_t kApproxHeaderBytes = 128;
constexpr std::size_t kDwordBytes = 4;
constexpr int kCallLimit = 100;

struct MemoryFile {
    std::vector<unsigned char> bytes;
    std::size_t position = 0;
    unsigned int reportedSize = 0;
    int opens = 0;
    int closes = 0;
    int reads = 0;
    int writes = 0;
    bool missing = false;
};
MemoryFile memoryFile;
int diagnostics = 0;

void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(EXIT_FAILURE);
    }
}
}

using FileHandle_t = MemoryFile*;
enum FileSystemSeek_t { FILESYSTEM_SEEK_HEAD };
FileHandle_t FS_Open(const char*, const char* mode) {
    ++memoryFile.opens;
    if (memoryFile.missing) {
        return nullptr;
    }
    memoryFile.position = 0;
    if (mode[0] == 'w') {
        memoryFile.bytes.clear();
    }
    return &memoryFile;
}
void FS_Close(FileHandle_t current) {
    Check(current == &memoryFile, "Foreign close");
    ++memoryFile.closes;
}
int FS_Read(void* output, int size, int count, FileHandle_t current) {
    Check(current == &memoryFile && size >= 0 && count == 1, "Invalid read");
    Check(++memoryFile.reads < kCallLimit, "Read loop");
    const std::size_t bytes = static_cast<std::size_t>(size);
    Check(memoryFile.position <= memoryFile.bytes.size() && bytes <= memoryFile.bytes.size() - memoryFile.position,
        "Fixture forbids short reads and uninitialized-byte comparisons");
    std::memcpy(output, memoryFile.bytes.data() + memoryFile.position, bytes);
    memoryFile.position += bytes;
    return count;
}
int FS_Write(const void* input, int size, int count, FileHandle_t current) {
    Check(current == &memoryFile && size >= 0 && count == 1, "Invalid write");
    Check(++memoryFile.writes < kCallLimit, "Write loop");
    const std::size_t bytes = static_cast<std::size_t>(size);
    if (memoryFile.position + bytes > memoryFile.bytes.size()) {
        memoryFile.bytes.resize(memoryFile.position + bytes);
    }
    std::memcpy(memoryFile.bytes.data() + memoryFile.position, input, bytes);
    memoryFile.position += bytes;
    return count;
}
void FS_Seek(FileHandle_t current, int offset, FileSystemSeek_t origin) {
    Check(current == &memoryFile && offset >= 0 && origin == FILESYSTEM_SEEK_HEAD, "Invalid seek");
    memoryFile.position = static_cast<std::size_t>(offset);
}
unsigned int FS_Size(FileHandle_t current) {
    Check(current == &memoryFile, "Foreign size");
    return memoryFile.reportedSize;
}
unsigned int FS_Tell(FileHandle_t current) {
    Check(current == &memoryFile, "Foreign tell");
    return static_cast<unsigned int>(memoryFile.position);
}
#if !defined(_WIN32)
int _strnicmp(const char* left, const char* right, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        unsigned char a = static_cast<unsigned char>(left[index]);
        unsigned char b = static_cast<unsigned char>(right[index]);
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<unsigned char>(a + ('a' - 'A'));
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<unsigned char>(b + ('a' - 'A'));
        }
        if (a != b || a == 0) {
            return static_cast<int>(a) - static_cast<int>(b);
        }
    }
    return 0;
}
#endif
void Con_Printf(const char*, ...) {
    ++diagnostics;
}

#include "actual_wave.inc"

namespace {
void PutDword(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < kDwordBytes; ++index) {
        constexpr unsigned int kBitsPerByte = 8;
        bytes[offset + index] = static_cast<unsigned char>(value >> (index * kBitsPerByte));
    }
}
void PrintBytes(const char* label, const std::vector<unsigned char>& bytes) {
    std::printf("%s=", label);
    for (const unsigned char byte : bytes) {
        std::printf("%02x", static_cast<unsigned int>(byte));
    }
    std::putchar('\n');
}
void Reset() {
    memoryFile = MemoryFile{};
    diagnostics = 0;
}
void CheckClosed() {
    Check(memoryFile.opens == 1 && memoryFile.closes == 1, "File ownership differs");
}
void TestWrite() {
    constexpr char kPcm[]{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x08};
    constexpr unsigned char kExpected[]{
        0x52, 0x49, 0x46, 0x46, 0x2c, 0, 0, 0, 0x57, 0x41, 0x56, 0x45,
        0x66, 0x6d, 0x74, 0x20, 0x10, 0, 0, 0, 1, 0, 2, 0,
        0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 0x10, 0,
        0x64, 0x61, 0x74, 0x61, 8, 0, 0, 0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x08
    };
    Reset();
    Check(WriteWaveFile("owned", kPcm, sizeof(kPcm), 16, 2, 44100), "Write failed");
    Check(memoryFile.bytes.size() == kRiffHeaderBytes + sizeof(kPcm), "Wave record width differs");
    Check(std::equal(memoryFile.bytes.begin(), memoryFile.bytes.end(), std::begin(kExpected)), "Wave output bytes differ");
    CheckClosed();
    PrintBytes("written", memoryFile.bytes);
}
void TestRead() {
    Reset();
    constexpr std::uint32_t kSamplesWord = 0x80000001u;
    constexpr std::size_t kPcmBytes = 7;
    memoryFile.bytes.resize(kLegacyReadDataOffset + kPcmBytes);
    memoryFile.bytes[22] = 2;
    memoryFile.bytes[34] = 16;
    PutDword(memoryFile.bytes, 24, kSamplesWord);
    PutDword(memoryFile.bytes, 40, static_cast<std::uint32_t>(kPcmBytes));
    PutDword(memoryFile.bytes, 44, 0x44332211);
    for (std::size_t index = 0; index < kPcmBytes; ++index) {
        memoryFile.bytes[kLegacyReadDataOffset + index] = static_cast<unsigned char>(index + 1);
    }
    char* pcm = nullptr;
    int bytes = 0;
    int bits = 0;
    int channels = 0;
    int samples = 0;
    Check(ReadWaveFile("owned", pcm, bytes, bits, channels, samples), "Read failed");
    Check(bytes == kPcmBytes && bits == 16 && channels == 2, "Read metadata differs");
    Check(static_cast<std::uint32_t>(samples) == kSamplesWord, "Sample-rate word differs");
    Check(std::memcmp(pcm, memoryFile.bytes.data() + kLegacyReadDataOffset, kPcmBytes) == 0, "Legacy PCM offset differs");
    CheckClosed();
    std::printf("read=%u,%d,%d,%d,%zu\n", static_cast<unsigned int>(samples), bytes, bits, channels, memoryFile.position);
    delete[] pcm;
}
void TestApprox() {
    struct Case { unsigned int size; std::uint32_t average; unsigned int expected; };
    constexpr Case kCases[]{
        {176528, 176400, 1002}, {136, 8, 1000}, {127, 1000, 0xffffffffu},
        {0xffffffffu, 0xffffffffu, 1000}, {128, 1, 0}, {128, 0, 0}
    };
    for (const Case& entry : kCases) {
        Reset();
        memoryFile.bytes.resize(kApproxHeaderBytes);
        std::memcpy(memoryFile.bytes.data(), "RIFF", 4);
        std::memcpy(memoryFile.bytes.data() + 8, "WAVEfmt ", 8);
        PutDword(memoryFile.bytes, 28, entry.average);
        memoryFile.reportedSize = entry.size;
        const unsigned int actual = COM_GetApproxWavePlayLength("owned");
        Check(actual == entry.expected, "Approximate duration differs");
        Check(diagnostics == (entry.average == 0 ? 1 : 0), "Duration diagnostic differs");
        CheckClosed();
        std::printf("approx=%u,%u,%u\n", entry.size, entry.average, actual);
    }
    for (const std::size_t offset : {std::size_t{0}, std::size_t{8}, std::size_t{12}}) {
        Reset();
        memoryFile.bytes.resize(kApproxHeaderBytes);
        std::memcpy(memoryFile.bytes.data(), "RIFF", 4);
        std::memcpy(memoryFile.bytes.data() + 8, "WAVEfmt ", 8);
        memoryFile.bytes[offset] = 'x';
        Check(COM_GetApproxWavePlayLength("owned") == 0, "Invalid signature accepted");
        CheckClosed();
    }
}
void TestMissing() {
    Reset();
    memoryFile.missing = true;
    Check(COM_GetApproxWavePlayLength("missing") == 0, "Missing duration differs");
    Check(memoryFile.opens == 1 && memoryFile.closes == 0, "Missing ownership differs");
    Reset();
    memoryFile.missing = true;
    char* pcm = nullptr;
    int bytes = 1;
    int bits = 2;
    int channels = 3;
    int samples = 4;
    Check(!ReadWaveFile("missing", pcm, bytes, bits, channels, samples), "Missing read accepted");
    Check(pcm == nullptr && bytes == 1 && bits == 2 && channels == 3 && samples == 4,
        "Missing read mutates outputs");
    Check(!WriteWaveFile("missing", "", 0, 8, 1, 22050), "Missing write accepted");
    Check(memoryFile.opens == 2 && memoryFile.closes == 0, "Missing voice ownership differs");
}
}

int main() {
    static_assert(sizeof(FormatChunk) == 24, "Win32 WAVE fmt record");
    static_assert(offsetof(FormatChunk, chunkSize) == 4, "Chunk length offset");
    static_assert(offsetof(FormatChunk, dwSamplesPerSec) == 12, "Sample rate offset");
    static_assert(offsetof(FormatChunk, dwAvgBytesPerSec) == 16, "Byte rate offset");
    TestWrite();
    TestRead();
    TestApprox();
    TestMissing();
    std::puts("PASS actual WAVE readers, writer, fixed records and existing read-offset semantics");
}
