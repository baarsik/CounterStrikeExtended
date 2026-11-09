#include <Windows.h>
#include <bcrypt.h>
#include <cerrno>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include "input_cases.h"

namespace {
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
    Require(bytes > 0, "actual CRT version bytes");
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

using Atoi = int (__cdecl*)(const char*);
using Atoi64 = __int64 (__cdecl*)(const char*);
using Wtoi = int (__cdecl*)(const wchar_t*);
using ErrorPointer = int* (__cdecl*)();

template<typename Function>
Function Symbol(HMODULE module, const char* name) {
    const FARPROC symbol = GetProcAddress(module, name);
    Require(symbol != nullptr, "legacy CRT export");
    Function function;
    static_assert(sizeof(function) == sizeof(symbol), "Windows callable width");
    std::memcpy(&function, &symbol, sizeof(function));
    return function;
}

struct Profile {
    const char* name;
    Atoi64 narrow;
    Wtoi wide;
    _locale_t locale = nullptr;
    ErrorPointer error = &_errno;
    Atoi narrow32 = &atoi;
};

std::int64_t Parse(const Profile& profile, const char* value) {
    return profile.locale ? _atoi64_l(value, profile.locale) : profile.narrow(value);
}

std::int32_t Parse(const Profile& profile, const wchar_t* value) {
    return profile.locale ? _wtoi_l(value, profile.locale) : profile.wide(value);
}

int Parse32(const Profile& profile, const char* value) {
    return profile.locale ? _atoi_l(value, profile.locale) : profile.narrow32(value);
}

void Write32(FILE* file, std::uint32_t value) {
    unsigned char bytes[4]{};
    for (unsigned int index = 0; index < sizeof(bytes); ++index) {
        bytes[index] = static_cast<unsigned char>(value >> (index * 8));
    }
    Require(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes), "encoded table write");
}

void Measure(const Profile& profile) {
    const std::string prefix = profile.name;
    FILE* output = nullptr;
    Require(fopen_s(&output, (prefix + "-cases.tsv").c_str(), "wb") == 0, "case output");
    FILE* output32 = nullptr;
    Require(fopen_s(&output32, (prefix + "-atoi-cases.tsv").c_str(), "wb") == 0, "atoi case output");
    for (const auto& input : InputCases()) {
        *profile.error() = 0;
        std::uint64_t bits = 0;
        if (input.wide) {
            std::vector<wchar_t> text;
            for (const auto unit : input.units) {
                Require(unit <= 65535, "Windows UTF16 code-unit domain");
                text.push_back(static_cast<wchar_t>(unit));
            }
            text.push_back(0);
            bits = static_cast<std::uint32_t>(Parse(profile, text.data()));
        } else {
            std::vector<char> text;
            for (const auto unit : input.units) {
                const auto byte = static_cast<unsigned char>(unit);
                char encoded;
                std::memcpy(&encoded, &byte, sizeof(encoded));
                text.push_back(encoded);
            }
            text.push_back(0);
            bits = static_cast<std::uint64_t>(Parse(profile, text.data()));
            const int originalError = *profile.error();
            *profile.error() = 0;
            const auto integer32 = static_cast<std::uint32_t>(Parse32(profile, text.data()));
            std::fprintf(output32, "%s\tnarrow32\t%016llx\t%d\n", input.name,
                static_cast<unsigned long long>(integer32), *profile.error());
            *profile.error() = originalError;
        }
        const int conversionError = *profile.error();
        std::fprintf(output, "%s\t%s\t%016llx\t%d\n", input.name, input.wide ? "wide32" : "narrow64",
            static_cast<unsigned long long>(bits), conversionError);
    }
    Require(std::fclose(output) == 0 && std::fclose(output32) == 0, "case output close");
    Require(fopen_s(&output, (prefix + "-wide-prefix.bin").c_str(), "wb") == 0, "wide table output");
    for (std::uint32_t unit = 0; unit <= 65535; ++unit) {
        wchar_t text[4]{static_cast<wchar_t>(unit), L'7', 0, 0};
        Write32(output, static_cast<std::uint32_t>(Parse(profile, text)));
        text[1] = L'+';
        text[2] = L'7';
        Write32(output, static_cast<std::uint32_t>(Parse(profile, text)));
    }
    Require(std::fclose(output) == 0, "wide table close");
    Require(fopen_s(&output, (prefix + "-byte-prefix.bin").c_str(), "wb") == 0, "byte table output");
    for (unsigned int unit = 0; unit <= 255; ++unit) {
        const auto byte = static_cast<unsigned char>(unit);
        char text[4]{0, '7', 0, 0};
        std::memcpy(text, &byte, sizeof(byte));
        const auto first = static_cast<std::uint64_t>(Parse(profile, text));
        Write32(output, static_cast<std::uint32_t>(first));
        Write32(output, static_cast<std::uint32_t>(first >> 32));
        text[1] = '+';
        text[2] = '7';
        const auto second = static_cast<std::uint64_t>(Parse(profile, text));
        Write32(output, static_cast<std::uint32_t>(second));
        Write32(output, static_cast<std::uint32_t>(second >> 32));
    }
    Require(std::fclose(output) == 0, "byte table close");
    Require(fopen_s(&output32, (prefix + "-atoi-byte-prefix.bin").c_str(), "wb") == 0, "atoi byte table output");
    for (unsigned int unit = 0; unit <= 255; ++unit) {
        const auto byte = static_cast<unsigned char>(unit);
        char text[4]{0, '7', 0, 0};
        std::memcpy(text, &byte, sizeof(byte));
        Write32(output32, static_cast<std::uint32_t>(Parse32(profile, text)));
        text[1] = '+';
        text[2] = '7';
        Write32(output32, static_cast<std::uint32_t>(Parse32(profile, text)));
    }
    Require(std::fclose(output32) == 0, "atoi byte table close");
}
}

int main() {
    static_assert(sizeof(int) == 4 && sizeof(long) == 4 && sizeof(wchar_t) == 2, "retained Windows numeric widths");
    FILE* identity = nullptr;
    Require(fopen_s(&identity, "providers.tsv", "wb") == 0, "provider receipt");
    std::fprintf(identity, "compiler\t%u\tptr%zu\tlong%zu\twchar%zu\n", _MSC_FULL_VER, sizeof(void*),
        sizeof(long), sizeof(wchar_t));
    Provider(identity, "atoi", &atoi);
    Provider(identity, "_atoi64", &_atoi64);
    Provider(identity, "_wtoi", &_wtoi);
    HMODULE legacy = LoadLibraryW(L"msvcrt.dll");
    Require(legacy != nullptr, "separate system legacy CRT");
    const auto legacyNarrow = Symbol<Atoi64>(legacy, "_atoi64");
    const auto legacyWide = Symbol<Wtoi>(legacy, "_wtoi");
    Provider(identity, "msvcrt._atoi64", legacyNarrow);
    Provider(identity, "msvcrt._wtoi", legacyWide);
    Measure({"linked-C", &_atoi64, &_wtoi});
    const auto legacyAtoi = Symbol<Atoi>(legacy, "atoi");
    const auto legacyError = Symbol<ErrorPointer>(legacy, "_errno");
    Measure({"msvcrt-startup", legacyNarrow, legacyWide, nullptr, legacyError, legacyAtoi});
    const int oldMode = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    Require(oldMode != -1, "isolated reference thread locale mode");
    const char* user = setlocale(LC_CTYPE, "");
    Require(user != nullptr, "isolated user-default LC_CTYPE");
    std::fprintf(identity, "user-LC_CTYPE\t%s\n", user);
    Measure({"linked-user", &_atoi64, &_wtoi});
    Require(setlocale(LC_CTYPE, "C") != nullptr, "reference thread locale restore");
    Require(_configthreadlocale(oldMode) != -1, "reference thread locale mode restore");
    for (const char* name : {"C", ".UTF8", "Polish_Poland.1250"}) {
        _locale_t locale = _create_locale(LC_ALL, name);
        Require(locale != nullptr, "explicit reference locale object");
        const std::string label = std::strcmp(name, "C") == 0 ? "linked-locale-C"
            : std::strcmp(name, ".UTF8") == 0 ? "linked-locale-UTF8" : "linked-locale-1250";
        std::fprintf(identity, "%s\t%s\n", label.c_str(), name);
        Measure({label.c_str(), &_atoi64, &_wtoi, locale});
        _free_locale(locale);
    }
    Require(std::fclose(identity) == 0, "provider receipt close");
    Require(FreeLibrary(legacy) != FALSE, "separate legacy CRT reference release");
    std::puts("actual_integer_reference_probe_passed=1");
    return 0;
}
