#include "basefilesystem.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <sys/time.h>

extern "C" int CseFixtureDescend(char*, unsigned int, bool, unsigned int);
namespace {
std::array<timeval, 3> frames{};
std::size_t cursor = 0;
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}

void Clock(bool zeroEpoch) {
    if (zeroEpoch) {
        frames = {{{0, 250000}, {3, 500000}, {4, 125000}}};
    } else {
        constexpr time_t kPost2038Epoch = 2200000000;
        frames = {{{kPost2038Epoch, 250000}, {kPost2038Epoch + 2, 500000}, {kPost2038Epoch - 1, 125000}}};
    }
    CPerformanceCounter counter;
    std::array<double, 3> actual{};
    for (std::size_t index = 0; index < actual.size(); ++index) {
        actual[index] = counter.GetCurTime();
        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(actual[index]), "binary64 clock encoding");
        std::memcpy(&bits, &actual[index], sizeof(bits));
        std::printf("clock_bits[%zu]=%016llx\n", index, static_cast<unsigned long long>(bits));
    }
    std::fflush(stdout);
    Require(actual[0] == 0.25, "exact first-call fractional return");
    Require(actual[1] == (zeroEpoch ? 0.5 : 2.5), "exact native epoch delta and zero sentinel quirk");
    Require(actual[2] == (zeroEpoch ? 1.125 : -0.875), "exact later delta including backward clock");
    Require(cursor == frames.size(), "actual gettimeofday call count");
    std::puts("actual_clock_values_passed=1");
}

void Diagnostics(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::ofstream(root / "unrelated 100%file.txt") << "x";
    const std::string base = root.string() + "/";
    const std::string missing = base + "missing 100%n/leaf.txt";
    std::array<char, 4096> path{};
    Require(missing.size() < path.size() && base.size() - 1 <= std::numeric_limits<unsigned int>::max(),
        "owned diagnostic path fits original index domain");
    std::memcpy(path.data(), missing.c_str(), missing.size() + 1);
    DIR* directory = ::opendir(root.c_str());
    Require(directory != nullptr, "independent native directory enumeration");
    while (dirent* entry = ::readdir(directory)) {
        std::printf("fixture_dir_entry=%s\n", entry->d_name);
    }
    Require(::closedir(directory) == 0, "close sole directory reference");
    Require(CseFixtureDescend(path.data(), static_cast<unsigned int>(base.size() - 1), false, UINT_MAX) == 0,
        "actual descent rejects missing owned basename");
    Require(std::strcmp(path.data(), missing.c_str()) == 0, "diagnostic path bytes restored after traversal");
    ClearPathMatchCache();
    std::puts("actual_path_diagnostics_passed=1");
}
}

extern "C" int CseFixtureGettimeofday(timeval* output, void*) {
    Require(cursor < frames.size(), "bounded controlled clock sequence");
    *output = frames[cursor++];
    return 0;
}

int main(int argc, const char** argv) {
    Require(argc == 3, "scenario and owned diagnostic root");
    if (std::strcmp(argv[1], "high-clock") == 0) {
        Clock(false);
    } else if (std::strcmp(argv[1], "zero-clock") == 0) {
        Clock(true);
    } else {
        Require(std::strcmp(argv[1], "diagnostics") == 0, "known scalar scenario");
        Diagnostics(argv[2]);
    }
    return 0;
}
