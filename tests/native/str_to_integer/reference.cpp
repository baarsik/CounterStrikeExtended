#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#endif
#include <cerrno>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "input_cases.h"

namespace {
#ifdef _WIN32
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

struct Profile {
    const char* name;
#ifdef _WIN32
    _locale_t locale;
#endif
};

long Signed(const Profile& profile, const char* text, char** end) {
#ifdef _WIN32
    if (profile.locale) {
        return _strtol_l(text, end, 10, profile.locale);
    }
#else
    (void)profile;
#endif
    return std::strtol(text, end, 10);
}

unsigned long Unsigned(const Profile& profile, const char* text, char** end) {
#ifdef _WIN32
    if (profile.locale) {
        return _strtoul_l(text, end, 10, profile.locale);
    }
#else
    (void)profile;
#endif
    return std::strtoul(text, end, 10);
}

void Measure(const Profile& profile) {
    const std::string filename = std::string(profile.name) + ".tsv";
    FILE* output = OpenOutput(filename.c_str());
    Require(output != nullptr, "owned measurement output");
    constexpr int kSentinelError = 12345;
    for (const auto& input : InputCases()) {
        for (const int initialError : {0, kSentinelError}) {
            for (const bool keepEnd : {true, false}) {
                for (const bool isUnsigned : {false, true}) {
                    char* end = nullptr;
                    errno = initialError;
                    const std::uint64_t bits = isUnsigned
                        ? static_cast<std::uint64_t>(Unsigned(profile, input.text.c_str(), keepEnd ? &end : nullptr))
                        : static_cast<std::uint64_t>(Signed(profile, input.text.c_str(), keepEnd ? &end : nullptr));
                    const int resultError = errno;
                    const std::ptrdiff_t offset = keepEnd ? end - input.text.c_str() : -1;
                    Require(!keepEnd || (offset >= 0 && static_cast<std::size_t>(offset) <= input.text.size()),
                        "actual CRT endpoint remains within owned input");
                    std::fprintf(output, "%s\t%c\t%d\t%d\t%08x\t%td\t%d\t%016llx\n", input.name,
                        isUnsigned ? 'u' : 's', initialError, keepEnd ? 1 : 0, static_cast<std::uint32_t>(bits),
                        offset, resultError, static_cast<unsigned long long>(bits));
                }
            }
        }
    }
    Require(std::fclose(output) == 0, "measurement output close");
}
}

int main() {
    static_assert(sizeof(int) == 4, "Retained integer result width");
#ifdef _WIN32
    static_assert(sizeof(long) == 4, "Linked UCRT decimal result width");
    FILE* identity = OpenOutput("providers.tsv");
    Require(identity != nullptr, "actual CRT identity output");
    std::fprintf(identity, "compiler\t%u\tptr%zu\tlong%zu\tERANGE=%d\n", _MSC_FULL_VER,
        sizeof(void*), sizeof(long), ERANGE);
    Provider(identity, "strtol", &strtol);
    Provider(identity, "strtoul", &strtoul);
    Provider(identity, "_strtol_l", &_strtol_l);
    Provider(identity, "_strtoul_l", &_strtoul_l);
    Measure({"linked-C", nullptr});
    for (const auto& profile : std::vector<std::pair<const char*, const char*>>{
        {"locale-C", "C"}, {"locale-UTF8", ".UTF-8"}, {"locale-1250", ".1250"}}) {
        _locale_t locale = _create_locale(LC_ALL, profile.second);
        Require(locale != nullptr, "explicit per-call reference locale");
        std::fprintf(identity, "profile\t%s\t%s\n", profile.first, profile.second);
        Measure({profile.first, locale});
        _free_locale(locale);
    }
    Require(std::fclose(identity) == 0, "CRT identity output close");
#else
    FILE* identity = OpenOutput("providers.tsv");
    Require(identity != nullptr, "native control identity output");
    std::fprintf(identity, "compiler\t%s\tptr%zu\tlong%zu\tERANGE=%d\n", __VERSION__,
        sizeof(void*), sizeof(long), ERANGE);
    Require(std::fclose(identity) == 0, "native identity close");
    Measure({"native-C"});
#endif
    return 0;
}
