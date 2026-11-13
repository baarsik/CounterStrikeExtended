#ifndef CSE_ENGINE_LOCALIZATION_CENTER_TEXT_H
#define CSE_ENGINE_LOCALIZATION_CENTER_TEXT_H

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace cse {
namespace localization {

template<std::size_t Bytes>
bool ReplaceCenterBuffer(char (&buffer)[Bytes], int& lines, int& eraseLines, bool active,
    const char* expected, const char* replacement) {
    if (!active || !expected || !replacement) {
        return false;
    }
    constexpr std::size_t MaximumTextBytes = Bytes - 1;
    std::size_t expectedBytes = 0;
    while (expectedBytes < MaximumTextBytes && expected[expectedBytes]) {
        ++expectedBytes;
    }
    if (std::strlen(buffer) != expectedBytes || std::memcmp(buffer, expected, expectedBytes) != 0) {
        return false;
    }
    std::size_t replacementBytes = 0;
    while (replacementBytes < MaximumTextBytes && replacement[replacementBytes]) {
        ++replacementBytes;
    }
    // Do not split an otherwise valid UTF8 character at the retained byte limit.
    constexpr unsigned char ContinuationMask = 0xc0;
    constexpr unsigned char ContinuationTag = 0x80;
    if (replacementBytes == MaximumTextBytes) {
        while (replacementBytes > 0
            && (static_cast<unsigned char>(replacement[replacementBytes]) & ContinuationMask) == ContinuationTag) {
            --replacementBytes;
        }
    }
    const int previousLines = lines;
    std::memmove(buffer, replacement, replacementBytes);
    buffer[replacementBytes] = '\0';
    lines = 1;
    for (std::size_t index = 0; index < replacementBytes; ++index) {
        if (buffer[index] == '\n') {
            ++lines;
        }
    }
    eraseLines = std::max(eraseLines, std::max(previousLines, lines));
    return true;
}

} // namespace localization
} // namespace cse

#endif
