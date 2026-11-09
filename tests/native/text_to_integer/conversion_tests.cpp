#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "input_cases.h"
#if !defined(CSE_TEXT_LIBC_CONTROL)
#include "legacy_text_to_integer.h"
#endif

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}

std::uint64_t NarrowBits(const char* input, bool singleByte) {
#if defined(CSE_TEXT_LIBC_CONTROL)
    static_cast<void>(singleByte);
    return static_cast<std::uint64_t>(std::strtoll(input, nullptr, 10));
#else
    return static_cast<std::uint64_t>(CseLegacyAtoi64(input, singleByte
        ? CseLegacyIntegerByteWhitespace::SingleByteNbsp : CseLegacyIntegerByteWhitespace::COrUtf8));
#endif
}

std::uint32_t NarrowBits32(const char* input, bool singleByte) {
#if defined(CSE_TEXT_LIBC_CONTROL)
    static_cast<void>(singleByte);
    return static_cast<std::uint32_t>(std::strtol(input, nullptr, 10));
#else
    return static_cast<std::uint32_t>(CseLegacyAtoi(input, singleByte
        ? CseLegacyIntegerByteWhitespace::SingleByteNbsp : CseLegacyIntegerByteWhitespace::COrUtf8));
#endif
}

std::uint32_t WideBits(const wchar_t* input) {
#if defined(CSE_TEXT_LIBC_CONTROL)
    return static_cast<std::uint32_t>(std::wcstol(input, nullptr, 10));
#else
    return static_cast<std::uint32_t>(CseLegacyWtoi(input));
#endif
}

std::uint32_t Read32(FILE* file) {
    unsigned char bytes[4]{};
    Require(std::fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes), "fixed encoded reference table");
    std::uint32_t value = 0;
    for (unsigned int index = 0; index < sizeof(bytes); ++index) {
        value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
    }
    return value;
}

void Cases(const std::string& base, bool singleByte, bool narrow32 = false) {
    std::ifstream file(base + (narrow32 ? "-atoi-cases.tsv" : "-cases.tsv"));
    Require(file.is_open(), "actual Windows reference cases");
    unsigned int count = 0;
    unsigned int mismatches = 0;
    for (const auto& input : InputCases()) {
        if (narrow32 && input.wide) {
            continue;
        }
        std::string name;
        std::string kind;
        std::string line;
        unsigned long long expected = 0;
        int referenceError = 0;
        Require(static_cast<bool>(std::getline(file, line)), "encoded reference line");
        std::istringstream record(line);
        Require(static_cast<bool>(record >> name >> kind >> std::hex >> expected >> std::dec >> referenceError),
            "encoded reference record");
        Require(input.name == name && kind == (input.wide ? "wide32" : narrow32 ? "narrow32" : "narrow64"),
            "reference corpus identity/order");
        std::uint64_t actual = 0;
        constexpr int kUnchangedErrorSentinel = 17;
        errno = kUnchangedErrorSentinel;
        if (input.wide) {
            std::vector<wchar_t> text;
            for (const auto unit : input.units) {
                text.push_back(static_cast<wchar_t>(unit));
            }
            text.push_back(0);
            errno = kUnchangedErrorSentinel;
            actual = WideBits(text.data());
        } else {
            std::vector<char> text;
            for (const auto unit : input.units) {
                const auto byte = static_cast<unsigned char>(unit);
                char encoded;
                std::memcpy(&encoded, &byte, sizeof(encoded));
                text.push_back(encoded);
            }
            text.push_back(0);
            errno = kUnchangedErrorSentinel;
            actual = narrow32 ? NarrowBits32(text.data(), singleByte) : NarrowBits(text.data(), singleByte);
        }
#if !defined(CSE_TEXT_LIBC_CONTROL)
        Require(errno == kUnchangedErrorSentinel, "value-only helper preserves errno");
#endif
        if (actual != expected) {
            if (mismatches < 8) {
                std::fprintf(stderr, "MISMATCH %s %s expected=%016llx actual=%016llx\n",
                    name.c_str(), kind.c_str(), expected, static_cast<unsigned long long>(actual));
            }
            ++mismatches;
        }
        ++count;
    }
    Require(file.peek() == std::char_traits<char>::eof(), "complete reference corpus consumed");
    file.close();
    Require(!file.fail(), "reference corpus close");
    std::printf("corpus_cases=%u mismatches=%u wchar_bytes=%zu long_bytes=%zu\n", count, mismatches,
        sizeof(wchar_t), sizeof(long));
    Require(mismatches == 0, "all encoded values match retained Win32 UCRT");
}

FILE* OpenBinary(const std::string& name) {
    FILE* file = nullptr;
#if defined(_WIN32)
    Require(fopen_s(&file, name.c_str(), "rb") == 0, "open owned reference table");
#else
    file = std::fopen(name.c_str(), "rb");
#endif
    Require(file != nullptr, "owned reference table");
    return file;
}

void PrefixDomain(const std::string& base, bool singleByte) {
    FILE* wide = OpenBinary(base + "-wide-prefix.bin");
    Require(wide != nullptr, "exhaustive Windows BMP reference");
    for (std::uint32_t unit = 0; unit <= 65535; ++unit) {
        wchar_t text[4]{static_cast<wchar_t>(unit), L'7', 0, 0};
        Require(WideBits(text) == Read32(wide), "BMP prefix then digit exact value");
        text[1] = L'+';
        text[2] = L'7';
        Require(WideBits(text) == Read32(wide), "BMP prefix then sign exact value");
    }
    Require(std::fgetc(wide) == EOF && std::fclose(wide) == 0, "complete BMP table");
    FILE* narrow = OpenBinary(base + "-byte-prefix.bin");
    Require(narrow != nullptr, "exhaustive Windows byte reference");
    for (unsigned int unit = 0; unit <= 255; ++unit) {
        const auto byte = static_cast<unsigned char>(unit);
        char text[4]{0, '7', 0, 0};
        std::memcpy(text, &byte, sizeof(byte));
        auto expected = static_cast<std::uint64_t>(Read32(narrow));
        expected |= static_cast<std::uint64_t>(Read32(narrow)) << 32;
        Require(NarrowBits(text, singleByte) == expected, "byte prefix then digit exact value");
        text[1] = '+';
        text[2] = '7';
        expected = Read32(narrow);
        expected |= static_cast<std::uint64_t>(Read32(narrow)) << 32;
        Require(NarrowBits(text, singleByte) == expected, "byte prefix then sign exact value");
    }
    Require(std::fgetc(narrow) == EOF && std::fclose(narrow) == 0, "complete byte table");
    std::puts("exhaustive_prefix_values=131584 passed=1");
}
void AtoiPrefixDomain(const std::string& base, bool singleByte) {
    FILE* file = OpenBinary(base + "-atoi-byte-prefix.bin");
    for (unsigned int unit = 0; unit <= 255; ++unit) {
        const auto byte = static_cast<unsigned char>(unit);
        char text[4]{0, '7', 0, 0};
        std::memcpy(text, &byte, sizeof(byte));
        Require(NarrowBits32(text, singleByte) == Read32(file), "atoi byte prefix then digit exact value");
        text[1] = '+';
        text[2] = '7';
        Require(NarrowBits32(text, singleByte) == Read32(file), "atoi byte prefix then sign exact value");
    }
    Require(std::fgetc(file) == EOF && std::fclose(file) == 0, "complete atoi byte table");
    std::puts("atoi_exhaustive_prefix_values=512 passed=1");
}

void NativeDomain() {
#if WCHAR_MAX > 65535
    constexpr std::uint32_t kFirstNonBmpScalar = 0x10000;
    constexpr std::uint32_t kLastUnicodeScalar = 0x10ffff;
    for (std::uint32_t unit = kFirstNonBmpScalar; unit <= kLastUnicodeScalar; ++unit) {
        wchar_t prefix[3]{static_cast<wchar_t>(unit), L'7', 0};
        Require(WideBits(prefix) == 0, "non-BMP native value is not a UTF16 digit/space/sign");
        wchar_t suffix[4]{L'1', static_cast<wchar_t>(unit), L'7', 0};
        Require(WideBits(suffix) == 1, "non-BMP native value terminates an existing number");
    }
    for (wchar_t unit : {static_cast<wchar_t>(0x7fff0030), static_cast<wchar_t>(0x40000085),
                         (std::numeric_limits<wchar_t>::min)(), static_cast<wchar_t>(-1)}) {
        wchar_t text[3]{unit, L'7', 0};
        Require(WideBits(text) == 0, "high/negative native values do not alias lower UTF16 units");
    }
    std::puts("non_bmp_scalar_positions=2097152 high_negative_controls=4 passed=1");
#else
    std::puts("native_wchar_domain_is_utf16=1 non_bmp_positions_not_representable=1");
#endif
}
}

int main(int argc, const char** argv) {
    Require(argc == 4, "reference prefix, C-or-single-byte policy, cases-or-prefix required");
    const bool singleByte = std::strcmp(argv[2], "single-byte") == 0;
    Require(singleByte || std::strcmp(argv[2], "C") == 0, "explicit measured byte whitespace policy");
    if (std::strcmp(argv[3], "atoi-cases") == 0) {
        Cases(argv[1], singleByte, true);
    } else if (std::strcmp(argv[3], "atoi-prefix") == 0) {
        AtoiPrefixDomain(argv[1], singleByte);
    } else if (std::strcmp(argv[3], "native-domain") == 0) {
        NativeDomain();
    } else if (std::strcmp(argv[3], "cases") == 0) {
        Cases(argv[1], singleByte);
    } else {
        Require(std::strcmp(argv[3], "prefix") == 0, "known scenario");
        PrefixDomain(argv[1], singleByte);
    }
    return 0;
}
