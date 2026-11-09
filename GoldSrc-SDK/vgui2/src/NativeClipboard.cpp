#include "NativeClipboard.h"
#include <cstddef>
#include <cstring>
#include <limits>

namespace cse_vgui2_clipboard {
namespace {

constexpr std::uint64_t Utf16UnitBytes = 2;
constexpr unsigned int BitsPerByte = 8;
constexpr std::uint32_t ByteMask = (1u << BitsPerByte) - 1;
constexpr std::uint64_t TerminatorUnits = 1;
constexpr std::uint64_t SupplementaryUnits = 2;
constexpr std::uint32_t MaximumUnit = 0xffff;
constexpr std::uint32_t MaximumScalar = 0x10ffff;
constexpr std::uint32_t SupplementaryBase = 0x10000;
constexpr unsigned int SurrogatePayloadBits = 10;
constexpr std::uint32_t SurrogatePayloadMask = 0x3ff;
constexpr std::uint32_t HighSurrogateBase = 0xd800;
constexpr std::uint32_t LowSurrogateBase = 0xdc00;

static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "Clipboard cells require wchar16 or wchar32");
static_assert(std::numeric_limits<wchar_t>::max() >= MaximumUnit, "Each native cell must hold any UTF16 unit");
static_assert(sizeof(std::uintptr_t) <= sizeof(std::uint64_t)
    && sizeof(std::size_t) <= sizeof(std::uint64_t), "Span arithmetic requires <=64-bit native sizes");

bool ValidSpan(const void* data, std::uint64_t bytes) noexcept {
    if (bytes == 0) {
        return true;
    }
    if (!data || bytes > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return bytes <= std::numeric_limits<std::uintptr_t>::max() - address;
}

bool Aligned(const wchar_t* data) noexcept {
    return reinterpret_cast<std::uintptr_t>(data) % alignof(wchar_t) == 0;
}

bool Overlap(const void* first, std::uint64_t firstBytes, const void* second,
    std::uint64_t secondBytes) noexcept {
    if (firstBytes == 0 || secondBytes == 0) {
        return false;
    }
    const auto firstAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto secondAddress = reinterpret_cast<std::uintptr_t>(second);
    return firstAddress < secondAddress + secondBytes && secondAddress < firstAddress + firstBytes;
}

std::uint16_t ReadUnit(const unsigned char* bytes, std::uint64_t index) noexcept {
    const std::size_t offset = static_cast<std::size_t>(index * Utf16UnitBytes);
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << BitsPerByte));
}

void WriteUnit(unsigned char* bytes, std::uint64_t& position, std::uint32_t unit) noexcept {
    bytes[static_cast<std::size_t>(position++)] = static_cast<unsigned char>(unit & ByteMask);
    bytes[static_cast<std::size_t>(position++)] = static_cast<unsigned char>(unit >> BitsPerByte);
}

Status ValidateWideInput(WideSpan source, std::int32_t textLength, std::uint64_t& required) noexcept {
    if (textLength <= 0) {
        return Status::InvalidArgument;
    }
    const std::uint64_t cells = static_cast<std::uint64_t>(textLength);
    const std::uint64_t sourceBytes = cells * sizeof(wchar_t);
    if (sourceBytes > source.storageBytes || !ValidSpan(source.data, source.storageBytes) || !Aligned(source.data)) {
        return Status::InvalidSpan;
    }
    std::uint64_t units = TerminatorUnits; // Appended terminator, independent of embedded source NULs.
    for (std::uint64_t index = 0; index < cells; ++index) {
        const auto value = static_cast<std::uint32_t>(source.data[static_cast<std::size_t>(index)]);
        if (value > MaximumScalar) {
            return Status::InvalidScalar;
        }
        units += value <= MaximumUnit ? 1 : SupplementaryUnits;
    }
    // textLength is positive signed32; even two units per cell plus a terminator
    // cannot overflow uint64. Physical size/address limits are checked by Pack.
    required = units * Utf16UnitBytes;
    return Status::Ok;
}

} // namespace

Status CountLegacyText(const SnapshotView& snapshot, std::int32_t& outCount) noexcept {
    if (!snapshot.hasLegacyTextCount) {
        return Status::MissingFormat;
    }
    if (snapshot.legacyTextStorageBytes > static_cast<std::uint64_t>(INT32_MAX)) {
        return Status::ResultOutOfRange;
    }
    outCount = static_cast<std::int32_t>(snapshot.legacyTextStorageBytes);
    return Status::Ok;
}

Status ReadUnicodeBytes(const SnapshotView& snapshot, std::int32_t byteOffset, ByteBuffer destination,
    std::int32_t& outBytes) noexcept {
    if (!snapshot.hasUnicode) {
        return Status::MissingFormat;
    }
    if (byteOffset < 0 || !destination.data || destination.storageBytes == 0) {
        return Status::InvalidArgument;
    }
    const ByteSpan source = snapshot.unicodeStorage;
    if (!ValidSpan(source.data, source.storageBytes) || !ValidSpan(destination.data, destination.storageBytes)) {
        return Status::InvalidSpan;
    }
    const auto offset = static_cast<std::uint64_t>(byteOffset);
    if (offset > source.storageBytes) {
        return Status::OutOfRange;
    }
    const std::uint64_t remaining = source.storageBytes - offset;
    const std::uint64_t copied = remaining < destination.storageBytes ? remaining : destination.storageBytes;
    if (copied > static_cast<std::uint64_t>(INT32_MAX)) {
        return Status::ResultOutOfRange;
    }
    if (Overlap(source.data, source.storageBytes, destination.data, copied)
        || Overlap(&outBytes, sizeof(outBytes), source.data, source.storageBytes)
        || Overlap(&outBytes, sizeof(outBytes), destination.data, copied)) {
        return Status::OverlappingStorage;
    }
    if (copied != 0) {
        std::memcpy(destination.data, source.data + static_cast<std::size_t>(offset), static_cast<std::size_t>(copied));
    }
    outBytes = static_cast<std::int32_t>(copied);
    return Status::Ok;
}

Status ReadUnicodeCells(const SnapshotView& snapshot, std::int32_t unitOffset, WideBuffer destination,
    std::int32_t& outCells) noexcept {
    if (!snapshot.hasUnicode) {
        return Status::MissingFormat;
    }
    if (unitOffset < 0 || !destination.data || destination.storageBytes < sizeof(wchar_t)) {
        return Status::InvalidArgument;
    }
    const std::uint64_t extent = snapshot.unicodeUnitExtentBytes;
    if (extent == 0 || extent % Utf16UnitBytes != 0 || extent > snapshot.unicodeStorage.storageBytes) {
        return Status::MalformedUnicode;
    }
    const unsigned char* source = snapshot.unicodeStorage.data;
    if (!ValidSpan(source, extent) || !ValidSpan(destination.data, destination.storageBytes)
        || !Aligned(destination.data)) {
        return Status::InvalidSpan;
    }
    const std::uint64_t units = extent / Utf16UnitBytes;
    const auto offset = static_cast<std::uint64_t>(unitOffset);
    if (offset > units) {
        return Status::OutOfRange;
    }
    const std::uint64_t capacity = destination.storageBytes / sizeof(wchar_t);
    const std::uint64_t remaining = units - offset;
    const std::uint64_t copied = remaining < capacity ? remaining : capacity;
    if (copied > static_cast<std::uint64_t>(INT32_MAX)) {
        return Status::ResultOutOfRange;
    }
    const std::uint64_t writtenBytes = copied * sizeof(wchar_t);
    if (Overlap(source, extent, destination.data, writtenBytes)
        || Overlap(&outCells, sizeof(outCells), source, extent)
        || Overlap(&outCells, sizeof(outCells), destination.data, writtenBytes)) {
        return Status::OverlappingStorage;
    }
    bool terminated = false;
    for (std::uint64_t index = 0; index < units; ++index) {
        if (ReadUnit(source, index) == 0) {
            terminated = true;
            break;
        }
    }
    if (!terminated) {
        return Status::MalformedUnicode;
    }
    for (std::uint64_t index = 0; index < copied; ++index) {
        destination.data[static_cast<std::size_t>(index)] = static_cast<wchar_t>(ReadUnit(source, offset + index));
    }
    outCells = static_cast<std::int32_t>(copied);
    return Status::Ok;
}

Status PlanLegacyBytes(std::int32_t textLength, std::uint64_t& outStorageBytes) noexcept {
    if (textLength <= 0) {
        return Status::InvalidArgument;
    }
    outStorageBytes = static_cast<std::uint64_t>(textLength) + 1;
    return Status::Ok;
}

Status PackLegacyBytes(ByteSpan source, std::int32_t textLength, ByteBuffer destination,
    std::uint64_t& outStorageBytes) noexcept {
    std::uint64_t required = 0;
    const Status planned = PlanLegacyBytes(textLength, required);
    if (planned != Status::Ok) {
        return planned;
    }
    const auto sourceBytes = static_cast<std::uint64_t>(textLength);
    if (sourceBytes > source.storageBytes || !ValidSpan(source.data, source.storageBytes)) {
        return Status::InvalidSpan;
    }
    if (required > destination.storageBytes) {
        return Status::InsufficientStorage;
    }
    if (!ValidSpan(destination.data, destination.storageBytes)) {
        return Status::InvalidSpan;
    }
    if (Overlap(source.data, source.storageBytes, destination.data, required)
        || Overlap(&outStorageBytes, sizeof(outStorageBytes), source.data, source.storageBytes)
        || Overlap(&outStorageBytes, sizeof(outStorageBytes), destination.data, required)) {
        return Status::OverlappingStorage;
    }
    std::memcpy(destination.data, source.data, static_cast<std::size_t>(sourceBytes));
    destination.data[static_cast<std::size_t>(sourceBytes)] = 0;
    outStorageBytes = required;
    return Status::Ok;
}

Status PlanWideCells(WideSpan source, std::int32_t textLength, std::uint64_t& outStorageBytes) noexcept {
    if (ValidSpan(source.data, source.storageBytes)
        && Overlap(&outStorageBytes, sizeof(outStorageBytes), source.data, source.storageBytes)) {
        return Status::OverlappingStorage;
    }
    std::uint64_t required = 0;
    const Status status = ValidateWideInput(source, textLength, required);
    if (status == Status::Ok) {
        outStorageBytes = required;
    }
    return status;
}

Status PackWideCells(WideSpan source, std::int32_t textLength, ByteBuffer destination,
    std::uint64_t& outStorageBytes) noexcept {
    std::uint64_t required = 0;
    const Status planned = ValidateWideInput(source, textLength, required);
    if (planned != Status::Ok) {
        return planned;
    }
    if (required > destination.storageBytes) {
        return Status::InsufficientStorage;
    }
    if (!ValidSpan(destination.data, destination.storageBytes)) {
        return Status::InvalidSpan;
    }
    if (Overlap(source.data, source.storageBytes, destination.data, required)
        || Overlap(&outStorageBytes, sizeof(outStorageBytes), source.data, source.storageBytes)
        || Overlap(&outStorageBytes, sizeof(outStorageBytes), destination.data, required)) {
        return Status::OverlappingStorage;
    }
    std::uint64_t position = 0;
    for (std::int32_t index = 0; index < textLength; ++index) {
        std::uint32_t value = static_cast<std::uint32_t>(source.data[index]);
        if (value <= MaximumUnit) {
            WriteUnit(destination.data, position, value);
        } else {
            value -= SupplementaryBase;
            WriteUnit(destination.data, position, HighSurrogateBase + (value >> SurrogatePayloadBits));
            WriteUnit(destination.data, position, LowSurrogateBase + (value & SurrogatePayloadMask));
        }
    }
    WriteUnit(destination.data, position, 0);
    outStorageBytes = required;
    return Status::Ok;
}

} // namespace cse_vgui2_clipboard
