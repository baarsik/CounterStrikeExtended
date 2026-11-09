#include "tier0/platform.h"
#include "vcr_bindings.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <sys/time.h>
#include <vector>

extern "C" void Plat_SetCommandLine(const char* command);
extern "C" void Plat_SetCommandLineArgs(char** arguments, int count);

namespace {
constexpr std::size_t kCommandCapacity = 2048;
constexpr std::int64_t kNormalEpoch = 1000000;
// Nonzero low bits also catch an incorrectly truncated base that does not equal the zero sentinel.
constexpr std::int64_t kHighEpoch = (std::int64_t{1} << 40) + 123;
constexpr std::int64_t kMillisecondsWrapSeconds = 4294967;
constexpr double kHookReturn = -17.25;
timeval sample{};
int hookCalls = 0;
double hookInput = 0;

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", description);
        std::abort();
    }
}

std::uint64_t Bits(double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "Binary64 fixture encoding");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void CheckFloat(const char* label, double value, double expected) {
    std::printf("%s=%016llx\n", label, static_cast<unsigned long long>(Bits(value)));
    std::fflush(stdout);
    Require(Bits(value) == Bits(expected), label);
}

void SetTime(std::int64_t seconds, int microseconds) {
    using Seconds = decltype(sample.tv_sec);
    Require(seconds >= std::numeric_limits<Seconds>::min() && seconds <= std::numeric_limits<Seconds>::max(),
        "fixture seconds fit the actual timeval storage");
    sample.tv_sec = static_cast<Seconds>(seconds);
    sample.tv_usec = microseconds;
}

double TimeHook(double input) {
    ++hookCalls;
    hookInput = input;
    return kHookReturn;
}

VCR_t hooks{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, TimeHook};

void TimeCase(bool high) {
    const auto epoch = high ? kHighEpoch : kNormalEpoch;
    SetTime(epoch, 500000);
    CheckFloat("float-first", Plat_FloatTime(), 0.5);
    SetTime(epoch + 2, 250000);
    CheckFloat("float-delta", Plat_FloatTime(), 2.25);
    SetTime(epoch, 1);
    CheckFloat("float-microsecond", Plat_FloatTime(), 0.000001);
    SetTime(epoch - 1, 500000);
    CheckFloat("float-backward", Plat_FloatTime(), -0.5);
    SetTime(epoch + 1, 999999);
    Require(Plat_MSTime() == 0, "MSTime first-call fractional-second truncation");
    SetTime(epoch + 2, 500000);
    const auto delta = Plat_MSTime();
    Require(delta == 1500, "independent MSTime base and elapsed milliseconds");
    SetTime(epoch + 1, 999);
    Require(Plat_MSTime() == 0, "submillisecond fraction truncates");
    SetTime(epoch + 1, 1001);
    Require(Plat_MSTime() == 1, "millisecond fraction truncates");
    SetTime(epoch, 999000);
    const auto backward = Plat_MSTime();
    Require(backward == std::numeric_limits<unsigned int>::max(), "negative millisecond modulo encoding");
    SetTime(epoch + 1 + kMillisecondsWrapSeconds, 296000);
    Require(Plat_MSTime() == 0, "32-bit millisecond rollover");
    SetTime(epoch + 1 + kMillisecondsWrapSeconds, 297000);
    Require(Plat_MSTime() == 1, "millisecond value after rollover");
    std::printf("ms-first=0 ms-delta=%u ms-backward=%u ms-wrap=0 ms-after-wrap=1\n", delta, backward);
    Require(hookCalls == 0, "disabled VCR hook");
}

void VcrCase() {
    g_VCRMode = VCR_Record;
    SetTime(kNormalEpoch, 250000);
    CheckFloat("record-first", Plat_FloatTime(), 0.25);
    Require(hookCalls == 0, "first call skips enabled VCR hook");
    SetTime(kNormalEpoch + 1, 500000);
    CheckFloat("record-return", Plat_FloatTime(), kHookReturn);
    CheckFloat("record-input", hookInput, 1.5);
    Require(hookCalls == 1, "record hook called once");
    g_VCRMode = VCR_Playback;
    SetTime(kNormalEpoch + 2, 250000);
    CheckFloat("playback-return", Plat_FloatTime(), kHookReturn);
    CheckFloat("playback-input", hookInput, 2.25);
    Require(hookCalls == 2, "playback hook called once");
    g_VCRMode = VCR_Disabled;
    SetTime(kNormalEpoch + 3, 750000);
    CheckFloat("disabled-return", Plat_FloatTime(), 3.75);
    SetTime(kNormalEpoch, 999999);
    Require(Plat_MSTime() == 0 && hookCalls == 2, "MSTime does not dispatch the float hook");
}

void ZeroEpoch() {
    g_VCRMode = VCR_Record;
    SetTime(0, 250000);
    CheckFloat("zero-first", Plat_FloatTime(), 0.25);
    SetTime(0, 500000);
    CheckFloat("zero-second", Plat_FloatTime(), 0.5);
    SetTime(1, 750000);
    CheckFloat("nonzero-first", Plat_FloatTime(), 0.75);
    Require(hookCalls == 0, "zero sentinel retains repeated first-call behavior");
    SetTime(1, 500000);
    CheckFloat("nonzero-hook", Plat_FloatTime(), kHookReturn);
    CheckFloat("nonzero-input", hookInput, 0.5);
    Require(hookCalls == 1, "hook begins only after a nonzero base is established");
    SetTime(0, 250000);
    Require(Plat_MSTime() == 0, "MSTime zero-base first call");
    SetTime(0, 500000);
    Require(Plat_MSTime() == 0, "MSTime zero sentinel repeats first-call behavior");
    SetTime(1, 750000);
    Require(Plat_MSTime() == 0, "MSTime first nonzero base still returns the first-call value");
    SetTime(1, 500000);
    Require(Plat_MSTime() == 500 && hookCalls == 1, "MSTime begins elapsed milliseconds without VCR dispatch");
    std::puts("ms-zero=0,0,0,500");
}

void CheckCommand(const char* label, std::vector<std::string> arguments, const std::string& expected) {
    std::vector<char*> pointers;
    for (auto& argument : arguments) {
        pointers.push_back(argument.data());
    }
    Require(arguments.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "bounded argument count");
    Plat_SetCommandLineArgs(pointers.empty() ? nullptr : pointers.data(), static_cast<int>(pointers.size()));
    const auto* actual = Plat_GetCommandLine();
    Require(std::strlen(actual) == expected.size() && expected.size() < kCommandCapacity, "bounded command length");
    Require(std::memcmp(actual, expected.c_str(), expected.size() + 1) == 0, label);
    std::printf("%s[%zu]=", label, expected.size());
    for (const unsigned char byte : expected) {
        std::printf("%02x", byte);
    }
    std::putchar('\n');
}

void CommandValid() {
    Plat_SetCommandLine("reset this");
    CheckCommand("no-args", {}, "");
    CheckCommand("empty", {""}, " ");
    CheckCommand("two-empty", {"", ""}, "  ");
    CheckCommand("unquoted", {"a b", "\"c\"", "", "d"}, "a b \"c\"  d ");
    CheckCommand("last-space-fits", {std::string(kCommandCapacity - 2, 'a')},
        std::string(kCommandCapacity - 2, 'a') + ' ');
}

void CommandLimits() {
    CheckCommand("exact-content-limit", {std::string(kCommandCapacity - 1, 'a')},
        std::string(kCommandCapacity - 1, 'a'));
    CheckCommand("long-argument", {std::string(kCommandCapacity * 2, 'b')},
        std::string(kCommandCapacity - 1, 'b'));
    CheckCommand("prefix-space-retained", {std::string(kCommandCapacity - 2, 'c'), "tail"},
        std::string(kCommandCapacity - 2, 'c') + ' ');
    CheckCommand("many-empty", std::vector<std::string>(kCommandCapacity + 1),
        std::string(kCommandCapacity - 1, ' '));
    CheckCommand("reset-after-full", {}, "");
}
}

extern "C" {
VCRMode g_VCRMode = VCR_Disabled;
VCR_t* g_pVCR = &hooks;

int CseFixtureGettimeofday(timeval* output, void*) {
    *output = sample;
    return 0;
}
}

int main(int argc, const char** argv) {
    Require(argc == 2, "one scenario required");
    if (std::strcmp(argv[1], "time-normal") == 0) {
        TimeCase(false);
    } else if (std::strcmp(argv[1], "time-high") == 0) {
        TimeCase(true);
    } else if (std::strcmp(argv[1], "vcr") == 0) {
        VcrCase();
    } else if (std::strcmp(argv[1], "zero-epoch") == 0) {
        ZeroEpoch();
    } else if (std::strcmp(argv[1], "command-valid") == 0) {
        CommandValid();
    } else if (std::strcmp(argv[1], "command-limits") == 0) {
        CommandLimits();
    } else if (std::strcmp(argv[1], "live") == 0) {
        const double first = Plat_FloatTime();
        Require(std::isfinite(first) && first >= 0 && first < 1, "actual gettimeofday first fractional result");
        Require(Plat_MSTime() == 0, "actual MSTime first-call quirk");
    } else {
        Require(false, "unknown scenario");
    }
    std::puts("actual_posix_platform_scenario_passed=1");
    return 0;
}
