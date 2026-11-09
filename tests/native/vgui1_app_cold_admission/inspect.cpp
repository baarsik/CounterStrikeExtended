#include "VGui1AppColdAdmission.h"

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace admission = cse_vgui1_admission;

static void Hex(const unsigned char* bytes, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        std::printf("%02x", bytes[index]);
    }
}

static void String(const char* text) {
    std::putchar('"');
    for (; *text; ++text) {
        const unsigned char value = static_cast<unsigned char>(*text);
        if (value == '"' || value == '\\') {
            std::putchar('\\');
            std::putchar(value);
        } else if (value < 32 || value >= 127) {
            std::printf("\\u%04x", value);
        } else {
            std::putchar(value);
        }
    }
    std::putchar('"');
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const int descriptor = open(argv[1], O_RDONLY | O_CLOEXEC);
    struct stat facts{};
    if (descriptor < 0) {
        return 3;
    }
    if (fstat(descriptor, &facts) != 0 || !S_ISREG(facts.st_mode)
        || facts.st_size <= 0 || static_cast<std::uint64_t>(facts.st_size) > admission::MaximumImageBytes) {
        close(descriptor);
        return 3;
    }
    const auto count = static_cast<std::size_t>(facts.st_size);
    void* bytes = mmap(nullptr, count, PROT_READ, MAP_PRIVATE, descriptor, 0);
    if (bytes == MAP_FAILED) {
        close(descriptor);
        return 4;
    }
    admission::ImageFacts image{};
    const auto status = admission::ReadMachO(static_cast<const unsigned char*>(bytes), count,
        admission::NativeCpu, image);
    munmap(bytes, count);
    close(descriptor);
    if (status != admission::Status::Ready) {
        std::fprintf(stderr, "%s\n", admission::StatusName(status));
        return 5;
    }
    std::printf("{\"bytes\":%llu,\"metadata_offset\":%llu,\"cpu\":%u,\"uuid\":\"",
        static_cast<unsigned long long>(image.fileBytes), static_cast<unsigned long long>(image.metadataOffset),
        image.cpu);
    Hex(image.uuid, sizeof(image.uuid));
    std::printf("\",\"sha256\":\"");
    Hex(image.imageDigest.bytes, sizeof(image.imageDigest.bytes));
    std::printf("\",\"record_sha256\":\"");
    Hex(image.recordDigest.bytes, sizeof(image.recordDigest.bytes));
    std::printf("\",\"provenance\":\"");
    Hex(image.record.provenance.bytes, sizeof(image.record.provenance.bytes));
    const auto& catalog = image.record.catalog;
    std::printf("\",\"kind\":%u,\"catalog\":[%u,%u,%u,%u,%u,%u,%u,%u],\"tables\":[",
        static_cast<unsigned int>(image.record.kind), catalog.descriptorBytes, catalog.descriptorVersion,
        catalog.runtimeVersion, catalog.architecture, catalog.pointerBits, catalog.compilerAbi,
        catalog.tableCount, catalog.reserved);
    for (std::uint32_t index = 0; index < catalog.tableCount; ++index) {
        const auto& table = catalog.tables[index];
        std::printf("%s[%u,%u,%llu]", index ? "," : "", table.identifier, table.reserved,
            static_cast<unsigned long long>(table.signature));
    }
    std::printf("],\"install_name\":");
    String(image.installName);
    std::printf(",\"dependencies\":[");
    for (std::uint32_t index = 0; index < image.dependencyCount; ++index) {
        if (index) {
            std::putchar(',');
        }
        String(image.dependencies[index]);
    }
    std::puts("]}");
    return 0;
}
