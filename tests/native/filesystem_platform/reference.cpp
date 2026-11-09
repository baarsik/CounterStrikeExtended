#include <Windows.h>
#include <winioctl.h>
#include <bcrypt.h>
#if defined(_DEBUG)
#include <crtdbg.h>
#endif
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s error=%lu errno=%d\n", message, GetLastError(), errno);
        std::exit(1);
    }
}

int seekResult = 0;
int seekError = 0;
long seekArgument = 0;
int assertions = 0;
int invalidParameters = 0;
void InvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
    ++invalidParameters;
}

int CapturedSeek(FILE* file, long position, int origin) {
    seekArgument = position;
    seekResult = std::fseek(file, position, origin);
    seekError = errno;
    return seekResult;
}

void CapturedAssertion(const wchar_t*, const wchar_t*, unsigned int) {
    ++assertions;
}

class LowLevel {
public:
    void FS_fseek(FILE* file, long long position, int origin);
    int FS_ftell(FILE* file);
    int FS_stat(const char* path, struct stat* data);
};
#define FILESYSTEM_STDIO_SOURCE L"actual-production-method-control"
#define _wassert CapturedAssertion
#define fseek CapturedSeek
#include "actual_low_level.h"
#undef fseek
#undef _wassert
#undef FILESYSTEM_STDIO_SOURCE

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
    constexpr std::size_t kReadChunkBytes = 64 * 1024;
    std::vector<unsigned char> buffer(kReadChunkBytes);
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
    constexpr std::size_t kSha256Bytes = 32;
    unsigned char digest[kSha256Bytes]{};
    Require(BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0, "SHA256 finish");
    Require(BCryptDestroyHash(hash) >= 0 && BCryptCloseAlgorithmProvider(algorithm, 0) >= 0, "SHA256 release");
    return Hex(digest, sizeof(digest));
}

class Formatter {
public:
    void FileTimeToString(char* output, int capacity, long timestamp);
};
#include "actual_formatter.h"

void Formatting() {
    Require(_putenv_s("TZ", "UTC0") == 0, "owned formatter process timezone normalization");
    _tzset();
    Formatter formatter;
    for (long value : {0L, 1L, 2147483647L}) {
        for (int capacity : {1, 2, 26, 32}) {
            char output[32];
            std::memset(output, 'Z', sizeof(output));
            formatter.FileTimeToString(output, capacity, value);
            std::printf("fmt\t%ld\t%d\t%s\n", value, capacity,
                Hex(reinterpret_cast<const unsigned char*>(output), sizeof(output)).c_str());
        }
    }
}

void Provider(const char* name, const void* address) {
    HMODULE module = nullptr;
    Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<const wchar_t*>(address), &module) != FALSE, "actual CRT owner");
    wchar_t path[32768]{};
    Require(GetModuleFileNameW(module, path, static_cast<DWORD>(sizeof(path) / sizeof(path[0]))) > 0, "CRT image path");
    char utf8[32768]{};
    Require(WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr) > 0, "CRT path UTF8");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const unsigned char*>(module)
        + dos->e_lfanew);
    std::printf("provider\t%s\t%s\t%s\t%04x\n", name, utf8, HashFile(path).c_str(),
        static_cast<unsigned int>(nt->FileHeader.Machine));
}

template<typename Function>
void Provider(const char* name, Function function) {
    const void* address = nullptr;
    static_assert(sizeof(address) == sizeof(function), "Windows callable pointer width");
    std::memcpy(&address, &function, sizeof(address));
    Provider(name, address);
}

void Positions(const char* path) {
    FILE* file = nullptr;
    Require(fopen_s(&file, path, "w+b") == 0, "owned zero-length position file");
    LowLevel low;
    for (long long position : {0LL, 1LL, 2147483646LL, 2147483647LL, 2147483648LL,
                               4294967295LL, 4294967296LL, -1LL}) {
        errno = 0;
        const int set = _fseeki64(file, position, SEEK_SET);
        const int setError = errno;
        errno = 0;
        const int tell = low.FS_ftell(file);
        const int tellError = errno;
        errno = 0;
        const long long tell64 = _ftelli64(file);
        std::printf("tell\t%lld\t%d\t%d\t%08x\t%d\t%lld\n", position, set, setError,
            static_cast<unsigned int>(tell), tellError, tell64);
    }
    for (long long position : {0LL, 2147483647LL, 2147483648LL, 4294967295LL, 4294967296LL, -2147483649LL}) {
        Require(_fseeki64(file, 0, SEEK_SET) == 0, "reset owned position");
        errno = 0;
        low.FS_fseek(file, position, SEEK_SET);
        std::printf("seek\t%lld\t%ld\t%d\t%d\t%lld\t%d\n", position, seekArgument, seekResult,
            seekError, _ftelli64(file), assertions);
    }
    Require(std::fclose(file) == 0 && DeleteFileA(path) != FALSE, "release owned zero-length file");
}

void SparseSizes(const char* path) {
    constexpr unsigned long long kLowWatermark = 16ULL * 1024 * 1024 * 1024;
    constexpr LONGLONG kMaximumAllocatedBytes = 1024 * 1024;
    LowLevel low;
    for (long long size : {0LL, 2147483647LL, 2147483648LL, 4294967295LL, 4294967296LL, 4294967297LL}) {
        ULARGE_INTEGER available{};
        Require(GetDiskFreeSpaceExA("C:\\", &available, nullptr, nullptr) != FALSE
            && available.QuadPart >= kLowWatermark, "16GiB free-space low-watermark before sparse metadata probe");
        HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(file != INVALID_HANDLE_VALUE, "owned sparse file create");
        DWORD returned = 0;
        Require(DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr) != FALSE,
            "explicit NTFS sparse flag");
        BY_HANDLE_FILE_INFORMATION attributes{};
        Require(GetFileInformationByHandle(file, &attributes) != FALSE
            && (attributes.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0, "actual NTFS sparse attribute");
        FILE_STANDARD_INFO before{};
        Require(GetFileInformationByHandleEx(file, FileStandardInfo, &before, sizeof(before)) != FALSE
            && before.AllocationSize.QuadPart <= kMaximumAllocatedBytes, "bounded allocation before logical extension");
        LARGE_INTEGER end{};
        end.QuadPart = size;
        Require(SetFilePointerEx(file, end, nullptr, FILE_BEGIN) != FALSE && SetEndOfFile(file) != FALSE,
            "sparse logical size only");
        FILE_STANDARD_INFO info{};
        Require(GetFileInformationByHandleEx(file, FileStandardInfo, &info, sizeof(info)) != FALSE,
            "actual sparse allocation information");
        Require(info.AllocationSize.QuadPart <= kMaximumAllocatedBytes, "sparse probe never allocates gigabytes");
        errno = 0;
        struct stat data;
        std::memset(&data, 0x5a, sizeof(data));
        const auto original = data;
        const int status = low.FS_stat(path, &data);
        std::printf("stat\t%lld\t%d\t%d\t%lld\t%08x\t%lld\n", size, status, errno,
            static_cast<long long>(data.st_size), static_cast<unsigned int>(data.st_size),
            info.AllocationSize.QuadPart);
        bool zero = true;
        const auto* bytes = reinterpret_cast<const unsigned char*>(&data);
        for (std::size_t index = 0; index < sizeof(data); ++index) {
            zero = zero && bytes[index] == 0;
        }
        std::printf("stat_output_unchanged\t%lld\t%d\tall_zero=%d\n", size,
            std::memcmp(&original, &data, sizeof(data)) == 0, zero);
        Require(CloseHandle(file) != FALSE && DeleteFileA(path) != FALSE, "release sparse owned file");
    }
    struct stat missing;
    std::memset(&missing, 0x5a, sizeof(missing));
    errno = 0;
    const int missingStatus = low.FS_stat(path, &missing);
    const int missingError = errno;
    bool zero = true;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&missing);
    for (std::size_t index = 0; index < sizeof(missing); ++index) {
        zero = zero && bytes[index] == 0;
    }
    std::printf("stat_missing\t%d\t%d\tall_zero=%d\n", missingStatus, missingError, zero);
}

void Times(const char* path) {
    HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(file != INVALID_HANDLE_VALUE, "owned timestamp file");
    constexpr long long kFileTimeUnixEpoch = 116444736000000000LL;
    constexpr long long kFileTimeTicksPerSecond = 10000000;
    LowLevel low;
    for (long long seconds : {-2147483649LL, -2147483648LL, -1LL, 0LL, 1LL, 2147483646LL,
                              2147483647LL, 2147483648LL, 4294967295LL, 4294967296LL}) {
        ULARGE_INTEGER ticks{};
        ticks.QuadPart = static_cast<unsigned long long>(kFileTimeUnixEpoch + seconds * kFileTimeTicksPerSecond);
        FILETIME timestamp{ticks.LowPart, ticks.HighPart};
        Require(SetFileTime(file, &timestamp, &timestamp, &timestamp) != FALSE, "set only owned file timestamps");
        struct stat data{};
        errno = 0;
        const int result = low.FS_stat(path, &data);
        const int error = errno;
        const int legacyTime = static_cast<int>(data.st_ctime > data.st_mtime ? data.st_ctime : data.st_mtime);
        const int legacyModification = static_cast<int>(data.st_mtime);
        std::printf("raw_time_word\t%lld\t%08x\n", seconds,
            static_cast<unsigned int>(static_cast<int>(seconds)));
        const time_t encoded = legacyTime;
        errno = 0;
        char* converted = std::ctime(&encoded);
        const int ctimeError = errno;
        std::printf("time\t%lld\t%d\t%d\t%lld\t%lld\t%08x\t%08x\t%d\t%d\n", seconds, result, error,
            static_cast<long long>(data.st_ctime), static_cast<long long>(data.st_mtime),
            static_cast<unsigned int>(legacyTime), static_cast<unsigned int>(legacyModification),
            converted != nullptr, ctimeError);
    }
    Require(CloseHandle(file) != FALSE && DeleteFileA(path) != FALSE, "release owned timestamp file");
}
}

int main(int argc, const char** argv) {
    Require(argc == 2, "owned NTFS directory argument");
    _set_thread_local_invalid_parameter_handler(InvalidParameter);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    std::printf("compiler\t%u\tptr%zu\tlong%zu\toff%zu\ttime%zu\n", _MSC_FULL_VER, sizeof(void*), sizeof(long),
        sizeof(((struct stat*)nullptr)->st_size), sizeof(time_t));
    Provider("ftell", &ftell);
    Provider("fseek", &fseek);
    Provider("_ctime64", &_ctime64);
    char volumePath[MAX_PATH]{};
    char volumeType[64]{};
    Require(GetVolumePathNameA(argv[1], volumePath, sizeof(volumePath)) != FALSE
        && GetVolumeInformationA(volumePath, nullptr, 0, nullptr, nullptr, nullptr,
            volumeType, sizeof(volumeType)) != FALSE && std::strcmp(volumeType, "NTFS") == 0,
        "explicit local NTFS fixture volume");
    std::printf("volume_type\t%s\n", volumeType);
    ULARGE_INTEGER available{};
    Require(GetDiskFreeSpaceExA(argv[1], &available, nullptr, nullptr) != FALSE, "actual NTFS free space");
    constexpr unsigned long long kMinimumFreeBytes = 16ULL * 1024 * 1024 * 1024;
    std::printf("free_bytes\t%llu\n", available.QuadPart);
    if (available.QuadPart < kMinimumFreeBytes) {
        std::fputs("16GiB low-watermark: no owned file probes run\n", stderr);
        return 77;
    }
    std::puts("mode\tbounded_sparse_metadata\tmaximum_allocated_bytes=1048576\tno_gap_writes=1");
    const std::string prefix = std::string(argv[1]) + "\\";
    Positions((prefix + "position.tmp").c_str());
    SparseSizes((prefix + "sparse.tmp").c_str());
    Times((prefix + "time.tmp").c_str());
    Formatting();
    std::printf("invalid_parameter_calls\t%d\n", invalidParameters);
    return 0;
}
