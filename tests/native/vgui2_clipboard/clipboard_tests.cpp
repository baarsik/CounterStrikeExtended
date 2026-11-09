#include "NativeClipboard.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
using namespace cse_vgui2_clipboard;
constexpr std::int32_t CountSentinel = 173;
constexpr std::uint64_t BytesSentinel = UINT64_C(987);
constexpr unsigned char GuardByte = 0xa5;
unsigned int checks = 0;

void Require(bool okay, const char* label) {
    ++checks;
    if (!okay) {
        std::fprintf(stderr, "FAIL %s\n", label);
        std::exit(1);
    }
}

void BasicViews() {
    const unsigned char bytes[] = {0x41, 0, 0xa9, 3, 0, 0, 0xb1, 0xb2, 0xb3};
    SnapshotView snapshot{9, true, 17, true, {bytes, sizeof(bytes)}, 8};
    std::int32_t count = CountSentinel;
    Require(CountLegacyText(snapshot, count) == Status::Ok && count == 17, "independent allocation count");
    snapshot.unicodeUnitExtentBytes = 9;
    snapshot.unicodeStorage = {nullptr, std::numeric_limits<std::uint64_t>::max()};
    count = CountSentinel;
    Require(CountLegacyText(snapshot, count) == Status::Ok && count == 17,
        "malformed unrelated Unicode cannot invalidate Count");
    snapshot.legacyTextStorageBytes = static_cast<std::uint64_t>(INT32_MAX) + 1;
    count = CountSentinel;
    Require(CountLegacyText(snapshot, count) == Status::ResultOutOfRange && count == CountSentinel,
        "oversized count rejects without out mutation");
    snapshot.legacyTextStorageBytes = INT32_MAX;
    Require(CountLegacyText(snapshot, count) == Status::Ok && count == INT32_MAX, "maximum signed32 count");
    snapshot.hasLegacyTextCount = false;
    count = CountSentinel;
    Require(CountLegacyText(snapshot, count) == Status::MissingFormat && count == CountSentinel,
        "missing count view is a typed failure");
    snapshot = {9, true, 17, true, {bytes, sizeof(bytes)}, 9};
    std::array<unsigned char, 32> output;
    output.fill(GuardByte);
    count = CountSentinel;
    Require(ReadUnicodeBytes(snapshot, 0, {output.data(), 3}, count) == Status::Ok && count == 3,
        "raw view ignores malformed unit extent");
    Require(output[0] == 0x41 && output[1] == 0 && output[2] == 0xa9 && output[3] == GuardByte,
        "raw odd-byte copy and guard");
    std::array<wchar_t, 12> wide;
    wide.fill(static_cast<wchar_t>(0x5a5a));
    count = CountSentinel;
    Require(ReadUnicodeCells(snapshot, 0, {wide.data(), sizeof(wide)}, count) == Status::MalformedUnicode
        && count == CountSentinel && wide[0] == 0x5a5a, "odd declared unit extent rejects transactionally");
    snapshot.unicodeUnitExtentBytes = 8;
    Require(ReadUnicodeCells(snapshot, 0, {wide.data(), sizeof(wide)}, count) == Status::Ok && count == 4,
        "complete cells include embedded NUL tail");
    Require(wide[0] == 0x41 && wide[1] == 0x03a9 && wide[2] == 0 && wide[3] == 0xb2b1
        && wide[4] == 0x5a5a, "widen units and leave spare destination unchanged");
    wide.fill(static_cast<wchar_t>(0x5a5a));
    Require(ReadUnicodeCells(snapshot, 1, {wide.data(), sizeof(wchar_t)}, count) == Status::Ok && count == 1
        && wide[0] == 0x03a9 && wide[1] == 0x5a5a, "wide offset is units; short output need not fit NUL");
    const unsigned char unterminated[] = {0x41, 0, 0x42, 0};
    snapshot.unicodeStorage = {unterminated, sizeof(unterminated)};
    snapshot.unicodeUnitExtentBytes = sizeof(unterminated);
    wide.fill(static_cast<wchar_t>(0x5a5a));
    count = CountSentinel;
    Require(ReadUnicodeCells(snapshot, 0, {wide.data(), sizeof(wide)}, count) == Status::MalformedUnicode
        && count == CountSentinel && wide[0] == 0x5a5a, "unterminated units fail before prefix mutation");
    Require(ReadUnicodeBytes(snapshot, 0, {output.data(), sizeof(output)}, count) == Status::Ok && count == 4,
        "raw view does not require text NUL");
}

void Packing() {
    const unsigned char raw[] = {0x41, 0, 0x80, 0x5a};
    std::array<unsigned char, 32> output;
    output.fill(GuardByte);
    std::uint64_t written = BytesSentinel;
    Require(PlanLegacyBytes(4, written) == Status::Ok && written == 5, "raw plan exact explicit length");
    written = BytesSentinel;
    Require(PackLegacyBytes({raw, sizeof(raw)}, 4, {output.data(), output.size()}, written) == Status::Ok
        && written == 5, "pack explicit raw8 bytes");
    const unsigned char expectedRaw[] = {0x41, 0, 0x80, 0x5a, 0};
    Require(std::memcmp(output.data(), expectedRaw, sizeof(expectedRaw)) == 0 && output[5] == GuardByte,
        "raw pack retains embedded NUL/tail and one terminator");
    const wchar_t input[] = {0x41, 0, 0xd83d, 0xde00};
    const unsigned char expectedWide[] = {0x41, 0, 0, 0, 0x3d, 0xd8, 0, 0xde, 0, 0};
    output.fill(GuardByte);
    Require(PlanWideCells({input, sizeof(input)}, 4, written) == Status::Ok && written == sizeof(expectedWide),
        "wide plan counts UTF16 wire bytes");
    Require(PackWideCells({input, sizeof(input)}, 4, {output.data(), output.size()}, written) == Status::Ok
        && written == sizeof(expectedWide), "pack units without lossy UTF8 bridge");
    Require(std::memcmp(output.data(), expectedWide, sizeof(expectedWide)) == 0
        && output[sizeof(expectedWide)] == GuardByte, "wide wire bytes exact and guard preserved");
    output.fill(GuardByte);
    written = BytesSentinel;
    Require(PackWideCells({input, sizeof(input)}, 4, {output.data(), sizeof(expectedWide) - 1}, written)
        == Status::InsufficientStorage && written == BytesSentinel && output[0] == GuardByte,
        "insufficient pack is transactional");
    Require(PlanLegacyBytes(INT32_MAX, written) == Status::Ok
        && written == UINT64_C(2147483648), "plan adds in uint64 before any legacy int overflow");
    for (std::int32_t length : {INT32_MIN, -1, 0}) {
        written = BytesSentinel;
        Require(PlanLegacyBytes(length, written) == Status::InvalidArgument && written == BytesSentinel,
            "invalid setter length does not change plan");
    }
    if (sizeof(wchar_t) == 4) {
        const wchar_t scalar[] = {static_cast<wchar_t>(0x1f600)};
        const unsigned char expected[] = {0x3d, 0xd8, 0, 0xde, 0, 0};
        Require(PackWideCells({scalar, sizeof(scalar)}, 1, {output.data(), output.size()}, written) == Status::Ok
            && written == sizeof(expected) && std::memcmp(output.data(), expected, sizeof(expected)) == 0,
            "native32 scalar expands to two units");
        for (wchar_t invalid : {static_cast<wchar_t>(-1), static_cast<wchar_t>(0x110000)}) {
            const wchar_t values[] = {L'A', 0, invalid};
            output.fill(GuardByte);
            written = BytesSentinel;
            Require(PackWideCells({values, sizeof(values)}, 3, {output.data(), output.size()}, written)
                == Status::InvalidScalar && written == BytesSentinel && output[0] == GuardByte,
                "late invalid scalar rejects before any output");
        }
    }
}

void Boundaries() {
    const unsigned char bytes[] = {0x41, 0, 0, 0, 0x42, 0, 0xd8, 0};
    SnapshotView snapshot{1, true, 9, true, {bytes, sizeof(bytes)}, sizeof(bytes)};
    std::array<unsigned char, 40> output;
    std::array<wchar_t, 12> wide;
    for (std::int32_t offset : {INT32_MIN, -1, 9, INT32_MAX}) {
        output.fill(GuardByte);
        const auto before = output;
        std::int32_t count = CountSentinel;
        const Status status = ReadUnicodeBytes(snapshot, offset, {output.data(), output.size()}, count);
        Require(status != Status::Ok && count == CountSentinel && output == before,
            "invalid raw offsets preserve entire destination and count");
    }
    for (std::int32_t offset : {INT32_MIN, -1, 5, INT32_MAX}) {
        wide.fill(static_cast<wchar_t>(0x5a5a));
        const auto before = wide;
        std::int32_t count = CountSentinel;
        Require(ReadUnicodeCells(snapshot, offset, {wide.data(), sizeof(wide)}, count) != Status::Ok
            && count == CountSentinel && wide == before, "invalid wide offsets preserve whole output");
    }
    output.fill(GuardByte);
    std::int32_t count = CountSentinel;
    Require(ReadUnicodeBytes(snapshot, 8, {output.data(), output.size()}, count) == Status::Ok && count == 0
        && output[0] == GuardByte, "exact byte end is an empty successful read");
    wide.fill(static_cast<wchar_t>(0x5a5a));
    Require(ReadUnicodeCells(snapshot, 4, {wide.data(), sizeof(wide)}, count) == Status::Ok && count == 0
        && wide[0] == 0x5a5a, "exact unit end is an empty successful read");
    Require(ReadUnicodeCells(snapshot, 2, {wide.data(), sizeof(wide)}, count) == Status::Ok && count == 2
        && wide[0] == L'B' && wide[1] == 0xd8,
        "tail after embedded NUL remains raw units, with no forced terminator");
    for (std::uint64_t capacity = 0; capacity < sizeof(wchar_t); ++capacity) {
        wide.fill(static_cast<wchar_t>(0x5a5a));
        const auto before = wide;
        count = CountSentinel;
        Require(ReadUnicodeCells(snapshot, 0, {wide.data(), capacity}, count) == Status::InvalidArgument
            && count == CountSentinel && wide == before, "sub-cell capacity never makes a partial write");
    }
    std::memset(wide.data(), GuardByte, sizeof(wide));
    Require(ReadUnicodeCells(snapshot, 0, {wide.data(), sizeof(wchar_t) + 1}, count) == Status::Ok && count == 1
        && wide[0] == L'A', "odd physical capacity admits only complete native cells");
    const auto* rawWide = reinterpret_cast<const unsigned char*>(wide.data());
    for (std::size_t index = sizeof(wchar_t); index < sizeof(wide); ++index) {
        Require(rawWide[index] == GuardByte, "unreported destination bytes stay unchanged");
    }
    for (std::uint64_t extent : {UINT64_C(0), UINT64_C(1), UINT64_C(3), UINT64_C(10), UINT64_MAX}) {
        snapshot.unicodeUnitExtentBytes = extent;
        wide.fill(static_cast<wchar_t>(0x5a5a));
        const auto before = wide;
        count = CountSentinel;
        Require(ReadUnicodeCells(snapshot, 0, {wide.data(), sizeof(wide)}, count) != Status::Ok
            && count == CountSentinel && wide == before, "invalid declared unit extents are transactional");
        Require(ReadUnicodeBytes(snapshot, 0, {output.data(), 1}, count) == Status::Ok && count == 1,
            "raw byte view ignores unit validation");
    }
    snapshot.unicodeUnitExtentBytes = sizeof(bytes);
    snapshot.hasUnicode = false;
    count = CountSentinel;
    output.fill(GuardByte);
    const auto before = output;
    Require(ReadUnicodeBytes(snapshot, 0, {output.data(), output.size()}, count) == Status::MissingFormat
        && count == CountSentinel && output == before, "missing raw format preserves all outputs");
    snapshot.hasUnicode = true;
    count = CountSentinel;
    Require(ReadUnicodeBytes(snapshot, 0, {nullptr, 4}, count) == Status::InvalidArgument
        && count == CountSentinel, "null output rejects");
    snapshot.unicodeStorage = {nullptr, 4};
    Require(ReadUnicodeBytes(snapshot, 0, {output.data(), output.size()}, count) == Status::InvalidSpan
        && count == CountSentinel && output == before, "null source rejects before writing");
    snapshot.unicodeStorage = {bytes, UINT64_MAX};
    Require(ReadUnicodeBytes(snapshot, 0, {output.data(), output.size()}, count) == Status::InvalidSpan
        && count == CountSentinel && output == before, "address/size overflow rejects before access");
    snapshot.unicodeStorage = {bytes, sizeof(bytes)};
    Require(ReadUnicodeBytes(snapshot, 0, {output.data(), UINT64_MAX}, count) == Status::InvalidSpan
        && count == CountSentinel && output == before, "destination span overflow rejects before writing");
    const wchar_t source[] = {L'A', L'B'};
    std::uint64_t written = BytesSentinel;
    Require(PlanWideCells({source, sizeof(wchar_t)}, 2, written) == Status::InvalidSpan
        && written == BytesSentinel, "wide planning checks exact input prefix");
    Require(PackLegacyBytes({bytes, 1}, 2, {output.data(), output.size()}, written) == Status::InvalidSpan
        && written == BytesSentinel && output == before, "raw packing checks exact input prefix");
    Require(PackWideCells({source, sizeof(source)}, 2, {output.data(), UINT64_MAX}, written)
        == Status::InvalidSpan && written == BytesSentinel && output == before,
        "wide output range rejection happens before any pack write");
    alignas(wchar_t) unsigned char aligned[32] = {};
    const auto* misaligned = reinterpret_cast<const wchar_t*>(aligned + 1);
    Require(PlanWideCells({misaligned, sizeof(wchar_t)}, 1, written) == Status::InvalidSpan
        && written == BytesSentinel, "misaligned typed input rejects before dereference");
    count = CountSentinel;
    auto* badDestination = reinterpret_cast<wchar_t*>(aligned + 1);
    Require(ReadUnicodeCells(snapshot, 0, {badDestination, sizeof(wchar_t)}, count) == Status::InvalidSpan
        && count == CountSentinel && aligned[1] == 0, "misaligned typed destination rejects");
    std::array<unsigned char, 16> alias;
    alias.fill(GuardByte);
    const auto beforeAlias = alias;
    written = BytesSentinel;
    Require(PackLegacyBytes({alias.data(), alias.size()}, 4, {alias.data() + 1, alias.size() - 1}, written)
        == Status::OverlappingStorage && written == BytesSentinel && alias == beforeAlias,
        "overlapping pack spans reject without writes");
    snapshot.unicodeStorage = {alias.data(), alias.size()};
    count = CountSentinel;
    Require(ReadUnicodeBytes(snapshot, 0, {alias.data() + 1, alias.size() - 1}, count)
        == Status::OverlappingStorage && count == CountSentinel && alias == beforeAlias,
        "overlapping reads reject without memcpy alias UB");
    written = BytesSentinel;
    Require(PackLegacyBytes({reinterpret_cast<const unsigned char*>(&written), sizeof(written)}, 4,
        {output.data(), output.size()}, written) == Status::OverlappingStorage && written == BytesSentinel
        && output == before, "out-count cannot alias relevant source");
    std::vector<unsigned char> highInput{0x41, 0, 0, 0};
    std::vector<wchar_t> highOutput(4, static_cast<wchar_t>(0x5a5a));
#if UINTPTR_MAX > UINT32_MAX
    Require(reinterpret_cast<std::uintptr_t>(highInput.data()) > UINT32_MAX
        && reinterpret_cast<std::uintptr_t>(highOutput.data()) > UINT32_MAX, "owned source/destination above4GiB");
#endif
    snapshot = {55, true, 7, true, {highInput.data(), highInput.size()}, highInput.size()};
    Require(ReadUnicodeCells(snapshot, 0, {highOutput.data(), highOutput.size() * sizeof(wchar_t)}, count)
        == Status::Ok && count == 2 && highOutput[0] == L'A' && highOutput[1] == 0,
        "owned high-address unit read preserves native pointers");
    highInput[0] = 0x42;
    Require(ReadUnicodeCells(snapshot, 0, {highOutput.data(), highOutput.size() * sizeof(wchar_t)}, count)
        == Status::Ok && highOutput[0] == L'B', "fresh call uses supplied bytes even with unchanged generation");
}

void RejectedWidthMetadata() {
#if UINTPTR_MAX > UINT32_MAX
    // These are deliberately unbacked size claims on owned no-access pages,
    // not successful huge data spans. Width/range failures must occur before
    // any source read or destination write, including text validation.
    std::size_t pageBytes = 0;
#if defined(_WIN32)
    SYSTEM_INFO information{};
    GetSystemInfo(&information);
    pageBytes = information.dwPageSize;
    void* sourcePage = VirtualAlloc(nullptr, pageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    void* destinationPage = VirtualAlloc(nullptr, pageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    Require(sourcePage && destinationPage, "owned no-access metadata pages allocated");
#else
    const long nativePageBytes = sysconf(_SC_PAGESIZE);
    Require(nativePageBytes > 0, "native page size for owned no-access guards");
    pageBytes = static_cast<std::size_t>(nativePageBytes);
    void* sourcePage = mmap(nullptr, pageBytes, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    void* destinationPage = mmap(nullptr, pageBytes, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(sourcePage != MAP_FAILED && destinationPage != MAP_FAILED, "owned no-access metadata pages allocated");
#endif
    constexpr std::uint64_t TooManyResults = static_cast<std::uint64_t>(INT32_MAX) + 1;
    SnapshotView metadata{1, false, 0, true,
        {static_cast<const unsigned char*>(sourcePage), TooManyResults}, 0};
    std::int32_t count = CountSentinel;
    Require(ReadUnicodeBytes(metadata, 0, {static_cast<unsigned char*>(destinationPage), TooManyResults}, count)
        == Status::ResultOutOfRange && count == CountSentinel, "raw result-width metadata rejects before any access");
    metadata.unicodeStorage.storageBytes = TooManyResults * 2;
    metadata.unicodeUnitExtentBytes = TooManyResults * 2;
    Require(ReadUnicodeCells(metadata, 0,
        {static_cast<wchar_t*>(destinationPage), TooManyResults * sizeof(wchar_t)}, count)
        == Status::ResultOutOfRange && count == CountSentinel,
        "wide result-width metadata rejects before NUL scan or writing");
#if defined(_WIN32)
    Require(VirtualFree(sourcePage, 0, MEM_RELEASE) && VirtualFree(destinationPage, 0, MEM_RELEASE),
        "owned metadata pages released");
#else
    const int sourceReleased = munmap(sourcePage, pageBytes);
    const int destinationReleased = munmap(destinationPage, pageBytes);
    Require(sourceReleased == 0 && destinationReleased == 0, "owned metadata pages released");
#endif
#endif
}

void RawMatrix() {
    std::vector<unsigned char> input(257);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<unsigned char>(index);
    }
    const SnapshotView snapshot{1, false, 0, true, {input.data(), input.size()}, 3};
    for (std::size_t offset = 0; offset <= input.size(); ++offset) {
        for (std::size_t capacity = 1; capacity <= 17; ++capacity) {
            std::array<unsigned char, 32> output;
            output.fill(GuardByte);
            std::int32_t count = CountSentinel;
            Require(ReadUnicodeBytes(snapshot, static_cast<std::int32_t>(offset),
                {output.data(), capacity}, count) == Status::Ok, "all raw-byte offset/capacity combinations");
            const std::size_t remaining = input.size() - offset;
            const std::size_t expected = remaining < capacity ? remaining : capacity;
            Require(count == static_cast<std::int32_t>(expected), "raw result independent from unit extent/NUL");
            for (std::size_t index = 0; index < output.size(); ++index) {
                Require(output[index] == (index < expected ? input[offset + index] : GuardByte),
                    "all copied bytes and untouched guard bytes");
            }
        }
    }
    std::vector<unsigned char> output(input.size() + 2, GuardByte);
    for (std::size_t length = 1; length <= input.size(); ++length) {
        std::uint64_t bytes = BytesSentinel;
        Require(PackLegacyBytes({input.data(), input.size()}, static_cast<std::int32_t>(length),
            {output.data(), output.size()}, bytes) == Status::Ok && bytes == length + 1,
            "all raw explicit-length prefixes include embedded zeros");
        Require(std::memcmp(input.data(), output.data(), length) == 0 && output[length] == 0,
            "raw pack is not a string transcoder");
    }
}

void ExhaustiveUnits() {
    for (unsigned int value = 0; value <= 0xffff; ++value) {
        const wchar_t input[] = {static_cast<wchar_t>(value)};
        unsigned char bytes[6] = {GuardByte, GuardByte, GuardByte, GuardByte, GuardByte, GuardByte};
        std::uint64_t written = BytesSentinel;
        Require(PackWideCells({input, sizeof(input)}, 1, {bytes, sizeof(bytes)}, written) == Status::Ok
            && written == 4, "every raw16 unit packs including surrogates");
        Require(bytes[0] == static_cast<unsigned char>(value & 0xff)
            && bytes[1] == static_cast<unsigned char>(value >> 8) && bytes[2] == 0 && bytes[3] == 0
            && bytes[4] == GuardByte, "independent raw16 endian oracle");
        const SnapshotView snapshot{1, false, 0, true, {bytes, 4}, 4};
        wchar_t output[3] = {static_cast<wchar_t>(0x5a5a), static_cast<wchar_t>(0x5a5a),
            static_cast<wchar_t>(0x5a5a)};
        std::int32_t count = CountSentinel;
        Require(ReadUnicodeCells(snapshot, 0, {output, sizeof(output)}, count) == Status::Ok && count == 2
            && static_cast<std::uint32_t>(output[0]) == value && output[1] == 0 && output[2] == 0x5a5a,
            "every raw16 unit widens without combining or repair");
    }
}
}

int main() {
    static_assert(std::is_trivial<SnapshotView>::value && std::is_standard_layout<SnapshotView>::value,
        "Snapshot is a borrowed POD view, not an implemented owner lease");
    BasicViews();
    Packing();
    Boundaries();
    RejectedWidthMetadata();
    RawMatrix();
    ExhaustiveUnits();
    std::printf("PASS checks=%u wchar_bytes=%zu; pure component only\n", checks, sizeof(wchar_t));
    return 0;
}
