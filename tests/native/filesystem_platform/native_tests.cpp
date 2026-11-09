#include "basefilesystem.h"
#include <array>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <limits>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s errno=%d\n", message, errno);
        std::abort();
    }
}

std::uint32_t Bits(int value) {
    return static_cast<std::uint32_t>(value);
}

void Positions(CFileSystem_Stdio& fs, const std::filesystem::path& root) {
    const auto path = root / "position.tmp";
    FILE* file = std::fopen(path.c_str(), "w+b");
    Require(file != nullptr, "owned zero-length file");
    constexpr std::array<long long, 7> kPositions{0, 1, INT_MAX - 1LL, INT_MAX, INT_MAX + 1LL, 4294967295LL,
        4294967296LL};
    for (const long long position : kPositions) {
        Require(::fseeko(file, position, SEEK_SET) == 0, "native64 position setup without extending EOF");
        errno = 0;
        const int actual = fs.FS_ftell(file);
        Require(actual == (position <= INT_MAX ? static_cast<int>(position) : -1), "legacy tell boundary");
        Require(errno == (position <= INT_MAX ? 0 : EINVAL), "legacy tell errno contract");
        Require(::ftello(file) == position, "tell failure does not change full native cursor");
    }
    for (const auto& row : std::array<std::array<long long, 3>, 6>{{
        {{0, 0, 0}}, {{2147483647LL, 2147483647LL, 0}}, {{2147483648LL, 0, EINVAL}},
        {{4294967295LL, 0, EINVAL}}, {{4294967296LL, 0, 0}}, {{-2147483649LL, 2147483647LL, 0}}
    }}) {
        Require(::fseeko(file, 0, SEEK_SET) == 0, "reset cursor");
        errno = 0;
        fs.FS_fseek(file, row[0], SEEK_SET);
        Require(::ftello(file) == row[1] && errno == row[2], "exact observed Windows legacy32 seek word");
    }
    int descriptors[2]{};
    Require(::pipe(descriptors) == 0, "owned pipe for real ftell failure");
    FILE* pipeFile = ::fdopen(descriptors[0], "rb");
    Require(pipeFile != nullptr, "owned nonseekable FILE");
    errno = 0;
    Require(fs.FS_ftell(pipeFile) == -1 && errno == ESPIPE, "underlying ftell error survives checked boundary");
    Require(std::fclose(pipeFile) == 0 && ::close(descriptors[1]) == 0, "close both sole pipe references");
    Require(std::fclose(file) == 0, "close sole owned FILE");
    Require(std::filesystem::file_size(path) == 0, "high seek did not allocate or extend file");
    std::filesystem::remove(path);
}

void Sizes(CFileSystem_Stdio& fs, const std::filesystem::path& root) {
    const auto path = root / "sparse.tmp";
    constexpr mode_t kOwnerReadWrite = S_IRUSR | S_IWUSR;
    constexpr long long kAllocatedBlockBytes = 512;
    const int descriptor = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, kOwnerReadWrite);
    Require(descriptor >= 0, "owned sparse-capable APFS file");
    fs.AddSearchPath(root.c_str(), "MODEL");
    for (long long size : {0LL, 2147483647LL, 2147483648LL, 4294967295LL, 4294967296LL, 4294967297LL}) {
        Require(::ftruncate(descriptor, size) == 0, "logical sparse extension only");
        struct stat actual{};
        Require(::fstat(descriptor, &actual) == 0 && actual.st_size == size, "real native logical size");
        constexpr long long kAllocationCap = 1024 * 1024;
        Require(actual.st_blocks * kAllocatedBlockBytes <= kAllocationCap, "real sparse allocation bounded by1MiB");
        struct stat converted;
        std::memset(&converted, 0x5a, sizeof(converted));
        errno = 0;
        const int status = fs.FS_stat(path.c_str(), &converted);
        if (size <= INT_MAX) {
            Require(status == 0 && converted.st_size == size, "representable size matches native stat");
        } else {
            const std::array<unsigned char, sizeof(struct stat)> zero{};
            Require(status == -1 && errno == EOVERFLOW
                && std::memcmp(&converted, zero.data(), sizeof(converted)) == 0,
                "Windows-style stat overflow clears output and fails explicitly");
        }
        Require(fs.FileExists("sparse.tmp") == (size <= INT_MAX),
            "VFS existence follows legacy stat capability");
        Require(fs.Size("sparse.tmp") == (size <= INT_MAX ? static_cast<unsigned int>(size) : UINT_MAX),
            "VFS name-size preserves legacy failure sentinel");
        FileHandle_t opened = fs.Open("sparse.tmp", "rb", "MODEL");
        const unsigned int expectedSize = size <= INT_MAX ? static_cast<unsigned int>(size) : 0;
        Require(opened != nullptr && fs.Size(opened) == expectedSize,
            "retained Open behavior keeps zero metadata after reported stat failure");
        fs.Close(opened);
    }
    fs.RemoveAllSearchPaths();
    Require(::close(descriptor) == 0, "close sole sparse descriptor");
    std::filesystem::remove(path);
    struct stat missing;
    std::memset(&missing, 0x5a, sizeof(missing));
    errno = 0;
    Require(fs.FS_stat(path.c_str(), &missing) == -1 && errno == ENOENT,
        "missing-file error preserved through fallback");
    const std::array<unsigned char, sizeof(struct stat)> zero{};
    Require(std::memcmp(&missing, zero.data(), sizeof(missing)) == 0, "missing stat clears whole native output");
    errno = 0;
    Require(fs.FS_stat(path.c_str(), nullptr) == -1 && errno == EINVAL,
        "null stat output rejected without dereference");
}

class ControlledFileSystem : public CFileSystem_Stdio {
public:
    struct stat supplied{};
    int FS_stat(const char*, struct stat* output) override {
        *output = supplied;
        return 0;
    }
};

void TimeEncoding(const std::filesystem::path& root) {
    ControlledFileSystem fs;
    CBaseFileSystem::CSearchPath path;
    const std::string prefix = root.string() + "/";
    path.m_Path = prefix.c_str();
    path.m_bIsPackFile = false;
    path.m_bAllowWrite = true;
    std::ofstream(root / "small.txt") << "x";
    for (const auto& row : std::array<std::array<long long, 2>, 8>{{
        {{-2147483649LL, 0x7fffffff}}, {{-2147483648LL, 0x80000000}}, {{-1, 0xffffffff}}, {{0, 0}},
        {{2147483647LL, 0x7fffffff}}, {{2147483648LL, 0x80000000}}, {{4294967295LL, 0xffffffff}}, {{4294967296LL, 0}}
    }}) {
        fs.supplied.st_mtime = row[0];
        fs.supplied.st_ctime = row[0];
        fs.supplied.st_size = 1;
        FileHandle_t file = fs.FindFile(&path, "small.txt", "rb", false);
        Require(file != nullptr, "real owned FILE with controlled metadata");
        auto* handle = static_cast<CBaseFileSystem::CFileHandle*>(file);
        Require(Bits(handle->m_nFileTime) == row[1] && Bits(handle->m_nModificationTime) == row[1],
            "explicit signed32 timestamp bit encoding at endpoints/post2038");
        fs.Close(file);
    }
    path.m_bIsPackFile = true;
    auto* packOwner = new CBaseFileSystem::CFileHandle;
    constexpr std::size_t kPackHeaderBytes = 12;
    constexpr std::size_t kPackRecordBytes = 64;
    constexpr std::size_t kPackNameBytes = 56;
    constexpr std::size_t kPayloadBytes = 1;
    constexpr std::size_t kDirectoryOffset = kPackHeaderBytes + kPayloadBytes;
    std::array<char, kPackHeaderBytes + kPayloadBytes + kPackRecordBytes> packBytes{};
    std::memcpy(packBytes.data(), "PACK", 4);
    packBytes[4] = static_cast<char>(kDirectoryOffset);
    packBytes[8] = static_cast<char>(kPackRecordBytes);
    packBytes[kPackHeaderBytes] = 'x';
    std::memcpy(packBytes.data() + kDirectoryOffset, "entry.txt", sizeof("entry.txt"));
    packBytes[kDirectoryOffset + kPackNameBytes] = static_cast<char>(kPackHeaderBytes);
    packBytes[kDirectoryOffset + kPackNameBytes + 4] = static_cast<char>(kPayloadBytes);
    const std::string filename = (root / "model.pak").string();
    {
        std::ofstream output(filename, std::ios::binary);
        output.write(packBytes.data(), static_cast<std::streamsize>(packBytes.size()));
        Require(output.good(), "literal unchanged32bit PACK bytes");
    }
    packOwner->m_pFile = fs.Trace_FOpen(filename.c_str(), "rb", false);
    Require(packOwner->m_pFile != nullptr, "owned traced stream for pack views");
    packOwner->m_nLength = 1;
    path.m_hPackFile = packOwner;
    packOwner->m_nLength = static_cast<long long>(packBytes.size());
    Require(fs.PreparePackFile(path, 0), "actual module builds the pack tree from literal bytes");
    for (const auto& row : std::array<std::array<long long, 2>, 5>{{
        {{-2147483648LL, 0x80000000}}, {{-1, 0xffffffff}}, {{2147483647LL, 0x7fffffff}},
        {{2147483648LL, 0x80000000}}, {{4294967296LL, 0}}
    }}) {
        path.m_lPackFileTime = static_cast<long>(row[0]);
        FileHandle_t view = fs.FindFile(&path, "entry.txt", "rb", false);
        Require(view != nullptr, "owned pack view created");
        auto* handle = static_cast<CBaseFileSystem::CFileHandle*>(view);
        Require(Bits(handle->m_nFileTime) == row[1] && Bits(handle->m_nModificationTime) == row[1],
            "pack time fields preserve the same signed32 word");
        fs.Close(view);
    }
    Require(std::fgetc(packOwner->m_pFile) == 'x', "closing views retains the shared owned pack stream");
    // Return the path to loose lookup; explicitly close its traced owner exactly once.
    path.m_bIsPackFile = false;
    path.m_hPackFile = nullptr;
    fs.Close(packOwner);
    fs.supplied.st_size = INT_MAX;
    Require(fs.FastFindFileSize(&path, "small.txt") == INT_MAX, "fast size maximum boundary");
    fs.supplied.st_size = static_cast<off_t>(INT_MAX) + 1;
    errno = 0;
    Require(fs.FastFindFileSize(&path, "small.txt") == -1 && errno == EOVERFLOW,
        "fast size independently rejects native overflow");
    fs.supplied.st_size = -1;
    errno = 0;
    Require(fs.FastFindFileSize(&path, "small.txt") == -1 && errno == EOVERFLOW,
        "negative controlled native size is not reinterpreted unsigned");
}

void TimeFormatting(CFileSystem_Stdio& fs) {
    Require(::setenv("TZ", "UTC0", 1) == 0, "owned fixture process timezone");
    ::tzset();
    std::ifstream reference(CSE_FS_REFERENCE_LOG);
    Require(reference.is_open(), "actual Windows UTC formatter goldens");
    std::string line;
    int comparisons = 0;
    constexpr char kHex[] = "0123456789abcdef";
    while (std::getline(reference, line)) {
        if (line.compare(0, 4, "fmt\t") != 0) {
            continue;
        }
        std::istringstream row(line);
        std::string kind;
        long value = 0;
        int count = 0;
        std::string expected;
        Require(static_cast<bool>(row >> kind >> value >> count >> expected), "encoded Windows format row");
        std::array<char, 32> actual;
        actual.fill('Z');
        fs.FileTimeToString(actual.data(), count, value);
        std::string encoded;
        for (const char valueByte : actual) {
            const auto byte = static_cast<unsigned char>(valueByte);
            encoded += kHex[byte >> 4];
            encoded += kHex[byte & 15];
        }
        Require(encoded == expected, "all formatted/truncated/padding bytes match real Win32 UTC output");
        ++comparisons;
    }
    Require(comparisons == 12, "all actual Windows formatting goldens consumed");
    for (long value : {-1L, static_cast<long>(INT_MIN), static_cast<long>(INT_MAX) + 1}) {
        char output[8] = "stale";
        errno = 0;
        fs.FileTimeToString(output, sizeof(output), value);
        Require(output[0] == 0 && errno == EINVAL, "unsupported Windows ctime domain fails explicitly");
    }
}

std::string warningCallback;
void WarningCallback(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    char text[8192]{};
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    warningCallback = text;
}

void WarningOutput(CFileSystem_Stdio& fs) {
    fs.SetWarningFunc(WarningCallback);
    for (const FileWarningLevel_t level : {FILESYSTEM_WARNING_QUIET, FILESYSTEM_WARNING_REPORTUNCLOSED,
                                          FILESYSTEM_WARNING_REPORTUSAGE, FILESYSTEM_WARNING_REPORTALLACCESSES}) {
        fs.SetWarningLevel(level);
        fs.Close(nullptr);
        Require(warningCallback == "FS:  Tried to Close NULL file handle!\n",
            "internal unconditional warning stays visible for every declared configured level");
        fs.Warning(FILESYSTEM_WARNING_QUIET, "owned path %s, number=%d\n", "100%ready%n.txt", 42);
        Require(warningCallback == "owned path 100%ready%n.txt, number=42\n",
            "callback treats formatted percent bytes as data");
        fs.Warning(FILESYSTEM_WARNING_REPORTALLACCESSES, "verbose warning\n");
        Require(warningCallback == (level == FILESYSTEM_WARNING_REPORTALLACCESSES ? "verbose warning\n"
            : "owned path 100%ready%n.txt, number=42\n"), "configured filter preserves declared level ordering");
    }
    std::string oversized(5000, 'x');
    fs.Warning(FILESYSTEM_WARNING_QUIET, "%s", oversized.c_str());
    Require(warningCallback == std::string(4095, 'x'), "native warning truncation remains4095 bytes");
}
}

int main(int argc, const char** argv) {
    Require(argc == 3, "owned data root and scenario");
    const std::filesystem::path root = argv[1];
    std::filesystem::create_directories(root);
    CFileSystem_Stdio fs;
    if (std::strcmp(argv[2], "positions") == 0) {
        Positions(fs, root);
    } else if (std::strcmp(argv[2], "sizes") == 0) {
        Sizes(fs, root);
    } else if (std::strcmp(argv[2], "times") == 0) {
        TimeEncoding(root);
    } else if (std::strcmp(argv[2], "format") == 0) {
        TimeFormatting(fs);
    } else {
        Require(std::strcmp(argv[2], "warning") == 0, "known scenario");
        WarningOutput(fs);
    }
    std::puts("filesystem_platform_body_passed=1");
    return 0;
}
