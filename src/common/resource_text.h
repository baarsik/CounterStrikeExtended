#ifndef CSE_COMMON_RESOURCE_TEXT_H
#define CSE_COMMON_RESOURCE_TEXT_H

#include "legacy_text_encoding.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cse {
namespace resource_text {

// Disk UTF-16 uses two-byte cells on every platform, independently of wchar_t.
// Files without a Unicode BOM retain the byte encoding expected by legacy callers.
inline bool Decode(const void* data, std::size_t bytes, std::string& output) {
    output.clear();
    if (bytes == 0) {
        return true;
    }
    if (!data) {
        return false;
    }

    constexpr unsigned char BomFirst = 0xff;
    constexpr unsigned char BomSecond = 0xfe;
    constexpr std::size_t Utf16CellBytes = sizeof(std::uint16_t);
    constexpr unsigned int ByteBits = CHAR_BIT;
    const unsigned char* input = static_cast<const unsigned char*>(data);
    const bool littleEndian = bytes >= Utf16CellBytes && input[0] == BomFirst && input[1] == BomSecond;
    const bool bigEndian = bytes >= Utf16CellBytes && input[0] == BomSecond && input[1] == BomFirst;
    if (!littleEndian && !bigEndian) {
        constexpr unsigned char Utf8Bom[] = {0xef, 0xbb, 0xbf};
        constexpr std::size_t Utf8BomBytes = sizeof(Utf8Bom);
        const bool utf8Bom = bytes >= Utf8BomBytes && input[0] == Utf8Bom[0] && input[1] == Utf8Bom[1]
            && input[2] == Utf8Bom[2];
        const std::size_t skip = utf8Bom ? Utf8BomBytes : 0;
        output.assign(reinterpret_cast<const char*>(input + skip), bytes - skip);
        return true;
    }

    if (bytes % Utf16CellBytes != 0) {
        return false;
    }
    const std::size_t cellCount = (bytes - Utf16CellBytes) / Utf16CellBytes;
    // A single UTF-16 cell needs at most three UTF-8 bytes; pairs need four.
    constexpr std::size_t MaximumUtf8BytesPerCell = 3;
    if (cellCount > (static_cast<std::size_t>(INT_MAX) - 1) / MaximumUtf8BytesPerCell) {
        return false;
    }
    std::vector<std::uint16_t> cells(cellCount + 1, 0);
    for (std::size_t index = 0; index < cellCount; ++index) {
        const std::size_t offset = (index + 1) * Utf16CellBytes;
        const std::uint16_t first = input[offset];
        const std::uint16_t second = input[offset + 1];
        cells[index] = littleEndian ? first | (second << ByteBits) : (first << ByteBits) | second;
    }
    const std::size_t capacity = cellCount * MaximumUtf8BytesPerCell + 1;
    output.resize(capacity);
    const int written = legacy_text::WideCellsToUtf8(cells.data(), &output[0], static_cast<int>(capacity));
    if (written == 0) {
        output.clear();
        return false;
    }
    output.resize(static_cast<std::size_t>(written - 1));
    return true;
}

} // namespace resource_text
} // namespace cse

#endif
