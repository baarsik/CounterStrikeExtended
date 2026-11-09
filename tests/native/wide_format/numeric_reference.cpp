#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#else
#include <xlocale.h>
#endif
#ifdef CSE_WIDE_HELPER
#include "legacy_wide_format.h"
#endif
#include "numeric_thresholds.h"
#include <array>
#include <cerrno>
#include <cfenv>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
#if defined(_WIN32) && !defined(CSE_WIDE_HELPER)
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s error=%lu\n", message, GetLastError());
        std::abort();
    }
}

std::string Utf8(const wchar_t* value) {
    const int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    Require(count > 0, "UTF8 path count");
    std::vector<char> result(static_cast<std::size_t>(count));
    Require(WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), count, nullptr, nullptr) == count,
        "UTF8 path conversion");
    return result.data();
}

std::string Hex(const unsigned char* data, std::size_t count) {
    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t index = 0; index < count; ++index) {
        text += kHexDigits[data[index] >> 4];
        text += kHexDigits[data[index] & 15];
    }
    return text;
}

std::string HashFile(const wchar_t* path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    Require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "SHA256 provider");
    DWORD objectBytes = 0;
    DWORD copied = 0;
    Require(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
        sizeof(objectBytes), &copied, 0) >= 0, "SHA256 object size");
    std::vector<unsigned char> object(objectBytes);
    BCRYPT_HASH_HANDLE hash = nullptr;
    Require(BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) >= 0, "SHA256 object");
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(file != INVALID_HANDLE_VALUE, "actual provider image open");
    std::vector<unsigned char> buffer(65536);
    for (;;) {
        DWORD count = 0;
        Require(ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) != FALSE,
            "actual provider image read");
        if (count == 0) {
            break;
        }
        Require(BCryptHashData(hash, buffer.data(), count, 0) >= 0, "SHA256 data");
    }
    Require(CloseHandle(file) != FALSE, "provider image close");
    unsigned char digest[32]{};
    Require(BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0, "SHA256 finish");
    Require(BCryptDestroyHash(hash) >= 0 && BCryptCloseAlgorithmProvider(algorithm, 0) >= 0, "SHA256 release");
    return Hex(digest, sizeof(digest));
}

template<typename Function>
void Provider(FILE* output, const char* symbol, Function function) {
    HMODULE module = nullptr;
    const void* address = nullptr;
    static_assert(sizeof(address) == sizeof(function), "Windows function address width");
    std::memcpy(&address, &function, sizeof(address));
    Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<const wchar_t*>(address), &module) != FALSE, "actual CRT function owner");
    wchar_t path[32768]{};
    Require(GetModuleFileNameW(module, path, static_cast<DWORD>(sizeof(path) / sizeof(path[0]))) > 0,
        "actual CRT image path");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const unsigned char*>(module)
        + dos->e_lfanew);
    DWORD ignored = 0;
    const DWORD bytes = GetFileVersionInfoSizeW(path, &ignored);
    if (bytes == 0) {
        std::fprintf(output, "%s\t%s\t%s\t%04x\tno-version-resource\n", symbol, Utf8(path).c_str(),
            HashFile(path).c_str(), nt->FileHeader.Machine);
        return;
    }
    std::vector<unsigned char> version(bytes);
    Require(GetFileVersionInfoW(path, 0, bytes, version.data()) != FALSE, "actual CRT version");
    VS_FIXEDFILEINFO* fixed = nullptr;
    UINT fixedBytes = 0;
    Require(VerQueryValueW(version.data(), L"\\", reinterpret_cast<void**>(&fixed), &fixedBytes) != FALSE,
        "actual CRT version record");
    std::fprintf(output, "%s\t%s\t%s\t%04x\t%u.%u.%u.%u\n", symbol, Utf8(path).c_str(),
        HashFile(path).c_str(), nt->FileHeader.Machine, HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
        HIWORD(fixed->dwFileVersionLS), LOWORD(fixed->dwFileVersionLS));
}

#else
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s errno=%d\n", message, errno);
        std::abort();
    }
}
#endif

FILE* OpenOutput(const char* filename) {
    FILE* output = nullptr;
#ifdef _WIN32
    Require(fopen_s(&output, filename, "wb") == 0, "owned output open");
#else
    output = std::fopen(filename, "wb");
    Require(output != nullptr, "owned output open");
#endif
    return output;
}


constexpr std::size_t kBufferBytes = 2048;
struct Format {
    const char* text;
    const char* native;
    bool extremaOnly = false;
};
}
int main() {
    FILE* result = OpenOutput("numeric.tsv");
    FILE* identity = OpenOutput("numeric-providers.tsv");
#if defined(CSE_WIDE_HELPER)
    std::fprintf(identity, "backend\towned-integer-binary64\n");
#elif defined(_WIN32)
    Provider(identity, "common-vsprintf", &__stdio_common_vsprintf);
    const auto locale = _create_locale(LC_ALL, "C");
#else
    const auto locale = newlocale(LC_ALL_MASK, "C", nullptr);
#endif
#ifndef CSE_WIDE_HELPER
    Require(locale != nullptr, "isolated numeric C locale");
#endif
    const std::vector<Format> formats{{"%f", "%f"}, {"%.0f", "%.0f"}, {"%.17f", "%.17f"},
        {"%+.6f", "%+.6f"}, {"%e", "%e"}, {"%.0e", "%.0e"}, {"%.17e", "%.17e"},
        {"%g", "%g"}, {"%.1g", "%.1g"}, {"%.17g", "%.17g"}, {"%#g", "%#g"},
        {"%a", "%.13a"}, {"%.0a", "%.0a"}, {"%.1a", "%.1a"}, {"%.13a", "%.13a"},
        {"%A", "%.13A"}, {"%G", "%G"}, {"%.0g", "%.0g"}, {"%#.0f", "%#.0f"},
        {"%#.0e", "%#.0e"}, {"%#.17g", "%#.17g"}, {"%+014.6e", "%+014.6e"},
        {"%#.0a", "%#.0a"}, {"%.14a", "%.14a"}, {"%.100f", "%.100f"},
        {"%.100g", "%.100g"}, {"%.100e", "%.100e"}, {"%.1f", "%.1f"},
        {"%#.0g", "%#.0g"}, {"%#.6f", "%#.6f"}, {"%#f", "%#f"}, {"%#.0F", "%#.0F"},
        {"%#.0G", "%#.0G"}, {"%#.0A", "%#.0A"}, {"%#e", "%#e"}, {"%#.1g", "%#.1g"},
        {"%#.0E", "%#.0E"}, {"%.2a", "%.2a"}, {"%.12a", "%.12a"}, {"%#.2a", "%#.2a"},
        {"%+#012.0f", "%+#012.0f"}, {"%.322f", "%.322f", true}, {"%.323f", "%.323f", true},
        {"%.1074f", "%.1074f", true}, {"%.1074g", "%.1074g", true},
        {"%#012.0f", "%#012.0f", true}, {"%+#012.0e", "%+#012.0e", true},
        {"%-#12.0f", "%-#12.0f", true}, {"% #12.0a", "% #12.0a", true},
        {"%+#020.0a", "%+#020.0a", true}, {"%-#20.0A", "%-#20.0A", true},
        {"%#012.1f", "%#012.1f", true}, {"%#020.1a", "%#020.1a", true},
        {"%#012.0g", "%#012.0g", true}, {"%+#012.0G", "%+#012.0G", true}};
    std::vector<std::uint64_t> inputs{0, 0x8000000000000000ULL, 1, 0x000fffffffffffffULL,
        0x0010000000000000ULL, 0x001fffffffffffffULL, 0x3ff0000000000000ULL, 0x3ff8000000000000ULL,
        0x7fefffffffffffffULL, 0xffefffffffffffffULL, 0x3f80000000000000ULL};
    const std::vector<std::uint64_t> boundaryBits{0x7ff0000000000000ULL, 0xfff0000000000000ULL,
        0x7ff8000000000000ULL, 0xfff8000000000000ULL, 0x7ff8000000000001ULL, 0xfff8000000000001ULL,
        0x7ff0000000000001ULL, 0xfff0000000000001ULL, 0x7fffffffffffffffULL, 0xffffffffffffffffULL,
        0x3fe0000000000000ULL, 0xbfe0000000000000ULL, 0x3ff8000000000000ULL, 0xbff8000000000000ULL,
        0x4004000000000000ULL, 0xc004000000000000ULL, 0xbf80000000000000ULL,
        0x3f1a36e2eb1c432cULL, 0x412e848000000000ULL, 0x3fefffffffffffffULL, 0x3ff0000000000001ULL};
    for (const auto bits : boundaryBits) {
        inputs.push_back(bits);
        if ((bits & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL) {
            inputs.push_back(bits - 1);
            inputs.push_back(bits + 1);
        }
    }
    std::uint64_t state = 0xd1b54a32d192ed03ULL;
    constexpr int kRandomValues = 2048;
    for (int index = 0; index < kRandomValues; ++index) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        const auto bits = state * 0x2545f4914f6cdd1dULL;
        if ((bits & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL) {
            inputs.push_back(bits);
        }
    }
    for (const auto bits : ThresholdInputs()) {
        inputs.push_back(bits);
    }
    const int previous = std::fegetround();
    const std::array<int, 4> modes{{FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}};
    for (std::size_t mode = 0; mode < modes.size(); ++mode) {
        Require(std::fesetround(modes[mode]) == 0, "numeric rounding mode");
        for (const auto bits : inputs) {
            double value;
            std::memcpy(&value, &bits, sizeof(value));
            for (std::size_t format = 0; format < formats.size(); ++format) {
                if (formats[format].extremaOnly) {
                    const std::uint64_t magnitude = bits & 0x7fffffffffffffffULL;
                    const bool nonfinite = (magnitude & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL;
                    if (!nonfinite && magnitude != 1 && magnitude != 0x000fffffffffffffULL && magnitude != 0x0010000000000000ULL
                        && magnitude != 0x001fffffffffffffULL && magnitude != 0x7fefffffffffffffULL
                        && magnitude != 0x3fb999999999999aULL) {
                        continue;
                    }
                }
                std::array<char, kBufferBytes> buffer;
                buffer.fill('Z');
                errno = 12345;
#ifdef CSE_WIDE_HELPER
                wchar_t wideFormat[64]{};
                const std::size_t formatBytes = std::strlen(formats[format].text);
                Require(formatBytes < sizeof(wideFormat) / sizeof(wideFormat[0]), "owned test format fits");
                for (std::size_t index = 0; index < formatBytes; ++index) {
                    wideFormat[index] = static_cast<unsigned char>(formats[format].text[index]);
                }
                wchar_t wide[kBufferBytes]{};
                const int returned = CseLegacySnwprintf(wide, kBufferBytes, wideFormat, value);
                const int capturedError = errno;
                Require(returned >= 0 && static_cast<std::size_t>(returned) < buffer.size(), "owned numeric field");
                for (int index = 0; index <= returned; ++index) {
                    Require(wide[index] >= 0 && wide[index] <= 127, "numeric output is ASCII UTF16 cells");
                    buffer[static_cast<std::size_t>(index)] = static_cast<char>(wide[index]);
                }
                errno = capturedError;
#elif defined(_WIN32)
                const int returned = _snprintf_l(buffer.data(), buffer.size(), formats[format].text, locale, value);
#else
                const int returned = snprintf_l(buffer.data(), buffer.size(), locale, formats[format].native, value);
#endif
                const int error = errno;
#if defined(_WIN32) && !defined(CSE_WIDE_HELPER)
                wchar_t wideFormat[64]{};
                const std::size_t formatBytes = std::strlen(formats[format].text);
                Require(formatBytes < sizeof(wideFormat) / sizeof(wideFormat[0]), "wide numeric format fits");
                for (std::size_t index = 0; index < formatBytes; ++index) {
                    wideFormat[index] = static_cast<unsigned char>(formats[format].text[index]);
                }
                wchar_t wide[kBufferBytes]{};
                errno = 12345;
                const int wideReturned = _snwprintf_l(wide, kBufferBytes, wideFormat, locale, value);
                const int wideError = errno;
                Require(wideReturned == returned && wideError == error, "actual UCRT wide/narrow numeric status");
                for (int index = 0; index <= returned; ++index) {
                    Require(wide[index] == static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]),
                        "actual UCRT numeric field has identical UTF16/ASCII cells");
                }
#endif
                Require(returned >= 0 && static_cast<std::size_t>(returned) < buffer.size(),
                    "finite field fits owned numeric buffer");
                std::fprintf(result, "%zu\t%016llx\t%zu\t%d\t%d\t%s\n", mode,
                    static_cast<unsigned long long>(bits), format, returned, error, buffer.data());
            }
        }
    }
    Require(std::fesetround(previous) == 0, "restore numeric rounding mode");
#if defined(CSE_WIDE_HELPER)
#elif defined(_WIN32)
    _free_locale(locale);
#else
    freelocale(locale);
#endif
    Require(std::fclose(result) == 0 && std::fclose(identity) == 0, "numeric receipt close");
    return 0;
}
