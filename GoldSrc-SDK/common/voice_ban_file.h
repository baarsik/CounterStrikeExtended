#ifndef CSE_VOICE_BAN_FILE_H
#define CSE_VOICE_BAN_FILE_H

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace cse_voice_bans {

// Legacy version 1 is a fixed little-endian 32-bit prefix followed by raw 16-byte IDs.
constexpr unsigned char VersionOnePrefix[]{1, 0, 0, 0};
constexpr std::size_t PlayerIdBytes = 16;

template<class Append>
void ReadLegacy(FILE* file, Append append) {
    unsigned char version[sizeof(VersionOnePrefix)]{};
    if (!file || std::fread(version, 1, sizeof(version), file) != sizeof(version)
        || std::memcmp(version, VersionOnePrefix, sizeof(version)) != 0
        || std::fseek(file, 0, SEEK_END) != 0) {
        return;
    }
    const long end = std::ftell(file);
    constexpr long HeaderBytes = static_cast<long>(sizeof(VersionOnePrefix));
    if (end < HeaderBytes || std::fseek(file, HeaderBytes, SEEK_SET) != 0) {
        return;
    }
    // CRT positions are native storage, not serialized counts. The legacy reader
    // ignores an incomplete trailing record; never synthesize an ID after a short read.
    const long count = (end - HeaderBytes) / static_cast<long>(PlayerIdBytes);
    for (long index = 0; index < count; ++index) {
        char playerId[PlayerIdBytes]{};
        if (std::fread(playerId, 1, sizeof(playerId), file) != sizeof(playerId)) {
            return;
        }
        append(playerId);
    }
}

inline void WriteVersion(FILE* file) {
    std::fwrite(VersionOnePrefix, 1, sizeof(VersionOnePrefix), file);
}

}
#endif
