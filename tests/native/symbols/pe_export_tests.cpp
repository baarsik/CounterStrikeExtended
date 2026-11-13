#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "../../../src/common/pe_exports.h"
#include "pe_test_image.h"

namespace {
using namespace fixture;

bool Rejected(const std::vector<unsigned char> &data) {
    return !cse::pe::VisitExportNames(data.data(), data.size(), [](cse::pe::ExportName) { return true; });
}

bool CheckBounds(bool pe64) {
    const auto valid = MakeImage(pe64);
    for (std::size_t size = 0; size < valid.size(); ++size) {
        if (cse::pe::VisitExportNames(valid.data(), size, [](cse::pe::ExportName) { return true; })) {
            std::fprintf(stderr, "Truncated PE accepted at %zu bytes\n", size);
            return false;
        }
    }
    auto bad = valid;
    Put16(bad, 0, 0);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put32(bad, 60, UINT32_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put32(bad, PeOffset, 0);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put16(bad, OptionalOffset, UINT16_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put16(bad, PeOffset + SignatureBytes + 2, UINT16_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put32(bad, RawOffset + ExportNameCountOffset, UINT32_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put32(bad, RawOffset + ExportNamesRvaOffset, UINT32_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put32(bad, RawOffset + NamesOffset, UINT32_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    std::fill(bad.begin() + RawOffset + TextOffset, bad.end(), 'x');
    if (!Rejected(bad)) {
        return false;
    }
    const bool canceled = cse::pe::VisitExportNames(valid.data(), valid.size(),
        [](cse::pe::ExportName) { return false; });
    if (canceled) {
        return false;
    }
    bad = valid;
    Put16(bad, PeOffset + SignatureBytes + 16, 2);
    if (!Rejected(bad)) {
        return false;
    }
    const auto directoryCount = OptionalOffset + (pe64 ? 108 : 92);
    bad = valid;
    Put32(bad, directoryCount, UINT32_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    const auto section = OptionalOffset + (pe64 ? 240 : 224);
    bad = valid;
    Put32(bad, section + 12, UINT32_MAX);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    Put16(bad, PeOffset + SignatureBytes + 2, 2);
    constexpr std::size_t SectionHeaderBytes = 40;
    std::memcpy(bad.data() + section + SectionHeaderBytes, bad.data() + section, SectionHeaderBytes);
    if (!Rejected(bad)) {
        return false;
    }
    // Names need not be adjacent or belong to a section named .rdata.
    auto fragmented = valid;
    constexpr std::size_t SecondTextOffset = 256;
    const char second[] = "?Touch@OwnedEntity@@QEAAXPEAV1@@Z";
    Put32(fragmented, RawOffset + ExportNameCountOffset, 2);
    Put32(fragmented, RawOffset + NamesOffset + sizeof(std::uint32_t), SectionRva + SecondTextOffset);
    std::memcpy(fragmented.data() + RawOffset + SecondTextOffset, second, sizeof(second));
    std::memcpy(fragmented.data() + section, ".edata", 7);
    std::vector<std::string> names;
    if (!cse::pe::VisitExportNames(fragmented.data(), fragmented.size(), [&](cse::pe::ExportName name) {
            names.emplace_back(name.data, name.size);
            return true;
        }) || names.size() != 2 || names[1] != second) {
        return false;
    }
    auto empty = valid;
    Put32(empty, RawOffset + ExportNameCountOffset, 0);
    unsigned int callbacks = 0;
    if (!cse::pe::VisitExportNames(empty.data(), empty.size(), [&](cse::pe::ExportName) {
            ++callbacks;
            return true;
        }) || callbacks != 0) {
        return false;
    }
    for (const bool noDirectory : {false, true}) {
        empty = valid;
        if (noDirectory) {
            Put32(empty, directoryCount, 0);
        } else {
            Put32(empty, directoryCount + sizeof(std::uint32_t), 0);
            Put32(empty, directoryCount + 2 * sizeof(std::uint32_t), 0);
        }
        if (!cse::pe::VisitExportNames(empty.data(), empty.size(), [](cse::pe::ExportName) { return false; })) {
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) {
            return 1;
        }
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
        return !file.bad() && cse::pe::VisitExportNames(bytes.data(), bytes.size(), [](cse::pe::ExportName name) {
            return std::fwrite(name.data, 1, name.size, stdout) == name.size && std::putchar('\n') != EOF;
        }) ? 0 : 1;
    }
    for (const bool pe64 : {false, true}) {
        const auto image = MakeImage(pe64);
        std::vector<std::string> names;
        const bool valid = cse::pe::VisitExportNames(image.data(), image.size(),
            [&](cse::pe::ExportName name) {
                names.emplace_back(name.data, name.size);
                return true;
            });
        if (!valid || names != std::vector<std::string>{"?Think@OwnedEntity@@QEAAXXZ"}) {
            std::fprintf(stderr, "%s export names were not decoded\n", pe64 ? "PE32+" : "PE32");
            return 1;
        }
        if (!CheckBounds(pe64)) {
            std::fprintf(stderr, "%s bounds or fragmented-name test failed\n", pe64 ? "PE32+" : "PE32");
            return 1;
        }
    }
    std::puts("PE32 and PE32+ export names decoded");
    return 0;
}
