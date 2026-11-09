#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "actual_types.inc"
#include "selected_progs.h"

namespace {

constexpr std::size_t MappingAlignment = 65536;
constexpr std::size_t NormalMappingBytes = MappingAlignment;
constexpr std::size_t CommandOffset = 4096;
constexpr std::size_t PoseOffset = 8192;
constexpr int CommandWords = 4;
constexpr int OrderedVertices = 3;
constexpr int Poses = 2;
constexpr byte UnwrittenPayloadByte = 0xa5;
constexpr std::uint64_t HighAddressHint = UINT64_C(64) << 30;
constexpr int MAX_QPATH = 64;
constexpr int MAX_OSPATH = 260;

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Reserve one contiguous region; only the explicitly used pages are committed on Windows.
class AddressRegion {
public:
    explicit AddressRegion(std::size_t bytes) : bytes_(bytes) {
#if defined(_WIN32)
#if UINTPTR_MAX > UINT32_MAX
        data_ = static_cast<byte*>(VirtualAlloc(reinterpret_cast<void*>(HighAddressHint), bytes_,
            MEM_RESERVE, PAGE_NOACCESS));
#else
        data_ = static_cast<byte*>(VirtualAlloc(nullptr, bytes_, MEM_RESERVE, PAGE_NOACCESS));
#endif
#else
#if UINTPTR_MAX > UINT32_MAX
        void* hint = reinterpret_cast<void*>(HighAddressHint);
#else
        void* hint = nullptr;
#endif
        void* allocation = mmap(hint, bytes_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (allocation != MAP_FAILED) {
            data_ = static_cast<byte*>(allocation);
        }
#endif
        Check(data_ != nullptr, "Cannot reserve fixture region");
#if UINTPTR_MAX > UINT32_MAX
        Check(reinterpret_cast<std::uintptr_t>(data_) > UINT32_MAX, "Fixture region must be above 4 GiB");
#endif
    }

    ~AddressRegion() {
        if (data_) {
#if defined(_WIN32)
            VirtualFree(data_, 0, MEM_RELEASE);
#else
            munmap(data_, bytes_);
#endif
        }
    }

    byte* At(std::size_t offset, std::size_t bytes) {
        Check(offset <= bytes_ && bytes <= bytes_ - offset, "Fixture mapping bounds");
#if defined(_WIN32)
        const std::size_t begin = offset & ~(MappingAlignment - 1);
        const std::size_t end = (offset + bytes + MappingAlignment - 1) & ~(MappingAlignment - 1);
        Check(VirtualAlloc(data_ + begin, end - begin, MEM_COMMIT, PAGE_READWRITE) != nullptr,
            "Cannot commit fixture pages");
#endif
        return data_ + offset;
    }

    AddressRegion(const AddressRegion&) = delete;
    AddressRegion& operator=(const AddressRegion&) = delete;

private:
    std::size_t bytes_;
    byte* data_ = nullptr;
};

void EdictLinks() {
    static_assert(std::is_standard_layout<edict_t>::value, "offsetof requires the actual standard-layout edict");
    static_assert(std::is_standard_layout<link_t>::value, "Link layout must remain standard");
    static_assert(offsetof(edict_t, free) == 0, "Edict prefix changed");
    static_assert(offsetof(edict_t, serialnumber) == sizeof(int), "Edict serial offset changed");
    static_assert(offsetof(edict_t, area) == 2 * sizeof(int), "Edict area offset changed");
    static_assert(offsetof(edict_t, headnode) == offsetof(edict_t, area) + sizeof(link_t),
        "Edict field after the link changed");
    static_assert(sizeof(link_t) == 2 * sizeof(void*), "Link contains two native pointers");
    static_assert(offsetof(link_t, next) == sizeof(void*), "Link pointer offsets changed");
    AddressRegion region(NormalMappingBytes);
    constexpr int EdictCount = 3;
    auto* edicts = reinterpret_cast<edict_t*>(region.At(0, sizeof(edict_t) * EdictCount));
    for (int index = 0; index < EdictCount; ++index) {
        new (&edicts[index]) edict_t{};
        edicts[index].serialnumber = index + 1;
        edicts[index].area.prev = &edicts[(index + EdictCount - 1) % EdictCount].area;
        edicts[index].area.next = &edicts[(index + 1) % EdictCount].area;
    }
    link_t* cursor = &edicts[0].area;
    for (int index = 0; index < EdictCount; ++index) {
        Check(EDICT_FROM_AREA(cursor) == &edicts[index], "Edict link recovered the wrong object");
        Check(EDICT_FROM_AREA(cursor)->serialnumber == index + 1, "Edict data changed");
        cursor = cursor->next;
    }
    Check(cursor == &edicts[0].area, "Edict link order changed");
    int evaluations = 0;
    const auto NextLink = [&]() {
        ++evaluations;
        return cursor;
    };
    Check(EDICT_FROM_AREA(NextLink()) == &edicts[0] && evaluations == 1,
        "The link macro evaluated its argument more than once");
    std::printf("edict size=%zu alignment=%zu area=%zu link=%zu private=%zu vars=%zu base=%p\n",
        sizeof(edict_t), alignof(edict_t), offsetof(edict_t, area), sizeof(link_t),
        offsetof(edict_t, pvPrivateData), offsetof(edict_t, v), static_cast<void*>(edicts));
}

finalvert_t* pfinalverts = nullptr;
auxvert_t* pauxverts = nullptr;
float* plightvalues = nullptr;
auxvert_t auxverts[MAXSTUDIOVERTS]{};
vec3_t lightvalues[MAXSTUDIOVERTS]{};

#include "actual_studio.inc"

void StudioBuffer() {
    static_assert(sizeof(finalvert_t) == 32, "Actual final vertex ABI changed");
    R_StudioVertBuffer();
    constexpr std::uintptr_t VertexAlignment = 32;
    Check(pfinalverts != nullptr, "Studio buffer was not selected");
#if UINTPTR_MAX > UINT32_MAX
    Check(reinterpret_cast<std::uintptr_t>(pfinalverts) > UINT32_MAX,
        "Studio buffer address lost high bits");
#endif
    Check(reinterpret_cast<std::uintptr_t>(pfinalverts) % VertexAlignment == 0,
        "Studio buffer is not 32-byte aligned");
    Check(pauxverts == auxverts && plightvalues == lightvalues[0], "Studio side buffers changed");
    finalvert_t* original = pfinalverts;
    for (int index = 0; index < MAXSTUDIOVERTS; ++index) {
        pfinalverts[index].v[0] = index;
        pfinalverts[index].flags = MAXSTUDIOVERTS - index;
    }
    R_StudioVertBuffer();
    Check(pfinalverts == original, "Studio buffer lifetime changed");
    for (int index = 0; index < MAXSTUDIOVERTS; ++index) {
        Check(pfinalverts[index].v[0] == index && pfinalverts[index].flags == MAXSTUDIOVERTS - index,
            "Studio vertex span was not retained");
    }
    std::printf("studio base=%p vertices=%d vertex-bytes=%zu\n",
        static_cast<void*>(pfinalverts), MAXSTUDIOVERTS, sizeof(finalvert_t));
}

// External services of the complete GL_MakeAliasModelDisplayLists method are deterministic fixture bindings.
struct model_t {
    char name[MAX_QPATH];
};
using FileHandle_t = void*;
model_t* aliasmodel = nullptr;
aliashdr_t* paliashdr = nullptr;
int commands[8192]{};
int numcommands = 0;
int vertexorder[8192]{};
int numorder = 0;
trivertx_t* poseverts[MAXALIASFRAMES]{};
std::vector<byte> cachedFile;
std::size_t readCursor = 0;
AddressRegion* hunk = nullptr;
std::size_t requestedCommandOffset = CommandOffset;
std::size_t requestedPoseOffset = PoseOffset;
int allocations = 0;

struct EngineError {};

[[noreturn]] void Sys_Error(const char*, ...) {
    throw EngineError{};
}

void Con_Printf(const char*, ...) {
    throw std::runtime_error("The cached fixture unexpectedly rebuilt triangles");
}

void BuildTris() {
    throw std::runtime_error("The cached fixture unexpectedly rebuilt triangles");
}

void Q_strcpy(char* destination, const char* source) {
    std::strcpy(destination, source);
}

std::size_t Q_strlen(const char* text) {
    return std::strlen(text);
}

void COM_StripExtension(const char* source, char* destination) {
    std::strcpy(destination, source);
    if (char* extension = std::strrchr(destination, '.')) {
        *extension = '\0';
    }
}

void Q_memcpy(void* destination, const void* source, std::size_t bytes) {
    std::memcpy(destination, source, bytes);
}

FileHandle_t FS_Open(const char*, const char* mode) {
    Check(std::strcmp(mode, "rb") == 0, "Unexpected file mode");
    readCursor = 0;
    return &cachedFile;
}

void FS_Read(void* destination, std::size_t bytes, std::size_t count, FileHandle_t) {
    const std::size_t total = bytes * count;
    Check(total <= cachedFile.size() - readCursor, "Cached file bounds");
    std::memcpy(destination, cachedFile.data() + readCursor, total);
    readCursor += total;
}

void FS_Write(const void*, std::size_t, std::size_t, FileHandle_t) {
    throw std::runtime_error("The cached fixture unexpectedly wrote a file");
}

void FS_Close(FileHandle_t) {
}

void* Hunk_Alloc(int bytes) {
    Check(allocations < 2 && bytes >= 0, "Unexpected alias hunk allocation");
    const std::size_t offset = allocations++ == 0 ? requestedCommandOffset : requestedPoseOffset;
    byte* storage = hunk->At(offset, static_cast<std::size_t>(bytes));
    std::memset(storage, UnwrittenPayloadByte, static_cast<std::size_t>(bytes));
    return storage;
}

#include "actual_alias.inc"
#include "actual_flatcolor.inc"

template<typename T>
void AppendCached(const T& object) {
    const auto* bytes = reinterpret_cast<const byte*>(&object);
    cachedFile.insert(cachedFile.end(), bytes, bytes + sizeof(object));
}

void AliasFixture(std::size_t commandOffset, std::size_t poseOffset, bool mustReject) {
    static_assert(sizeof(aliashdr_t) == 268, "The pointer-free alias header layout changed");
    static_assert(offsetof(aliashdr_t, posedata) == 92, "Alias pose offset field moved");
    static_assert(offsetof(aliashdr_t, commands) == 96, "Alias command offset field moved");
    static_assert(sizeof(aliashdr_t::commands) == sizeof(std::int32_t), "Commands remain 32-bit relative offsets");
    static_assert(sizeof(aliashdr_t::posedata) == sizeof(std::int32_t), "Poses remain 32-bit relative offsets");
    static_assert(sizeof(trivertx_t) == 4, "Alias vertices remain four byte records");
    const std::size_t bytes = std::max(commandOffset, poseOffset) + MappingAlignment;
    AddressRegion region(bytes);
    hunk = &region;
    requestedCommandOffset = commandOffset;
    requestedPoseOffset = poseOffset;
    allocations = 0;
    auto* header = new (region.At(0, sizeof(aliashdr_t))) aliashdr_t{};
    constexpr int OffsetSentinel = -777;
    header->commands = OffsetSentinel;
    header->posedata = OffsetSentinel;
    header->numposes = Poses;
    constexpr int GoldenCommands[CommandWords] = {3, 0x3f000000, 0x3f800000, 0};
    constexpr int GoldenOrder[OrderedVertices] = {2, 0, 1};
    trivertx_t input[Poses][OrderedVertices] = {
        {{{10, 11, 12}, 13}, {{20, 21, 22}, 23}, {{30, 31, 32}, 33}},
        {{{40, 41, 42}, 43}, {{50, 51, 52}, 53}, {{60, 61, 62}, 63}}
    };
    constexpr byte GoldenPoseBytes[Poses * OrderedVertices * sizeof(trivertx_t)] = {
        30, 31, 32, 33, 10, 11, 12, 13, 20, 21, 22, 23,
        60, 61, 62, 63, 40, 41, 42, 43, 50, 51, 52, 53
    };
    for (int index = 0; index < Poses; ++index) {
        poseverts[index] = input[index];
    }
    cachedFile.clear();
    AppendCached(CommandWords);
    AppendCached(OrderedVertices);
    AppendCached(GoldenCommands);
    AppendCached(GoldenOrder);
    model_t model{};
    std::strcpy(model.name, "progs/fixture.mdl");
    bool rejected = false;
    try {
        GL_MakeAliasModelDisplayLists(&model, header);
    } catch (const EngineError&) {
        rejected = true;
    }
    Check(rejected == mustReject, "Alias offset representability was not enforced");
    if (rejected) {
        const bool commandFailed = commandOffset > static_cast<std::size_t>(INT_MAX);
        Check(commandFailed ? header->commands == OffsetSentinel : header->posedata == OffsetSentinel,
            "An out-of-range alias offset overwrote its 32-bit field before failure");
        const std::size_t rejectedOffset = commandFailed ? commandOffset : poseOffset;
        const std::size_t payloadBytes = commandFailed ? sizeof(GoldenCommands) : sizeof(GoldenPoseBytes);
        const byte* payload = region.At(rejectedOffset, payloadBytes);
        for (std::size_t index = 0; index < payloadBytes; ++index) {
            Check(payload[index] == UnwrittenPayloadByte, "An invalid alias offset copied payload before failure");
        }
        return;
    }
    Check(header->poseverts == OrderedVertices, "Alias pose vertex count changed");
    Check(header->commands == static_cast<int>(commandOffset), "Alias command offset changed");
    Check(header->posedata == static_cast<int>(poseOffset), "Alias pose offset changed");
    Check(std::memcmp(reinterpret_cast<byte*>(header) + header->commands, GoldenCommands,
        sizeof(GoldenCommands)) == 0, "Alias command bytes changed");
    Check(std::memcmp(reinterpret_cast<byte*>(header) + header->posedata, GoldenPoseBytes,
        sizeof(GoldenPoseBytes)) == 0, "Alias vertex ordering or bytes changed");
    if (poseOffset + sizeof(GoldenPoseBytes) <= NormalMappingBytes) {
        // Relocate the complete span as Mod_LoadAliasModel does; consumers rebase both retained offsets.
        const std::size_t usedBytes = poseOffset + sizeof(GoldenPoseBytes);
        std::vector<byte> relocated(usedBytes);
        std::memcpy(relocated.data(), header, usedBytes);
        const auto* copiedHeader = reinterpret_cast<const aliashdr_t*>(relocated.data());
        Check(std::memcmp(relocated.data() + copiedHeader->commands, GoldenCommands,
            sizeof(GoldenCommands)) == 0, "Relocated alias commands changed");
        Check(std::memcmp(relocated.data() + copiedHeader->posedata, GoldenPoseBytes,
            sizeof(GoldenPoseBytes)) == 0, "Relocated alias poses changed");
    }
    std::printf("alias header=%zu commands-field=%zu posedata-field=%zu base=%p offsets=%d/%d\n",
        sizeof(aliashdr_t), offsetof(aliashdr_t, commands), offsetof(aliashdr_t, posedata),
        static_cast<void*>(header), header->commands, header->posedata);
}

void AliasOffsetBounds() {
#if CSE_HAS_ALIAS_OFFSET_HELPER
    constexpr std::ptrdiff_t ValidOffsets[] = {INT_MIN, -1, 0, 1, INT_MAX};
    for (const auto offset : ValidOffsets) {
        Check(GL_AliasDataOffset(offset) == offset, "A representable signed offset changed");
    }
#if PTRDIFF_MAX > INT32_MAX
    constexpr std::ptrdiff_t InvalidOffsets[] = {
        static_cast<std::ptrdiff_t>(INT_MIN) - 1, static_cast<std::ptrdiff_t>(INT_MAX) + 1,
        PTRDIFF_MIN, PTRDIFF_MAX
    };
    for (const auto offset : InvalidOffsets) {
        bool rejected = false;
        try {
            static_cast<void>(GL_AliasDataOffset(offset));
        } catch (const EngineError&) {
            rejected = true;
        }
        Check(rejected, "An unrepresentable signed offset was accepted");
    }
#endif
#else
    std::puts("Frozen reference: no offset guard helper; complete-method overflow scenarios exercise the defect");
#endif
}

void FlatColor() {
    struct GoldenCase {
        std::uint32_t address;
        int index;
    };
    // Fixed Win32 signed low-32 arithmetic shift/remainder results, including the inherited negative index.
    constexpr GoldenCase Cases[] = {
        {0x00000000, 0}, {0x0000000f, 0}, {0x00000010, 1}, {0x00000ff0, 0},
        {0x12345678, 208}, {0x7fffffff, 7}, {0x80000000, -8}, {0x8000000f, -8},
        {0xfffffff0, -1}, {0xffffffff, -1}, {0x89abcdef, -209}
    };
    for (const auto& expected : Cases) {
        const auto lowAddress = static_cast<std::uintptr_t>(expected.address);
        const int actual = D_SurfaceFlatColorIndex(reinterpret_cast<const void*>(lowAddress));
        Check(actual == expected.index, "Flatcolor signed low-32 reference result changed");
        std::printf("flatcolor low32=%08x index=%d\n", static_cast<unsigned int>(expected.address), actual);
#if UINTPTR_MAX > UINT32_MAX
        constexpr std::uintptr_t HighBits = UINT64_C(0x0000001200000000);
        Check(D_SurfaceFlatColorIndex(reinterpret_cast<const void*>(HighBits | lowAddress)) == expected.index,
            "Flatcolor hash incorrectly includes native high address bits");
#endif
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "Expected one scenario");
        const std::string scenario = argv[1];
        if (scenario == "edict-links") {
            EdictLinks();
        } else if (scenario == "studio-buffer") {
            StudioBuffer();
        } else if (scenario == "alias-bytes") {
            AliasFixture(CommandOffset, PoseOffset, false);
        } else if (scenario == "alias-offset-bounds") {
            AliasOffsetBounds();
        } else if (scenario == "alias-command-range" || scenario == "alias-pose-range") {
#if PTRDIFF_MAX > INT32_MAX
            constexpr std::size_t UnrepresentableOffset = static_cast<std::size_t>(INT32_MAX) + 1;
            AliasFixture(scenario == "alias-command-range" ? UnrepresentableOffset : CommandOffset,
                scenario == "alias-pose-range" ? UnrepresentableOffset : PoseOffset, true);
#else
            std::puts("Win32: every valid in-hunk ptrdiff_t already fits the retained int offset");
#endif
        } else if (scenario == "flatcolor") {
            FlatColor();
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
