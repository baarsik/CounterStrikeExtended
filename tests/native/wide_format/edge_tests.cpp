#include "legacy_wide_format.h"
#include <array>
#include <atomic>
#include <cerrno>
#include <cfenv>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s errno=%d\n", message, errno);
        std::abort();
    }
}

void Twice(const wchar_t* format, ...) {
    std::array<wchar_t, 64> first{};
    std::array<wchar_t, 64> second{};
    va_list arguments;
    va_start(arguments, format);
    const int left = CseLegacyVsnwprintf(first.data(), first.size(), format, arguments);
    const int right = CseLegacyVsnwprintf(second.data(), second.size(), format, arguments);
    va_end(arguments);
    Require(left == right && std::memcmp(first.data(), second.data(), sizeof(first)) == 0,
        "va_copy leaves the caller list available for a second call");
    Require(std::wcscmp(first.data(), L"17/4294967297/1.250") == 0, "typed mixed variadic arguments");
}

void BoundsAndLifetime() {
    constexpr int kOriginalError = 12345;
    std::array<wchar_t, 32> output;
    output.fill(L'Z');
    errno = kOriginalError;
    Require(CseLegacySnwprintf(nullptr, 0, L"A%d", 7) == 2 && errno == kOriginalError,
        "null destination zero capacity is an actual legacy length query");
    Require(CseLegacySnwprintf(nullptr, 1, L"A") == -1 && errno == EINVAL, "null nonzero capacity rejected");
    errno = kOriginalError;
    Require(CseLegacySnwprintf(output.data(), output.size(), nullptr) == -1 && errno == EINVAL,
        "null format failure");
    for (const auto unit : output) {
        Require(unit == L'Z', "null format preserves all destination cells like UCRT");
    }
    Require(CseLegacySnwprintf(output.data(), (std::numeric_limits<std::size_t>::max)(), L"A") == -1
        && errno == EINVAL && output[0] == L'Z', "unrepresentable object capacity fails before writing");
    {
        const std::wstring input(L"temporary input");
        Require(CseLegacySnwprintf(output.data(), output.size(), L"%s", input.c_str()) == 15,
            "caller-owned temporary argument");
    }
    Require(std::wcscmp(output.data(), L"temporary input") == 0, "output remains owned after argument release");
    Require(CseLegacySnwprintf(output.data(), output.size(), L"%s", L"\U0001F642X") == 3
        && output[0] == 0xd83d && output[1] == 0xde42 && output[2] == L'X',
        "native supplementary scalars and UTF16 inputs produce the same cells");
    Require(CseLegacySnwprintf(output.data(), output.size(), L"%.1s", L"\U0001F642X") == 1
        && output[0] == 0xd83d && output[1] == 0, "wide precision remains a cell boundary");
    Require(CseLegacySnwprintf(output.data(), output.size(), L"\U00010025") == 2
        && output[0] == 0xd800 && output[1] == 0xdc25, "native scalar low word cannot become a format percent");
    Twice(L"%d/%I64d/%.3f", 17, 4294967297LL, 1.25);
    std::array<wchar_t, 10> bounded;
    bounded.fill(L'Z');
    errno = kOriginalError;
    Require(CseLegacySnwprintf(bounded.data() + 1, 8, L"%.*f", INT_MAX, 1.0) == -1
        && errno == kOriginalError, "large requested precision truncates without a precision-sized allocation");
    const wchar_t expected[]{L'Z', L'1', L'.', L'0', L'0', L'0', L'0', L'0', L'0', L'Z'};
    Require(std::memcmp(bounded.data(), expected, sizeof(expected)) == 0, "bounded precision prefix and guards");
    Require(CseLegacySnwprintf(nullptr, 0, L"%.*f", INT_MAX, 1.0) == -1 && errno == EOVERFLOW,
        "unrepresentable length query fails without precision iteration");
}

void ConcurrentContexts() {
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    constexpr int kWorkers = 4;
    for (int worker = 0; worker < kWorkers; ++worker) {
        workers.emplace_back([worker, &failed]() {
            constexpr int kIterations = 200;
            CseLegacyWideFormatContext context;
            const bool byteIdentity = (worker & 1) != 0;
            context.narrow = byteIdentity ? CseLegacyWideNarrowEncoding::CByteIdentity
                : CseLegacyWideNarrowEncoding::StrictUtf8;
            const int mode = worker < 2 ? FE_UPWARD : FE_DOWNWARD;
            const int previous = std::fegetround();
            if (std::fesetround(mode) != 0) {
                failed.store(true);
                return;
            }
            const unsigned char raw[]{0xc3, 0xa9, 0};
            char text[sizeof(raw)];
            std::memcpy(text, raw, sizeof(raw));
            for (int iteration = 0; iteration < kIterations; ++iteration) {
                std::array<wchar_t, 32> output{};
                const int seed = 12345 + worker;
                errno = seed;
                const int written = CseLegacySnwprintfWithContext(context, output.data(), output.size(),
                    L"%hs:%.0f", text, -0.14075685411022312);
                const wchar_t* expected = byteIdentity ? (mode == FE_UPWARD ? L"\x00c3\x00a9:-0" : L"\x00c3\x00a9:-1")
                    : (mode == FE_UPWARD ? L"\x00e9:-0" : L"\x00e9:-1");
                if (written != (byteIdentity ? 5 : 4) || errno != seed || std::wcscmp(output.data(), expected) != 0
                    || std::fegetround() != mode) {
                    failed.store(true);
                    break;
                }
            }
            if (std::fesetround(previous) != 0) {
                failed.store(true);
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    Require(!failed.load(), "concurrent encoding/errno/rounding contexts remain independent");
}
}

int main() {
    BoundsAndLifetime();
    ConcurrentContexts();
    std::puts("PASS: native bounds, fixed-storage precision, va_copy, lifetime and concurrent contexts");
    return 0;
}
