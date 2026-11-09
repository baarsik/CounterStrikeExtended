#include "custom_decal_records.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
unsigned checks = 0;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "custom-record:%s\n", message);
        std::exit(17);
    }
}

void Put(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<unsigned char>(value >> (index * 8));
    }
}

std::vector<unsigned char> Load(const char* path) {
    std::ifstream file(path, std::ios::binary);
    Check(file.good(), "open authored WAD");
    std::vector<unsigned char> bytes;
    char value;
    while (file.get(value)) {
        bytes.push_back(static_cast<unsigned char>(value));
    }
    return bytes;
}

void GuardedPrefixes(const std::vector<unsigned char>& bytes) {
#if defined(_WIN32)
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageBytes = info.dwPageSize;
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageBytes * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(memory != nullptr, "owned guarded allocation");
    DWORD previous = 0;
    Check(VirtualProtect(memory + pageBytes, pageBytes, PAGE_NOACCESS, &previous) != 0, "owned guard page");
#else
    const long systemPageBytes = sysconf(_SC_PAGESIZE);
    Check(systemPageBytes > 0, "owned page extent");
    const std::size_t pageBytes = static_cast<std::size_t>(systemPageBytes);
    auto* memory = static_cast<unsigned char*>(mmap(nullptr, pageBytes * 2, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0));
    Check(memory != MAP_FAILED, "owned guarded allocation");
    Check(mprotect(memory + pageBytes, pageBytes, PROT_NONE) == 0, "owned guard page");
#endif
    Check(bytes.size() <= pageBytes, "small authored fixture fits one owned page");
    if (sizeof(void*) > 4) {
        Check(reinterpret_cast<std::uintptr_t>(memory) > UINT32_MAX, "guarded caller has native high address");
    }
    for (std::size_t size = 0; size <= bytes.size(); ++size) {
        unsigned char* span = memory + pageBytes - size;
        if (size) {
            std::memcpy(span, bytes.data(), size);
        }
        cse::CustomWadRecord output{};
        output.lumpOffset = 77;
        const bool admitted = cse::ReadCustomWad(span, size, size, &output);
        Check(admitted == (size == bytes.size()), "actual bounded prefix ending at inaccessible page");
        Check(admitted || output.lumpOffset == 77, "guarded failure preserves output");
    }
    constexpr std::size_t MipOffset = 12;
    constexpr std::size_t MipBytes = 1150;
    for (std::size_t size = 0; size <= MipBytes; ++size) {
        unsigned char* span = memory + pageBytes - size;
        if (size) {
            std::memcpy(span, bytes.data() + MipOffset, size);
        }
        cse::CustomMipRecord output{};
        output.width = 77;
        const bool admitted = cse::ReadCustomMip(span, size, &output);
        Check(admitted == (size == MipBytes), "actual mip prefix ending at inaccessible page");
        Check(admitted || output.width == 77, "guarded mip failure preserves output");
    }
#if defined(_WIN32)
    Check(VirtualFree(memory, 0, MEM_RELEASE) != 0, "release owned guarded allocation");
#else
    Check(munmap(memory, pageBytes * 2) == 0, "release owned guarded allocation");
#endif
}
} // namespace

int main(int argc, char** argv) {
    Check(argc == 2, "authored WAD required");
    const auto bytes = Load(argv[1]);
    cse::CustomWadRecord record{};
    Check(cse::ReadCustomWad(bytes.data(), bytes.size(), bytes.size(), &record), "valid full record");
    Check(record.mip.width == 16 && record.mip.height == 16 && record.mip.paletteColors == 256,
          "independent authored mip values");
    Check(record.mip.offsets[0] == 40 && record.mip.paletteOffset == 382 && record.lumpOffset == 12,
          "fixed disk offsets");
    GuardedPrefixes(bytes);
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        cse::CustomWadRecord output{};
        output.lumpOffset = 77;
        Check(!cse::ReadCustomWad(bytes.data(), size, bytes.size(), &output) && output.lumpOffset == 77,
              "every physical truncation fails without output mutation");
        Check(!cse::ReadCustomWad(bytes.data(), size, size, &output), "every logical truncation rejected");
    }
    Check(!cse::ReadCustomWad(nullptr, bytes.size(), bytes.size(), &record), "null raw span rejected");
    Check(!cse::ReadCustomWad(bytes.data(), bytes.size(), bytes.size(), nullptr), "null output rejected");
    std::vector<unsigned char> trailing = bytes;
    trailing.push_back(0xa5);
    Check(cse::ReadCustomWad(trailing.data(), trailing.size(), bytes.size(), &record),
          "extra initialized backing bytes preserve logical prefix semantics");
    for (unsigned colors = 0; colors <= 256; ++colors) {
        auto palette = bytes;
        constexpr std::size_t CountOffset = 12 + 40 + 340;
        palette[CountOffset] = static_cast<unsigned char>(colors);
        palette[CountOffset + 1] = static_cast<unsigned char>(colors >> 8);
        Check(cse::ReadCustomWad(palette.data(), palette.size(), palette.size(), &record)
            && record.mip.paletteColors == colors, "every retained palette count remains admitted");
    }
    for (std::size_t offset : {4U, 8U}) {
        for (std::uint32_t value : {0U, 2U, 0x7fffffffU, 0x80000000U, 0xffffffffU}) {
            auto bad = bytes;
            Put(bad, offset, value);
            Check(!cse::ReadCustomWad(bad.data(), bad.size(), bad.size(), &record), "bad signed header rejected");
        }
    }
    const std::size_t table = bytes.size() - cse::CustomLumpRecordBytes;
    for (std::size_t offset : {table, table + 4, table + 8}) {
        for (std::uint32_t value : {0U, 0x7fffffffU, 0x80000000U, 0xffffffffU}) {
            auto bad = bytes;
            Put(bad, offset, value);
            Check(!cse::ReadCustomWad(bad.data(), bad.size(), bad.size(), &record), "bad signed lump rejected");
        }
    }
    for (std::size_t offset : {28U, 32U, 36U, 40U, 44U, 48U}) {
        auto bad = bytes;
        Put(bad, offset, 0xffffffffU);
        Check(!cse::ReadCustomWad(bad.data(), bad.size(), bad.size(), &record), "bad mip dimensions/offset rejected");
    }
    int cacheBytes = -1;
    Check(cse::CustomCacheBytes(1150, 56, &cacheBytes) && cacheBytes == 1207, "native cached allocation extent");
    Check(!cse::CustomCacheBytes(0x7fffffffU, 56, &cacheBytes), "cache allocator signed overflow rejected");
    std::printf("custom record checks=%u\n", checks);
    return 0;
}
