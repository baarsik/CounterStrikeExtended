#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include "HLTV/common/DirectorCmd.h"
#include "HLTV/common/InfoString.h"
#include "HLTV/common/md5.h"
#include "../strings/high_offset_buffer.h"

namespace fixture {

constexpr unsigned int AllocationLimit = 65536;
const char* lengthOverride = nullptr;
std::size_t overriddenLength = 0;
bool overrideAllLengths = false;
unsigned int allocationCalls = 0;
unsigned int lastAllocation = 0;
std::vector<unsigned char> hashedBytes;

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::size_t StringLength(const char* text) {
    return overrideAllLengths || text == lengthOverride ? overriddenLength : std::strlen(text);
}

} // namespace fixture

extern "C" void* Mem_ZeroMalloc(unsigned int bytes) {
    ++fixture::allocationCalls;
    fixture::lastAllocation = bytes;
    return bytes <= fixture::AllocationLimit ? std::calloc(bytes, 1) : nullptr;
}

#include "actual_bitbuffer.inc"

// Exact protocol constants from the production header; no socket/provider code is needed.
#include "actual_protocol_constants.inc"
#define strlen ::fixture::StringLength
#include "actual_director.inc"
#include "actual_info.inc"
#undef strlen

#include "actual_md5_context.inc"

// Record the bytes passed to the unchanged hashing algorithm. Cryptographic computation is outside this count fixture.
static void MD5_Init(MD5Context_t*) {
    fixture::hashedBytes.clear();
}

static void MD5_Update(MD5Context_t*, const unsigned char* bytes, unsigned int size) {
    fixture::hashedBytes.assign(bytes, bytes + size);
}

static void MD5_Final(unsigned char* digest, MD5Context_t*) {
    for (unsigned int index = 0; index < 16; ++index) {
        digest[index] = static_cast<unsigned char>(index);
    }
}

extern "C" char* MD5_Print(unsigned char*) {
    static char text[] = "000102030405060708090a0b0c0d0e0f";
    return text;
}

#define strlen ::fixture::StringLength
#include "actual_md5_key.inc"
#undef strlen

#include "actual_bsp_types.inc"

struct RejectedHull {};

struct HullSystemBinding {
    void Errorf(const char*, ...) {
        throw RejectedHull{};
    }
};

// Actual MakeHull0 body and actual plane/node/clip/hull records; owned class context, not a BSPModel ABI model.
class BSPModel {
public:
    void MakeHull0();
    struct ModelFields {
        int numnodes = 0;
        mnode_t* nodes = nullptr;
        mplane_t* planes = nullptr;
        int numplanes = 0;
        hull_t hulls[4]{};
        char name[64]{};
    } model;
    HullSystemBinding* m_System = nullptr;
};

#include "actual_hull.inc"

namespace {

void CheckBytes(const void* actual, const std::vector<unsigned char>& expected, const char* message);

class BitFixture : public BitBuffer {
public:
    using BitBuffer::BitBuffer;

    void SetLittleEndian(bool value) {
        littleEndian = value;
    }
};

std::vector<unsigned char> PackBits(std::vector<unsigned char> bytes, unsigned int value, int count,
        int startBit, bool little) {
    constexpr int BitsPerByte = 8;
    if (!little && count <= 31 && value >= (1u << count) && value != UINT_MAX) {
        value = (1u << count) - 1;
    }
    for (int index = 0; index < count; ++index) {
        const int position = startBit + index;
        const int sourceBit = little ? index : count - index - 1;
        const int bit = little ? position % BitsPerByte : BitsPerByte - 1 - position % BitsPerByte;
        const auto mask = static_cast<unsigned char>(1u << bit);
        auto& byte = bytes[static_cast<std::size_t>(position / BitsPerByte)];
        if ((value & (1u << sourceBit)) != 0) {
            byte |= mask;
        } else if (!little) {
            byte &= static_cast<unsigned char>(~mask);
        }
    }
    return bytes;
}

unsigned int UnpackBits(const std::vector<unsigned char>& bytes, int count, int startBit, bool little) {
    constexpr int BitsPerByte = 8;
    unsigned int value = 0;
    for (int index = 0; index < count; ++index) {
        const int position = startBit + index;
        const int bit = little ? position % BitsPerByte : BitsPerByte - 1 - position % BitsPerByte;
        const unsigned int input = (bytes[static_cast<std::size_t>(position / BitsPerByte)] >> bit) & 1u;
        value |= input << (little ? index : count - index - 1);
    }
    return value;
}

void BitWords() {
    constexpr std::size_t GuardBytes = 16;
    constexpr std::size_t LogicalBytes = 9;
    constexpr std::size_t PaddingBytes = sizeof(std::uint32_t);
    constexpr unsigned char Guard = 0xa5;
    constexpr unsigned char Padding = 0x5a;
    constexpr unsigned int Value = 0x89abcdef;
    std::vector<unsigned char> corpus;
    for (const bool little : {true, false}) {
        for (int bits = 0; bits <= 32; ++bits) {
            for (int startBit = 0; startBit < 8; ++startBit) {
                alignas(std::uint32_t) std::array<unsigned char,
                    GuardBytes + 1 + LogicalBytes + PaddingBytes + GuardBytes> storage;
                storage.fill(Guard);
                unsigned char* data = storage.data() + GuardBytes + 1;
                std::memset(data, 0x24, LogicalBytes);
                std::memset(data + LogicalBytes, Padding, PaddingBytes);
                const int needed = (bits + startBit + 7) / 8;
                const int offset = static_cast<int>(LogicalBytes) - (needed != 0 ? needed : 1);
                std::vector<unsigned char> initial(data, data + LogicalBytes);
                const auto expected = PackBits(initial, Value, bits, offset * 8 + startBit, little);
                BitFixture buffer(data, static_cast<unsigned int>(LogicalBytes));
                buffer.SetLittleEndian(little);
                buffer.currentByte += offset;
                buffer.currentBit = startBit;
                buffer.WriteBits(Value, bits);
                CheckBytes(data, expected, "Bit packing changed at an unaligned/near-end address");
                fixture::Check(!buffer.sizeError && buffer.CurrentBit() == offset * 8 + startBit + bits,
                    "Bit-write cursor or error behavior changed");
                buffer.currentByte = data + offset;
                buffer.currentBit = startBit;
                const auto readExpected = UnpackBits(expected, bits, offset * 8 + startBit, little);
                fixture::Check(buffer.PeekBits(bits) == readExpected && buffer.currentByte == data + offset
                    && buffer.currentBit == startBit, "Peek bits changed data/cursor");
                fixture::Check(buffer.ReadBits(bits) == readExpected
                    && buffer.CurrentBit() == offset * 8 + startBit + bits, "Read bits changed data/cursor");
                for (std::size_t index = 0; index < storage.size(); ++index) {
                    const std::size_t begin = GuardBytes + 1;
                    if (index < begin || index >= begin + LogicalBytes + PaddingBytes) {
                        fixture::Check(storage[index] == Guard, "Bit access crossed an allocated-extent canary");
                    } else if (index >= begin + LogicalBytes) {
                        fixture::Check(storage[index] == Padding, "Valid bit operations changed padding bytes");
                    }
                }
                corpus.insert(corpus.end(), storage.begin(), storage.end());
                corpus.push_back(static_cast<unsigned char>(buffer.CurrentBit()));
                corpus.push_back(static_cast<unsigned char>(buffer.sizeError));
            }
        }
    }
    if (const char* path = std::getenv("CSE_HLTV_BIT_CORPUS")) {
        FILE* file = std::fopen(path, "wb");
        fixture::Check(file != nullptr, "Cannot create bit corpus receipt");
        const auto written = std::fwrite(corpus.data(), 1, corpus.size(), file);
        std::fclose(file);
        fixture::Check(written == corpus.size(), "Cannot write bit corpus receipt");
    }
    std::puts("528 endian/width/bit-offset combinations retained payload, cursor, padding and guards");
}

void BitBulk() {
    const std::vector<unsigned char> payload = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    constexpr unsigned char Guard = 0xa5;
    constexpr int StartBit = 3;
    constexpr unsigned int StreamBytes = 10;
    std::vector<unsigned char> input(payload.size() + 2, Guard);
    std::memcpy(input.data() + 1, payload.data(), payload.size());
    std::vector<unsigned char> storage(StreamBytes + sizeof(std::uint32_t) + 2, Guard);
    unsigned char* data = storage.data() + 1;
    std::memset(data, 0, StreamBytes + sizeof(std::uint32_t));
    BitBuffer stream(data, StreamBytes);
    stream.currentBit = StartBit;
    stream.WriteBuf(input.data() + 1, static_cast<int>(payload.size()));
    std::vector<unsigned char> expected(StreamBytes, 0);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        expected = PackBits(expected, payload[index], 8, StartBit + static_cast<int>(index) * 8, true);
    }
    CheckBytes(data, expected, "Unaligned bulk input changed bit packing");
    std::vector<unsigned char> output(payload.size() + 2, Guard);
    stream.Reset();
    stream.currentBit = StartBit;
    fixture::Check(stream.ReadBuf(static_cast<int>(payload.size()), output.data() + 1), "Bulk read failed");
    CheckBytes(output.data() + 1, payload, "Unaligned bulk output changed bytes");
    fixture::Check(input.front() == Guard && input.back() == Guard && output.front() == Guard
        && output.back() == Guard && storage.front() == Guard && storage.back() == Guard,
        "Bulk word copy crossed a canary");
}

void ExternalExtentProbe() {
    std::vector<unsigned char> external(1, 0x5a);
    BitBuffer view(external.data(), 1);
    fixture::Check(view.ReadBits(8) == 0x5a, "One-byte external stream read changed");
    view.Reset();
    external[0] = 0;
    view.WriteBits(0xa5, 8);
    fixture::Check(external[0] == 0xa5, "One-byte external stream write changed");
    view.currentByte = external.data() + 1;
    view.currentBit = 0;
    view.WriteBits(0, 0);
    fixture::Check(!view.sizeError && view.currentByte == external.data() + 1,
        "Zero-remaining no-op write changed cursor/error semantics");
}

void NativeSizeBounds() {
#if CSE_HAS_DIRECTOR_SIZE_GUARD
    for (const int overhead : {1, 5, 30}) {
        const auto maximumText = static_cast<std::size_t>(INT_MAX - overhead);
        fixture::Check(DirectorPayloadSize(maximumText, overhead) == INT_MAX,
            "Maximum representable director payload changed");
        fixture::Check(DirectorPayloadSize(maximumText + 1, overhead) < 0
            && DirectorPayloadSize(SIZE_MAX, overhead) < 0, "Director size arithmetic wrapped at its fence");
    }
#endif
#if CSE_HAS_INFO_SIZE_GUARD
    fixture::Check(InfoStringCapacity(static_cast<std::size_t>(INT_MAX) - 1, 0) == static_cast<unsigned int>(INT_MAX),
        "Maximum representable info capacity changed");
    fixture::Check(InfoStringCapacity(INT_MAX, 0) == 0 && InfoStringCapacity(SIZE_MAX, 0) == 0
        && InfoStringCapacity(0, UINT_MAX) == 0, "Unrepresentable info count accepted");
#endif
}

void CheckBytes(const void* actual, const std::vector<unsigned char>& expected, const char* message) {
    fixture::Check(std::memcmp(actual, expected.data(), expected.size()) == 0, message);
}

std::vector<unsigned char> Serialized(DirectorCmd& command) {
    BitBuffer stream(1024);
    command.WriteToStream(&stream);
    return std::vector<unsigned char>(stream.data, stream.data + stream.CurrentSize());
}

void DirectorBytes() {
    struct Case {
        int type;
        std::vector<unsigned char> payload;
    };
    const std::vector<Case> expected = {
        {DRC_CMD_SOUND, {'s', 0, 0, 0, 0, 0x3f}},
        {DRC_CMD_BANNER, {'b', 'g', 0}},
        {DRC_CMD_STUFFTEXT, {'e', 'c', 'h', 'o', 0}},
        {DRC_CMD_MESSAGE, {2, 0x44, 0x33, 0x22, 0x11, 0, 0, 0, 0x3f, 0, 0, 0x80, 0x3e,
            0, 0, 0x80, 0x3f, 0, 0, 0, 0x40, 0, 0, 0x40, 0x40, 0, 0, 0x80, 0x40, 'h', 'i', 0}}
    };
    for (const auto& golden : expected) {
        DirectorCmd command;
        char sound[] = "s";
        char banner[] = "bg";
        char stuff[] = "echo";
        char message[] = "hi";
        float position[] = {0.5f, 0.25f};
        if (golden.type == DRC_CMD_SOUND) {
            command.SetSoundData(sound, 0.5f);
        } else if (golden.type == DRC_CMD_BANNER) {
            command.SetBannerData(banner);
        } else if (golden.type == DRC_CMD_STUFFTEXT) {
            command.SetStuffTextData(stuff);
        } else {
            command.SetMessageData(2, 0x11223344, position, 1.0f, 2.0f, 3.0f, 4.0f, message);
        }
        fixture::Check(command.m_Size == static_cast<int>(golden.payload.size()) && !command.m_Data.sizeError,
            "Director payload count changed");
        CheckBytes(command.m_Data.data, golden.payload, "Director payload bytes changed");
        std::vector<unsigned char> wire = {51, static_cast<unsigned char>(golden.payload.size() + 1),
            static_cast<unsigned char>(golden.type)};
        wire.insert(wire.end(), golden.payload.begin(), golden.payload.end());
        fixture::Check(Serialized(command) == wire, "Director wire bytes changed");
        BitBuffer incoming(wire.data() + 2, static_cast<unsigned int>(wire.size() - 2));
        DirectorCmd decoded;
        fixture::Check(decoded.ReadFromStream(&incoming), "Director payload did not decode");
        fixture::Check(decoded.m_Size == command.m_Size && decoded.m_Type == command.m_Type,
            "Director decoded count/type changed");
        CheckBytes(decoded.m_Data.data, golden.payload, "Director decoded bytes changed");
    }
}

void DirectorLimits() {
    constexpr int MaximumWirePayload = 254;
    for (const int size : {MaximumWirePayload, MaximumWirePayload + 1}) {
        std::string text(static_cast<std::size_t>(size - 1), 'x');
        DirectorCmd command;
        command.SetBannerData(&text[0]);
        fixture::Check(command.m_Size == size, "Internal payload storage width changed");
        const auto wire = Serialized(command);
        fixture::Check(size == MaximumWirePayload ? wire.size() == static_cast<std::size_t>(size + 3) : wire.empty(),
            "Existing 254-byte wire gate changed");
    }
}

void DirectorOverflow(bool reading) {
#if SIZE_MAX > UINT32_MAX
    char text[] = "abc";
    fixture::overriddenLength = static_cast<std::size_t>(UINT32_MAX) + 5;
    for (const int type : {DRC_CMD_SOUND, DRC_CMD_MESSAGE, DRC_CMD_BANNER, DRC_CMD_STUFFTEXT}) {
        DirectorCmd command;
        std::vector<unsigned char> wire{static_cast<unsigned char>(type)};
        if (type == DRC_CMD_MESSAGE) {
            wire.resize(30, 0);
        }
        wire.insert(wire.end(), {'a', 'b', 'c', 0});
        if (type == DRC_CMD_SOUND) {
            wire.insert(wire.end(), {0, 0, 0, 0x3f});
        }
        const unsigned int allocations = fixture::allocationCalls;
        bool accepted = false;
        if (reading) {
            BitBuffer stream(wire.data(), static_cast<unsigned int>(wire.size()));
            fixture::overrideAllLengths = true;
            accepted = command.ReadFromStream(&stream);
        } else {
            fixture::lengthOverride = text;
            if (type == DRC_CMD_SOUND) {
                command.SetSoundData(text, 0.5f);
            } else if (type == DRC_CMD_MESSAGE) {
                float position[] = {0, 0};
                command.SetMessageData(0, 0, position, 0, 0, 0, 0, text);
            } else if (type == DRC_CMD_BANNER) {
                command.SetBannerData(text);
            } else {
                command.SetStuffTextData(text);
            }
        }
        fixture::lengthOverride = nullptr;
        fixture::overrideAllLengths = false;
        fixture::Check(!accepted && command.m_Type == DRC_CMD_NONE && command.m_Size == 0
            && command.m_Data.data == nullptr && command.m_Data.sizeError, "Director native count wrapped/was accepted");
        fixture::Check(fixture::allocationCalls == allocations, "Rejected director length reached allocation");
    }
#else
    static_cast<void>(reading);
    std::puts("Win32: 4-GiB-plus injected native lengths are not representable");
#endif
}

void InfoBytes() {
    char initial[] = "\\name\\HLTV";
    InfoString info(initial);
    fixture::Check(info.GetMaxSize() == 256 && info.GetCurrentSize() == 10, "Default info sizing changed");
    fixture::Check(std::strcmp(info.GetString(), initial) == 0, "Info constructor bytes changed");
    fixture::Check(info.SetValueForKey("rate", "10000"), "Info append failed");
    fixture::Check(std::strcmp(info.GetString(), "\\name\\HLTV\\rate\\10000") == 0, "Info append bytes changed");
    InfoString minimum(initial, 512);
    fixture::Check(minimum.GetMaxSize() == 512, "Caller info capacity minimum changed");
    minimum.SetMaxSize(64);
    fixture::Check(minimum.GetMaxSize() == 64 && std::strcmp(minimum.GetString(), initial) == 0,
        "Info resize content/ownership changed");
    InfoString exact(5);
    fixture::Check(exact.SetValueForKey("a", "b") && std::strcmp(exact.GetString(), "\\a\\b") == 0,
        "Exact-fit info token/terminator changed");
    InfoString tooSmall(4);
    fixture::Check(!tooSmall.SetValueForKey("a", "b") && tooSmall.GetString()[0] == '\0',
        "Info token exceeded its fixed capacity");
}

void InfoLimits() {
    char text[] = "abc";
#if SIZE_MAX > UINT32_MAX
    fixture::lengthOverride = text;
    fixture::overriddenLength = static_cast<std::size_t>(UINT32_MAX) + 5;
    const unsigned int allocations = fixture::allocationCalls;
    InfoString invalid(text);
    InfoString invalidMinimum(text, 512);
    fixture::lengthOverride = nullptr;
    fixture::Check(!invalid.GetString() && !invalidMinimum.GetString() && invalid.GetMaxSize() == 0,
        "Info native constructor length wrapped");
    fixture::Check(fixture::allocationCalls == allocations, "Invalid info length reached allocation");
#endif
    InfoString valid(text, 64);
    const unsigned int before = fixture::allocationCalls;
    valid.SetMaxSize(UINT_MAX);
    fixture::Check(fixture::allocationCalls == before && valid.GetMaxSize() == 64
        && std::strcmp(valid.GetString(), text) == 0, "Unrepresentable info capacity was not rejected before allocation");
}

void Md5Bytes() {
    for (const std::size_t length : {std::size_t{0}, std::size_t{1}, std::size_t{255}, std::size_t{256}}) {
        const std::string key(length, 'k');
        fixture::hashedBytes.clear();
        char* result = MD5_GetCDKeyHash(key.c_str());
        const bool valid = length != 0 && length <= 255;
        fixture::Check((result != nullptr) == valid, "CD-key length policy changed");
        if (valid) {
            fixture::Check(fixture::hashedBytes == std::vector<unsigned char>(key.begin(), key.end()),
                "CD-key byte count/input changed before hashing");
            fixture::Check(std::strcmp(result, "000102030405060708090a0b0c0d0e0f") == 0,
                "Returned static digest text changed");
        } else {
            fixture::Check(fixture::hashedBytes.empty(), "Rejected key reached hashing");
        }
    }
}

void Md5Overflow() {
#if SIZE_MAX > UINT32_MAX
    const char text[] = "abc";
    fixture::lengthOverride = text;
    fixture::overriddenLength = static_cast<std::size_t>(UINT32_MAX) + 4;
    fixture::hashedBytes.clear();
    char* result = MD5_GetCDKeyHash(text);
    fixture::lengthOverride = nullptr;
    fixture::Check(result == nullptr && fixture::hashedBytes.empty(), "Native key length narrowed before its fixed bound");
#endif
}

void Hull(bool invalid) {
    static_assert(sizeof(dclipnode_t) == 8 && offsetof(dclipnode_t, children) == 4, "Clip record width changed");
    static_assert(sizeof(mplane_t) == 20, "Actual in-memory plane stride changed");
    HighOffsetBuffer region(nullptr);
    fixture::Check(region.get() != nullptr, "Cannot allocate plane region");
    auto* planes = reinterpret_cast<mplane_t*>(region.get());
    for (int index = 0; index < 3; ++index) {
        new (&planes[index]) mplane_t{};
    }
    mnode_t nodes[2]{};
    mnode_t leaves[2]{};
    leaves[0].contents = -1;
    leaves[1].contents = -2;
    nodes[0].plane = &planes[2];
    nodes[1].plane = &planes[0];
    nodes[0].children[0] = &leaves[0];
    nodes[0].children[1] = &nodes[1];
    nodes[1].children[0] = &leaves[1];
    nodes[1].children[1] = &leaves[0];
    if (invalid) {
        nodes[0].plane = &planes[3];
    }
    BSPModel bsp;
    HullSystemBinding system;
    bsp.m_System = &system;
    bsp.model.numnodes = 2;
    bsp.model.nodes = nodes;
    bsp.model.numplanes = 3;
    bsp.model.planes = planes;
    bool rejected = false;
    try {
        bsp.MakeHull0();
    } catch (const RejectedHull&) {
        rejected = true;
    }
    if (invalid) {
        if (!rejected) {
            std::free(bsp.model.hulls[0].clipnodes);
        }
        fixture::Check(rejected && bsp.model.hulls[0].clipnodes == nullptr,
            "Hull accepted an invalid plane or retained a rejected allocation");
    } else {
        fixture::Check(!rejected, "Valid hull rejected");
        const std::vector<unsigned char> golden = {2, 0, 0, 0, 0xff, 0xff, 1, 0, 0, 0, 0, 0, 0xfe, 0xff, 0xff, 0xff};
        CheckBytes(bsp.model.hulls[0].clipnodes, golden, "Hull planenum/child record bytes changed");
        fixture::Check(bsp.model.hulls[0].planes == planes && bsp.model.hulls[0].lastclipnode == 1,
            "Hull pointer/extent changed");
        std::free(bsp.model.hulls[0].clipnodes);
    }
}

void HullBounds() {
#if CSE_HAS_HULL_INDEX_GUARD
    mplane_t planes[3]{};
    fixture::Check(BSPPlaneIndex(&planes[2], planes, 3) == 2, "Valid final plane index rejected");
    fixture::Check(BSPPlaneIndex(&planes[3], planes, 3) < 0, "One-past plane accepted");
    fixture::Check(BSPPlaneIndex(reinterpret_cast<mplane_t*>(reinterpret_cast<std::uintptr_t>(planes) + 1),
        planes, 3) < 0, "Misaligned plane accepted");
    fixture::Check(BSPPlaneIndex(nullptr, planes, 3) < 0 && BSPPlaneIndex(planes, nullptr, 3) < 0,
        "Null plane input accepted");
    fixture::Check(BSPPlaneIndex(planes, planes, 0) < 0 && BSPPlaneIndex(planes, planes, -1) < 0,
        "Invalid plane count accepted");
    fixture::Check(BSPPlaneIndex(reinterpret_cast<mplane_t*>(reinterpret_cast<std::uintptr_t>(planes) - 1),
        planes, 3) < 0, "Below-base plane accepted");
#if UINTPTR_MAX > UINT32_MAX
    constexpr std::uintptr_t BaseAddress = UINT64_C(0x0000001200000000);
    auto* base = reinterpret_cast<mplane_t*>(BaseAddress);
    auto* last = reinterpret_cast<mplane_t*>(BaseAddress + static_cast<std::uintptr_t>(INT_MAX - 1) * sizeof(mplane_t));
    auto* outside = reinterpret_cast<mplane_t*>(BaseAddress + static_cast<std::uintptr_t>(INT_MAX) * sizeof(mplane_t));
    fixture::Check(BSPPlaneIndex(last, base, INT_MAX) == INT_MAX - 1, "Valid int-bounded native index changed");
    fixture::Check(BSPPlaneIndex(outside, base, INT_MAX) < 0, "Out-of-range native index narrowed");
#endif
#else
    std::puts("Archived reference has no native index guard; complete-method invalid-plane case detects it");
#endif
}

} // namespace

int main(int argc, char** argv) {
    try {
        fixture::Check(argc == 2, "Expected one scenario");
        const std::string scenario = argv[1];
        if (scenario == "bit-words") {
            BitWords();
        } else if (scenario == "bit-bulk") {
            BitBulk();
        } else if (scenario == "bit-external-extent") {
            ExternalExtentProbe();
        } else if (scenario == "native-size-bounds") {
            NativeSizeBounds();
        } else if (scenario == "director-bytes") {
            DirectorBytes();
        } else if (scenario == "director-limits") {
            DirectorLimits();
        } else if (scenario == "director-overflow" || scenario == "director-read-overflow") {
            DirectorOverflow(scenario == "director-read-overflow");
        } else if (scenario == "info-bytes") {
            InfoBytes();
        } else if (scenario == "info-limits") {
            InfoLimits();
        } else if (scenario == "md5-bytes") {
            Md5Bytes();
        } else if (scenario == "md5-overflow") {
            Md5Overflow();
        } else if (scenario == "hull-bytes" || scenario == "hull-invalid") {
            Hull(scenario == "hull-invalid");
        } else if (scenario == "hull-bounds") {
            HullBounds();
        } else {
            throw std::runtime_error("Unknown scenario");
        }
        std::printf("PASS %s\n", scenario.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
