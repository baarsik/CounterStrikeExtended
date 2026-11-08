#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include "../../../GoldSrc-SDK/common/legacy_demo_records.h"

typedef unsigned char byte;
typedef float vec3_t[3];
typedef int qboolean;
#include "../../../GoldSrc-SDK/common/ref_params.h"
#include "../../../GoldSrc-SDK/common/usercmd.h"
#include "../../../GoldSrc-SDK/pm_shared/pm_movevars.h"

struct NativeDemoFrame {
    float timestamp;
    ref_params_t rp;
    usercmd_t cmd;
    movevars_t movevars;
    vec3_t view;
    int viewmodel;
};

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Demo check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

int main(int argc, char **argv) {
    using namespace cse::demo;
    // Hand-derived Win32 v5 record: 4 + 232 + 52 + 132 + 12 + 4 bytes.
    constexpr std::size_t frameBytes = 436;
    std::array<unsigned char, frameBytes> original{};
    for (std::size_t index = 0; index < original.size(); ++index) {
        original[index] = static_cast<unsigned char>(index * 37 + 11);
    }
    auto native = std::make_unique<NativeDemoFrame>();
    CHECK(DecodeLegacyDemoFrame(original.data(), original.size(), *native));
    CHECK(native->rp.cmd == &native->cmd && native->rp.movevars == &native->movevars);
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(native.get()) > UINT32_MAX);
    }
    const auto encoded = EncodeLegacyDemoFrame(*native);
    auto normalized = original;
    // Discard saved addresses and deterministic Win32 usercmd alignment bytes only.
    for (std::size_t index = 204; index < 212; ++index) {
        normalized[index] = 0;
    }
    normalized[239] = 0;
    normalized[265] = 0;
    normalized[270] = 0;
    normalized[271] = 0;
    CHECK(std::memcmp(&encoded, normalized.data(), normalized.size()) == 0);
    CHECK(!DecodeLegacyDemoFrame(original.data(), original.size() - 1, *native));
    CHECK(!DecodeLegacyDemoFrame(nullptr, original.size(), *native));
    // Packed HLTV frame storage can begin at any byte; decoding must not read it as an aligned native struct.
    std::array<unsigned char, sizeof(NativeDemoFrame) + 1> packed{};
    std::memcpy(packed.data() + 1, native.get(), sizeof(*native));
    NativeDemoFrame copied{};
    CopyNativeDemoFrame(packed.data() + 1, copied);
    CHECK(copied.rp.cmd == &copied.cmd && copied.rp.movevars == &copied.movevars);
    const auto copiedDisk = EncodeLegacyDemoFrame(copied);
    CHECK(std::memcmp(&copiedDisk, &encoded, sizeof(encoded)) == 0);
    // These header extents are fixed even where the host's MAX_PATH is 1024 or larger.
    LegacyDemoHeader header{};
    std::memcpy(header.szFileStamp, "HLDEMO", 6);
    header.nDemoProtocol = 5;
    header.nNetProtocolVersion = 48;
    header.szMapName[259] = 'm';
    header.szDllDir[259] = 'g';
    header.nDirectoryOffset = 0x12345678;
    const auto *headerBytes = reinterpret_cast<const unsigned char *>(&header);
    CHECK(sizeof(header) == 544 && headerBytes[275] == 'm' && headerBytes[535] == 'g');
    CHECK(headerBytes[540] == 0x78 && headerBytes[543] == 0x12);
    LegacyDemoEntry entry{};
    entry.nEntryType = 1;
    entry.szDescription[63] = 'd';
    entry.nFileLength = 0x12345678;
    const auto *entryBytes = reinterpret_cast<const unsigned char *>(&entry);
    CHECK(sizeof(entry) == 92 && entryBytes[67] == 'd' && entryBytes[88] == 0x78);
    CHECK(argc >= 2 && argc <= 3);
    std::ifstream input(argv[1], std::ios::binary);
    CHECK(input.read(reinterpret_cast<char *>(original.data()), original.size()));
    CHECK(input.peek() == std::char_traits<char>::eof());
    CHECK(DecodeLegacyDemoFrame(original.data(), original.size(), *native));
    CHECK(native->rp.health == 100 && native->cmd.forwardmove == 1.0f);
    const auto captured = EncodeLegacyDemoFrame(*native);
    normalized = original;
    for (std::size_t index = 204; index < 212; ++index) {
        normalized[index] = 0;
    }
    normalized[239] = 0;
    normalized[265] = 0;
    normalized[270] = 0;
    normalized[271] = 0;
    CHECK(std::memcmp(&captured, normalized.data(), normalized.size()) == 0);
    if (argc == 3) {
        std::ofstream output(argv[2], std::ios::binary);
        CHECK(output.write(reinterpret_cast<const char *>(&captured), sizeof(captured)));
    }
    return 0;
}
