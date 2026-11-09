#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#include <crtdbg.h>
#else
#include <xlocale.h>
#endif
#ifdef CSE_WIDE_HELPER
#include "legacy_wide_format.h"
#endif
#include <array>
#include <cerrno>
#include <cfenv>
#include <clocale>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <utility>
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

constexpr std::size_t kDestinationCells = 64;
constexpr wchar_t kPoison = 0x5a;
constexpr int kSentinelError = 12345;
constexpr int kCountSentinel = 707;
int invalidCalls = 0;
int countOutput = 0;
FILE* output = nullptr;
FILE* metadata = nullptr;

#ifdef _WIN32
#ifndef CSE_WIDE_HELPER
void __cdecl InvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
    ++invalidCalls;
}
#endif
using Locale = _locale_t;
#else
using Locale = locale_t;
#endif
struct Profile {
    const char* name;
    Locale locale;
};
std::vector<Profile> profiles;

template<typename Cell>
std::string Cells(const Cell* cells, std::size_t count) {
    constexpr char kHex[] = "0123456789abcdef";
    constexpr unsigned int kHexDigitsPerCell = 8;
    std::string encoded;
    for (std::size_t index = 0; index < count; ++index) {
        const auto value = static_cast<std::uint32_t>(cells[index]);
        for (unsigned int digit = kHexDigitsPerCell; digit > 0; --digit) {
            encoded += kHex[(value >> ((digit - 1) * 4)) & 15];
        }
    }
    return encoded;
}

int VaFormat(const Profile& profile, wchar_t* destination, std::size_t capacity, const wchar_t* format, ...) {
    va_list arguments;
    va_start(arguments, format);
#ifdef CSE_WIDE_HELPER
    CseLegacyWideFormatContext context;
    context.narrow = std::strcmp(profile.name, "locale-C") == 0 ? CseLegacyWideNarrowEncoding::CByteIdentity
        : CseLegacyWideNarrowEncoding::StrictUtf8;
    const int result = CseLegacyVsnwprintfWithContext(context, destination, capacity, format, arguments);
#elif defined(_WIN32)
    const int result = profile.locale ? _vsnwprintf_l(destination, capacity, format, profile.locale, arguments)
        : _vsnwprintf(destination, capacity, format, arguments);
#else
    const int result = profile.locale ? vswprintf_l(destination, capacity, profile.locale, format, arguments)
        : std::vswprintf(destination, capacity, format, arguments);
#endif
    va_end(arguments);
    return result;
}

template<typename... Arguments>
void Case(const char* name, const wchar_t* windowsFormat, const wchar_t* nativeFormat, Arguments... arguments) {
    const std::string windowCells = Cells(windowsFormat, std::wcslen(windowsFormat));
    const std::string nativeCells = Cells(nativeFormat, std::wcslen(nativeFormat));
    std::fprintf(metadata, "format\t%s\t%s\t%s\n", name, windowCells.c_str(), nativeCells.c_str());
    for (const auto& profile : profiles) {
#ifdef CSE_WIDE_HELPER
        constexpr int apiCount = 1;
        const wchar_t* format = windowsFormat;
#elif defined(_WIN32)
        const int apiCount = profile.locale ? 1 : 3;
        const wchar_t* format = windowsFormat;
#else
        constexpr int apiCount = 1;
        const wchar_t* format = nativeFormat;
#endif
        for (int api = 0; api < apiCount; ++api) {
            for (std::size_t capacity : {0U, 1U, 2U, 3U, 4U, 5U, 8U, 16U, 32U, 64U}) {
                for (const int seed : {0, kSentinelError}) {
                    std::array<wchar_t, kDestinationCells + 2> buffer;
                    buffer.fill(kPoison);
                    countOutput = kCountSentinel;
                    invalidCalls = 0;
                    errno = seed;
                    int result;
                    const char* apiName;
#ifdef CSE_WIDE_HELPER
                    (void)api;
                    apiName = "native-helper-va";
                    result = VaFormat(profile, buffer.data() + 1, capacity, format, arguments...);
#elif defined(_WIN32)
                    if (api == 1) {
                        apiName = "legacy-direct";
                        result = _snwprintf(buffer.data() + 1, capacity, format, arguments...);
                    } else if (api == 2) {
                        apiName = "c99-direct";
                        result = std::swprintf(buffer.data() + 1, capacity, format, arguments...);
                    } else {
                        apiName = "legacy-va";
                        result = VaFormat(profile, buffer.data() + 1, capacity, format, arguments...);
                    }
#else
                    (void)api;
                    apiName = "native-mapped-va";
                    result = VaFormat(profile, buffer.data() + 1, capacity, format, arguments...);
#endif
                    const int resultError = errno;
                    const bool guards = buffer.front() == kPoison && buffer.back() == kPoison;
                    Require(guards, "actual provider stays within owned buffer guards");
                    const std::string cells = Cells(buffer.data(), buffer.size());
                    std::fprintf(output, "%s\t%s\t%s\t%zu\t%d\t%d\t%d\t%d\t%d\t%s\n", name,
                        apiName, profile.name, capacity, seed, result, resultError, invalidCalls, countOutput,
                        cells.c_str());
                }
            }
        }
    }
}

char Byte(unsigned char value) {
    char result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void ByteCorpus() {
    FILE* rows = OpenOutput("bytes.tsv");
    const auto measure = [&](const std::string& name, const std::string& text, int precision,
        std::size_t capacity, const Profile& profile) {
        std::array<wchar_t, 10> cells;
        cells.fill(kPoison);
        errno = kSentinelError;
        const int returned = VaFormat(profile, cells.data() + 1, capacity, L"%.*hs", precision, text.c_str());
        const int error = errno;
        Require(cells.front() == kPoison && cells.back() == kPoison, "byte corpus guards");
        std::fprintf(rows, "%s\t%s\t%d\t%zu\t%d\t%d\t%s\n", name.c_str(), profile.name,
            precision, capacity, returned, error, Cells(cells.data(), cells.size()).c_str());
    };
    for (const auto& profile : profiles) {
        if (std::strcmp(profile.name, "locale-C") != 0 && std::strcmp(profile.name, "locale-UTF8") != 0) {
            continue;
        }
        for (unsigned int byte = 0; byte <= 255; ++byte) {
            const char text[]{Byte(static_cast<unsigned char>(byte)), '7', 0};
            measure("byte-" + std::to_string(byte), text, -1, 8, profile);
        }
        for (int value = -1; value <= 256; ++value) {
            std::array<wchar_t, 10> cells;
            cells.fill(kPoison);
            errno = kSentinelError;
            const int returned = VaFormat(profile, cells.data() + 1, 8, L"[%3C]", value);
            const int error = errno;
            Require(cells.front() == kPoison && cells.back() == kPoison, "character corpus guards");
            std::fprintf(rows, "character-%d\t%s\t-2\t8\t%d\t%d\t%s\n", value, profile.name,
                returned, error, Cells(cells.data(), cells.size()).c_str());
        }
        const std::vector<std::pair<const char*, const char*>> vectors{
            {"two-min", "c280"}, {"two-max", "dfbf"}, {"three-min", "e0a080"},
            {"before-surrogate", "ed9fbf"}, {"after-surrogate", "ee8080"}, {"bmp-max", "efbfbf"},
            {"four-min", "f0908080"}, {"supplementary", "41f09f99825a"}, {"maximum", "f48fbfbf"},
            {"overlong-two", "c080"}, {"overlong-three", "e08080"}, {"overlong-four", "f0808080"},
            {"surrogate-high", "eda080"}, {"surrogate-low", "edb080"}, {"above-maximum", "f4908080"},
            {"lead-five", "f880808080"}, {"short-two", "c2"}, {"short-three", "e0a0"},
            {"short-four", "f09080"}, {"invalid-tail", "41c35a"}, {"continuation", "80"}};
        for (const auto& input : vectors) {
            std::string text;
            constexpr char kHex[] = "0123456789abcdef";
            const std::size_t length = std::strlen(input.second);
            for (std::size_t index = 0; index < length; index += 2) {
                const auto high = static_cast<unsigned int>(std::strchr(kHex, input.second[index]) - kHex);
                const auto low = static_cast<unsigned int>(std::strchr(kHex, input.second[index + 1]) - kHex);
                text.push_back(Byte(static_cast<unsigned char>(high * 16 + low)));
            }
            for (int precision : {-1, 0, 1, 2, 3, 5}) {
                for (std::size_t capacity : {0U, 1U, 2U, 3U, 8U}) {
                    measure(input.first, text, precision, capacity, profile);
                }
            }
        }
    }
    Require(std::fclose(rows) == 0, "byte corpus output close");
}

void Cases() {
    Case("empty", L"", L"");
    Case("literal", L"ABCD", L"ABCD");
    Case("escapes", L"%%s%d %%n%d %d%%", L"%%s%d %%n%d %d%%", 2, 3, 77);
    Case("integer-min", L"%d", L"%d", (std::numeric_limits<int>::min)());
    Case("integer-max", L"%i", L"%i", (std::numeric_limits<int>::max)());
    Case("integer-flags", L"[%+08d][% d][%-7d][%.5d]", L"[%+08d][% d][%-7d][%.5d]", -17, 7, 3, 9);
    Case("integer-unsigned", L"%u %#x %#o", L"%u %#x %#o", (std::numeric_limits<unsigned int>::max)(), 31U, 31U);
    Case("clock", L"%d:%02d\n", L"%d:%02d\n", 3, 7);
    Case("currency", L"$%i", L"$%i", -50);
    Case("progress", L"- %d%% complete", L"- %d%% complete", 99);
    Case("i64-min", L"%I64i", L"%lli", (std::numeric_limits<long long>::min)());
    Case("i64-max", L"%I64i", L"%lli", (std::numeric_limits<long long>::max)());
    Case("i64-high", L"%I64i", L"%lli", 4294967297LL);
    Case("u64-max", L"%I64u", L"%llu", (std::numeric_limits<unsigned long long>::max)());
    const wchar_t wide[]{L'A', 0xe9, 0xd83d, 0xde42, L'Z', 0};
    Case("wide-default", L"%s", L"%ls", wide);
    Case("wide-explicit", L"%ls", L"%ls", wide);
    Case("wide-width", L"[%9.3s]", L"[%9.3ls]", wide);
    Case("wide-precision-one", L"%.1s", L"%.1ls", wide + 2);
    Case("wide-precision-two", L"%.2s", L"%.2ls", wide + 2);
    Case("wide-left", L"[%-8.4ls]", L"[%-8.4ls]", wide);
    Case("wide-star", L"[%*.*s]", L"[%*.*ls]", -8, 3, wide);
    Case("wide-negative-precision", L"[%.*s]", L"[%.*ls]", -1, wide);
    const char ascii[] = "Narrow";
    const char byteAccent[]{'A', Byte(0xe9), 'Z', 0};
    const char utf8Accent[]{'A', Byte(0xc3), Byte(0xa9), 'Z', 0};
    const char utf8Supplementary[]{'A', Byte(0xf0), Byte(0x9f),
        Byte(0x99), Byte(0x82), 'Z', 0};
    const char invalidUtf8[]{'A', Byte(0xc3), 'Z', 0};
    Case("narrow-capital", L"%S", L"%s", ascii);
    Case("narrow-explicit", L"%hs", L"%s", ascii);
    Case("mixed-career", L"%hs / %s", L"%s / %ls", ascii, wide);
    Case("narrow-byte-accent", L"%hs", L"%s", byteAccent);
    Case("narrow-utf8-accent", L"%S", L"%s", utf8Accent);
    Case("narrow-utf8-supplementary", L"%hs", L"%s", utf8Supplementary);
    Case("narrow-utf8-precision-one", L"%.1hs", L"%.1s", utf8Supplementary + 1);
    Case("narrow-utf8-precision-two", L"%.2hs", L"%.2s", utf8Supplementary + 1);
    Case("narrow-utf8-precision-three", L"%.3hs", L"%.3s", utf8Supplementary + 1);
    Case("narrow-invalid-utf8", L"%hs!", L"%s!", invalidUtf8);
    Case("late-invalid-utf8", L"ABCD%hs!", L"ABCD%s!", invalidUtf8);
    Case("precision-invalid-outside", L"%.1hs!", L"%.1s!", invalidUtf8);
    Case("precision-invalid-inside", L"%.2hs!", L"%.2s!", invalidUtf8);
    Case("supplementary-width", L"[%7.1hs]", L"[%7.1s]", utf8Supplementary + 1);
    Case("supplementary-left-width", L"[%-7.3hs]", L"[%-7.3s]", utf8Supplementary + 1);
    Case("empty-precision", L"[%8.0s][%.0hs]", L"[%8.0ls][%.0s]", wide, invalidUtf8);
    Case("integer-star", L"[%*.*d]", L"[%*.*d]", -9, 5, -17);
    Case("integer-negative-precision", L"[%08.*d]", L"[%08.*d]", -1, -17);
    Case("integer-zero-precision", L"[%.0d][%#.0x][%#.0o]", L"[%.0d][%#.0x][%#.0o]", 0, 0U, 0U);
    Case("integer-short", L"%hd %hhu %I32d", L"%hd %hhu %d", -32768, 255, -123);
    Case("integer-native-lengths", L"%ld %zu %td %jd", L"%ld %zu %td %jd", -123L,
        static_cast<std::size_t>(123), static_cast<std::ptrdiff_t>(-123), static_cast<std::intmax_t>(-123));
#if defined(_WIN32) || defined(CSE_WIDE_HELPER)
    Case("pointer-null", L"%p", L"%p", static_cast<void*>(nullptr));
    Case("pointer-value", L"[%020p]", L"[%020p]", reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12ab)));
#endif
    Case("pointer-high", L"%p", L"%p", reinterpret_cast<void*>((std::numeric_limits<std::uintptr_t>::max)()));
    Case("float-lengths", L"%.6lf|%.6Lf|%.1La", L"%.6lf|%.6Lf|%.1La", 1.25,
        static_cast<long double>(-1.25), static_cast<long double>(1.5));
    Case("narrow-character-accent", L"[%C]end", L"[%c]end", 0xe9);
    Case("invalid-char-only", L"%C", L"%c", 0xe9);
    Case("character-nul", L"A%cB", L"A%lcB", 0);
    Case("character-negative-cell", L"%lc", L"%lc", -1);
    Case("integer-small-words", L"%hhd %hd %hhu %hu", L"%hhd %hd %hhu %hu", -129, 65535, -1, -1);
    Case("width-minimum", L"[%*d]", L"[%*d]", (std::numeric_limits<int>::min)(), 7);
    Case("pointer-integers", L"%Id %Iu", L"%td %zu", (std::numeric_limits<std::intptr_t>::min)(),
        (std::numeric_limits<std::uintptr_t>::max)());
    Case("wide-string-modifiers", L"%ws|%lS|%hS", L"%ls|%ls|%s", wide, wide, ascii);
    Case("wide-character-modifiers", L"%wc|%wC|%hC", L"%lc|%lc|%c", 0xe9, 'W', 'Z');
    Case("wide-character-cell", L"%lc", L"%lc", 0xd83d);
    Case("float-nonfinite-flags", L"[%+012f]", L"[%+012f]", (std::numeric_limits<double>::infinity)());
    Case("float-uppercase", L"%F %E %G %A", L"%F %E %G %A", (std::numeric_limits<double>::infinity)(),
        (std::numeric_limits<double>::quiet_NaN)(), -(std::numeric_limits<double>::quiet_NaN)(), 1.5);
    Case("float-hex-default", L"%a", L"%a", 1.5);
    Case("float-hex-explicit", L"%.13a", L"%.13a", 1.5);

    Case("wide-character", L"%c", L"%lc", static_cast<int>(0xe9));
    Case("narrow-character", L"%C", L"%c", static_cast<int>('Q'));
    Case("explicit-characters", L"%lc %hc", L"%lc %c", static_cast<int>(0xe9), static_cast<int>('R'));
    Case("float-promoted-0.1", L"%f", L"%f", static_cast<double>(0.1f));
    Case("float-positive-zero", L"%f", L"%f", 0.0);
    Case("float-negative-zero", L"%f", L"%f", -0.0);
    Case("float-max", L"%f", L"%f", static_cast<double>((std::numeric_limits<float>::max)()));
    Case("float-min", L"%f", L"%f", static_cast<double>((std::numeric_limits<float>::min)()));
    Case("float-precision", L"[%+.9f]", L"[%+.9f]", static_cast<double>(1.2345678f));
    Case("float-infinity", L"%f", L"%f", (std::numeric_limits<double>::infinity)());
    Case("float-nan", L"%f", L"%f", (std::numeric_limits<double>::quiet_NaN)());
    Case("float-negative-nan", L"%f", L"%f", -(std::numeric_limits<double>::quiet_NaN)());
#if defined(_WIN32) || defined(CSE_WIDE_HELPER)
    Case("null-wide", L"%s", L"%ls", static_cast<const wchar_t*>(nullptr));
    Case("null-narrow", L"%hs", L"%s", static_cast<const char*>(nullptr));
    Case("disabled-count", L"A%nB", L"A%nB", &countOutput);
    Case("late-disabled-count", L"ABCD%nB", L"ABCD%nB", &countOutput);
    Case("unknown-format", L"A%qB", L"A%qB", 7);
    Case("trailing-percent", L"AB%", L"AB%");
#endif
    const int previousRounding = std::fegetround();
    for (const auto& mode : std::vector<std::pair<const char*, int>>{
        {"nearest", FE_TONEAREST}, {"upward", FE_UPWARD}, {"downward", FE_DOWNWARD}, {"zero", FE_TOWARDZERO}}) {
        Require(std::fesetround(mode.second) == 0, "owned-process floating rounding control");
        const std::string positive = std::string("rounding-positive-") + mode.first;
        const std::string negative = std::string("rounding-negative-") + mode.first;
        Case(positive.c_str(), L"%.0f", L"%.0f", 1.5);
        Case(negative.c_str(), L"%.0f", L"%.0f", -1.5);
    }
    Require(std::fesetround(previousRounding) == 0, "restore owned-process rounding control");
}
}

int main() {
    output = OpenOutput("results.tsv");
    metadata = OpenOutput("metadata.tsv");
#ifdef CSE_WIDE_HELPER
    profiles.push_back({"locale-C", nullptr});
    profiles.push_back({"locale-UTF8", nullptr});
#elif defined(_WIN32)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    const auto previousHandler = _set_thread_local_invalid_parameter_handler(&InvalidParameter);
    std::fprintf(metadata, "compiler\t%u\tptr%zu\twchar%zu\tlong%zu\tLC=%s\n", _MSC_FULL_VER,
        sizeof(void*), sizeof(wchar_t), sizeof(long), setlocale(LC_ALL, nullptr));
    using Direct = int (__cdecl*)(wchar_t*, std::size_t, const wchar_t*, ...);
    Provider(metadata, "_snwprintf-wrapper", static_cast<Direct>(&_snwprintf));
    Provider(metadata, "swprintf-wrapper", static_cast<Direct>(&swprintf));
    Provider(metadata, "common-vswprintf", &__stdio_common_vswprintf);
    profiles.push_back({"linked-C", nullptr});
    for (const auto& profile : std::vector<std::pair<const char*, const char*>>{
        {"locale-C", "C"}, {"locale-UTF8", ".UTF-8"}, {"locale-1250", ".1250"}, {"locale-Polish", "pl-PL"}}) {
        const auto locale = _create_locale(LC_ALL, profile.second);
        std::fprintf(metadata, "locale\t%s\t%s\t%d\n", profile.first, profile.second, locale != nullptr);
        Require(locale != nullptr, "explicit per-call CRT locale");
        profiles.push_back({profile.first, locale});
    }
    for (const auto& profile : std::vector<std::pair<const char*, const char*>>{
        {"ctype-user", ""}, {"ctype-UTF8", ".UTF-8"}, {"ctype-1250", ".1250"}}) {
        const auto locale = _create_locale(LC_CTYPE, profile.second);
        std::fprintf(metadata, "ctype-locale\t%s\t%s\t%d\n", profile.first, profile.second, locale != nullptr);
        Require(locale != nullptr, "explicit CTYPE-only CRT locale");
        profiles.push_back({profile.first, locale});
    }
#else
    std::fprintf(metadata, "compiler\t%s\tptr%zu\twchar%zu\tlong%zu\tLC=%s\n", __VERSION__,
        sizeof(void*), sizeof(wchar_t), sizeof(long), setlocale(LC_ALL, nullptr));
    for (const auto& profile : std::vector<std::pair<const char*, const char*>>{
        {"locale-C", "C"}, {"locale-UTF8", "en_US.UTF-8"}, {"locale-Polish", "pl_PL.UTF-8"}}) {
        const auto locale = newlocale(LC_ALL_MASK, profile.second, nullptr);
        Require(locale != nullptr, "explicit per-call native control locale");
        profiles.push_back({profile.first, locale});
    }
#endif
#if defined(_WIN32) && !defined(CSE_WIDE_HELPER)
    FILE* nulls = OpenOutput("nulls.tsv");
    for (std::size_t capacity : {std::size_t{0}, std::size_t{1}, std::size_t{64},
        (std::numeric_limits<std::size_t>::max)()}) {
        for (const wchar_t* format : {L"", L"AB"}) {
            invalidCalls = 0;
            errno = kSentinelError;
            const int returned = VaFormat(profiles.front(), nullptr, capacity, format);
            const int error = errno;
            std::fprintf(nulls, "%zu\t%zu\t%d\t%d\t%d\n", capacity, std::wcslen(format), returned,
                error, invalidCalls);
        }
    }
    for (std::size_t capacity : {0U, 1U, 64U}) {
        std::array<wchar_t, kDestinationCells + 2> buffer;
        buffer.fill(kPoison);
        invalidCalls = 0;
        errno = kSentinelError;
        const int returned = VaFormat(profiles.front(), buffer.data() + 1, capacity, nullptr);
        const int error = errno;
        std::fprintf(nulls, "null-format\t%zu\t%d\t%d\t%d\t%s\n", capacity, returned, error,
            invalidCalls, Cells(buffer.data(), buffer.size()).c_str());
    }
    Require(std::fclose(nulls) == 0, "null-destination control output close");
#endif
    Cases();
    ByteCorpus();
    for (const auto& profile : profiles) {
        if (profile.locale) {
#ifdef _WIN32
            _free_locale(profile.locale);
#else
            freelocale(profile.locale);
#endif
        }
    }
#if defined(_WIN32) && !defined(CSE_WIDE_HELPER)
    _set_thread_local_invalid_parameter_handler(previousHandler);
#endif
    Require(std::fclose(output) == 0 && std::fclose(metadata) == 0, "close all reference output");
    return 0;
}
