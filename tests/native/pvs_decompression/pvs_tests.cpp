#include "cmodel.h"
#include "common.h"
#include "mem.h"
#include "conprint.h"
#include "sys.h"
#include "model_visibility.h"
#include "zone.h"
#if !defined(SW_RENDERER)
#include "glquake.h"
#endif
#include <limits>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static int IdentityLong(int value) {
    return value;
}

static short IdentityShort(short value) {
    return value;
}

int (*LittleLong)(int) = IdentityLong;
short (*LittleShort)(short) = IdentityShort;

float Length(const vec_t* value) {
    return std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
}

void Q_memcpy(void* output, const void* input, int count) {
    std::memcpy(output, input, count);
}

void Q_memset(void* output, int value, int count) {
    std::memset(output, value, count);
}

void Con_DPrintf(const char*, ...) {}

static int liveCacheAllocations;
static int failAllocationAfter = -1;
static std::vector<void*> hunkAllocations;

void* Mem_Calloc(int count, std::size_t size) {
    if (failAllocationAfter == 0) {
        return nullptr;
    }
    if (failAllocationAfter > 0) {
        --failAllocationAfter;
    }
    ++liveCacheAllocations;
    return std::calloc(count, size);
}

void Mem_Free(void* block) {
    --liveCacheAllocations;
    std::free(block);
}

void* Hunk_AllocName(int bytes, const char*) {
    void* allocation = std::calloc(1, bytes);
    hunkAllocations.push_back(allocation);
    return allocation;
}

void Q_strncpy(char* output, const char* input, int count) {
    std::strncpy(output, input, count);
}

int Q_strcmp(const char* first, const char* second) {
    return std::strcmp(first, second);
}

int Q_strcasecmp(const char* first, const char* second) {
    return strcasecmp(first, second);
}

void* Cache_Check(cache_user_t* user) {
    return user->data;
}

void Cache_Free(cache_user_t* user) {
    user->data = nullptr;
}

#if !defined(SW_RENDERER)
void APIENTRY CseRenderDeleteLists(GLuint, GLsizei) {}
#endif

#if defined(CSE_ENABLE_METAL) && !defined(SW_RENDERER)
namespace cse::render {
void ClearBackendSurfaceMeshes() {}
}
#endif

void Sys_Error(const char*, ...) {
    throw std::runtime_error("engine error");
}

#if defined(PVS_BASELINE)
#if defined(SW_RENDERER)
void SW_Mod_Init() {}
#endif
#else
#include "owner.inc"
#endif
#include "visibility.inc"

static void Decode(const byte* input, std::size_t bytes, byte* output, std::size_t count) {
#if defined(PVS_BASELINE)
    (void)bytes;
    CM_DecompressPVS(const_cast<byte*>(input), output, static_cast<int>(count));
#else
    assert(CM_DecompressPVS(input, bytes, output, count));
#endif
}

#if !defined(PVS_BASELINE)
template<class Operation> static void ExpectError(Operation operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

static model_t* LoadFixture() {
    CM_FreePAS();
    std::memset(mod_known, 0, sizeof(mod_known));
    mod_numknown = 0;
    model_t* model = Mod_FindName(FALSE, "fixture");
    loadmodel = model;
    static msurface_t* marksurface;
    model->marksurfaces = &marksurface;
    static std::vector<byte> disk(sizeof(dleaf_t) * 5 + 4);
    std::fill(disk.begin(), disk.end(), 0);
    const byte visibility[] = {2, 4, 1, 8};
    std::memcpy(disk.data(), visibility, sizeof(visibility));
    for (int i = 0; i < 5; ++i) {
        dleaf_t leaf = {};
        leaf.visofs = i == 0 ? -1 : i - 1;
        std::memcpy(disk.data() + sizeof(visibility) + i * sizeof(leaf), &leaf, sizeof(leaf));
    }
    mod_base = disk.data();
    lump_t vis = {0, sizeof(visibility)};
    lump_t leafs = {sizeof(visibility), sizeof(dleaf_t) * 5};
    Mod_LoadVisibility(&vis);
    Mod_LoadLeafs(&leafs);
    model->numleafs = 3;
    return model;
}

#include "corpus_tests.h"

static void RunConsumers() {
    model_t* model = LoadFixture();
    CM_CalcPAS(model);
    assert(liveCacheAllocations == 2);
    assert(CM_LeafPVS(1)[0] == 2 && CM_LeafPAS(1)[0] == 6);
    assert(CM_LeafPVS(2)[0] == 4 && CM_LeafPAS(2)[0] == 5);
    assert(CM_LeafPVS(3)[0] == 1 && CM_LeafPAS(3)[0] == 3);
    for (int row = 0; row < 4; ++row) {
        for (int padding = 1; padding < 4; ++padding) {
            assert(CM_LeafPVS(row)[padding] == 0 && CM_LeafPAS(row)[padding] == 0);
        }
    }
    assert(Mod_LeafPVS(&model->leafs[4], model)[0] == 8);
    ExpectError([] {
        CM_LeafPVS(-1);
    });
    ExpectError([] {
        CM_LeafPAS(4);
    });
    ExpectError([&] {
        Mod_LeafPVS(model->leafs + 5, model);
    });
    ExpectError([&] {
        Mod_DecompressVis(model->visdata + 4, model);
    });
    ExpectError([&] {
        Mod_DecompressVis(reinterpret_cast<byte*>(reinterpret_cast<std::uintptr_t>(model->visdata) - 1), model);
    });

    // A second model must decode its own row rather than reuse another owner's cache.
    model_t* second = &mod_known[mod_numknown++];
    *second = *model;
    mod_known_info[second - mod_known].visibility = Mod_GetVisibilityInfo(model);
    second->leafs = static_cast<mleaf_t*>(Hunk_AllocName(sizeof(mleaf_t) * 5, "test"));
    std::memcpy(second->leafs, model->leafs, sizeof(mleaf_t) * 5);
    second->leafs[1].compressed_vis = model->visdata + 3;
    assert(Mod_LeafPVS(&second->leafs[1], second)[0] == 8);

    // Real submodel setup copies both private extents while retaining the logical count.
    static dmodel_t submodels[2];
    submodels[0].visleafs = 3;
    submodels[1].visleafs = 1;
    model->submodels = submodels;
    model->numsubmodels = 2;
    LoadInlineModels(model);
    model_t* inlineModel = Mod_FindName(FALSE, "*1");
    assert(Mod_GetVisibilityInfo(inlineModel).visBytes == 4);
    assert(Mod_GetVisibilityInfo(inlineModel).leafCount == 5);
    assert(inlineModel->numleafs == 1 && Mod_LeafPVS(&inlineModel->leafs[1], inlineModel)[0] == 2);
    assert(liveCacheAllocations == 2);
    CM_CalcPAS(model);
    Mod_ClearAll();
    assert(liveCacheAllocations == 0);
    ExpectError([&] {
        Mod_LeafPVS(model->leafs + 1, model);
    });
    assert(Mod_GetVisibilityInfo(inlineModel).leafCount == 0);

    model = LoadFixture();
    CM_CalcPAS(model);
    Mod_ChangeGame();
    assert(liveCacheAllocations == 0);
    ExpectError([&] {
        CM_CalcPAS(model);
    });
    model = LoadFixture();
    for (int invalid : {-1, MAX_MAP_LEAFS, INT_MAX}) {
        model->numleafs = invalid;
        ExpectError([&] {
            CM_CalcPAS(model);
        });
        assert(liveCacheAllocations == 0);
    }
    model->numleafs = 5;
    ExpectError([&] {
        CM_CalcPAS(model);
    });
    model->numleafs = 3;
    for (int failAfter : {0, 1}) {
        failAllocationAfter = failAfter;
        ExpectError([&] {
            CM_CalcPAS(model);
        });
        assert(liveCacheAllocations == 0);
    }
    failAllocationAfter = -1;
    // Reject a malformed row after earlier rows have filled the new cache.
    model->visdata[3] = 0;
    model->leafs[2].compressed_vis = model->visdata + 3;
    ExpectError([&] {
        CM_CalcPAS(model);
    });
    assert(liveCacheAllocations == 0 && CM_LeafPVS(1)[0] == 255);
    ExpectError([&] {
        Mod_DecompressVis(model->visdata + 3, model);
    });

    model = LoadFixture();
    std::vector<byte> badLeaf(sizeof(dleaf_t) + 1);
    dleaf_t diskLeaf = {};
    lump_t lump = {0, sizeof(dleaf_t)};
    mod_base = badLeaf.data() + 1;
    loadmodel = model;
    for (int invalid : {-2, 4, INT_MAX}) {
        diskLeaf.visofs = invalid;
        std::memcpy(badLeaf.data() + 1, &diskLeaf, sizeof(diskLeaf));
        ExpectError([&] {
            Mod_LoadLeafs(&lump);
        });
    }
    diskLeaf.visofs = -1;
    std::memcpy(badLeaf.data() + 1, &diskLeaf, sizeof(diskLeaf));
    Mod_LoadLeafs(&lump);
    assert(model->leafs[0].compressed_vis == nullptr);

    model = LoadFixture();
    CM_CalcPAS(model);
    model->needload = NL_NEEDS_LOADED | NL_UNREFERENCED;
    Mod_ClearAll();
    assert(Mod_GetVisibilityInfo(model).leafCount == 5);
    assert(Mod_LeafPVS(model->leafs + 1, model)[0] == 2);
    // Reusing a cached owner slot must drop old extents before a replacement is loaded.
    model->needload = NL_UNREFERENCED;
    for (int index = 1; index < MAX_KNOWN_MODELS; ++index) {
        mod_known[index].needload = NL_PRESENT;
    }
    mod_numknown = MAX_KNOWN_MODELS;
    CM_CalcPAS(model);
    assert(Mod_FindName(FALSE, "replacement") == model);
    assert(liveCacheAllocations == 0 && Mod_GetVisibilityInfo(model).leafCount == 0);
    ExpectError([&] {
        CM_CalcPAS(model);
    });
    model = LoadFixture();
    // A logical row at the limit may coexist with more physical leaves than the legacy limit.
    constexpr int PhysicalCount = MAX_MAP_LEAFS + 1;
    model->leafs = static_cast<mleaf_t*>(Hunk_AllocName(sizeof(mleaf_t) * PhysicalCount, "large"));
    mod_known_info[0].visibility.leafCount = PhysicalCount;
    model->numleafs = MAX_MAP_LEAFS;
    const byte zeroRow[] = {0, 255, 0, 255, 0, 255, 0, 255, 0, 4};
    model->visdata = static_cast<byte*>(Hunk_AllocName(sizeof(zeroRow), "zero"));
    std::memcpy(model->visdata, zeroRow, sizeof(zeroRow));
    mod_known_info[0].visibility.visBytes = sizeof(zeroRow);
    for (int index = 0; index < PhysicalCount; ++index) {
        model->leafs[index].compressed_vis = model->visdata;
    }
    CM_CalcPAS(model);
    assert(CM_LeafPVS(MAX_MAP_LEAFS)[ModelVisibility::MaximumRowBytes - 1] == 0);
    assert(CM_LeafPAS(MAX_MAP_LEAFS)[ModelVisibility::MaximumRowBytes - 1] == 0);
    CM_FreePAS();
    model->numleafs = 0;
    CM_CalcPAS(model);
    assert(liveCacheAllocations == 0);

    dheader_t header = {};
    header.lumps[LUMP_LEAFS] = {sizeof(header), sizeof(dleaf_t)};
    header.lumps[LUMP_VISIBILITY] = {sizeof(header), 0};
    const int fileBytes = sizeof(header) + sizeof(dleaf_t);
    assert(ModelVisibility::BrushLumps(&header, fileBytes));
    for (int bytes = 0; bytes < static_cast<int>(sizeof(header)); ++bytes) {
        assert(!ModelVisibility::BrushLumps(&header, bytes));
    }
    // Only metadata is read: exercise native allocator boundaries without huge backing allocations.
    const dheader_t smallHeader = header;
    constexpr int MaximumVisibilityPayload = static_cast<int>(cse::memory::MaximumHunkPayloadBytes);
    header.lumps[LUMP_VISIBILITY] = {0, MaximumVisibilityPayload};
    assert(ModelVisibility::BrushLumps(&header, INT_MAX));
    header.lumps[LUMP_VISIBILITY].filelen = MaximumVisibilityPayload + 1;
    assert(!ModelVisibility::BrushLumps(&header, INT_MAX));
    header = smallHeader;
    constexpr int MaximumPhysicalLeafCount =
        static_cast<int>(cse::memory::MaximumHunkPayloadBytes / sizeof(mleaf_t));
    header.lumps[LUMP_LEAFS] = {0, MaximumPhysicalLeafCount * static_cast<int>(sizeof(dleaf_t))};
    assert(ModelVisibility::BrushLumps(&header, INT_MAX));
    header.lumps[LUMP_LEAFS].filelen += sizeof(dleaf_t);
    assert(!ModelVisibility::BrushLumps(&header, INT_MAX));
    header = smallHeader;
    for (int lumpIndex : {LUMP_LEAFS, LUMP_VISIBILITY}) {
        const lump_t saved = header.lumps[lumpIndex];
        for (int invalid : {-1, INT_MAX}) {
            header.lumps[lumpIndex].fileofs = invalid;
            assert(!ModelVisibility::BrushLumps(&header, fileBytes));
        }
        header.lumps[lumpIndex] = saved;
        header.lumps[lumpIndex].filelen = -1;
        assert(!ModelVisibility::BrushLumps(&header, fileBytes));
        header.lumps[lumpIndex] = saved;
    }
#if defined(SW_RENDERER) && defined(__APPLE__)
    model = LoadFixture();
    Mod_ClearAll();
    SW_Mod_Init();
    assert(mod_numknown == 0 && Mod_GetVisibilityInfo(model).leafCount == 0);
#endif
}
#endif

int main(int argc, char** argv) {
    Mod_Init();
    const std::string test = argc > 1 ? argv[1] : "all";
    if (test == "empty" || test == "all") {
        const byte input[] = {0x55};
        byte output[] = {0xa5};
        Decode(input, sizeof(input), output, 0);
        assert(output[0] == 0xa5);
    }
    if (test == "cache-boundary" || test == "all") {
#if defined(PVS_BASELINE)
        model_t localModel = {};
        model_t* model = &localModel;
#else
        model_t* model = LoadFixture();
        mod_known_info[0].visibility.leafCount = 33;
#endif
        std::vector<mleaf_t> leafs(33);
        model->numleafs = 32;
        model->leafs = leafs.data();
        CM_CalcPAS(model);
        assert(CM_LeafPVS(32)[3] == 255 && CM_LeafPAS(32)[3] == 255);
        CM_FreePAS();
    }
    if (test == "padded" || test == "all") {
        const byte input[] = {0, 255};
        byte output[] = {0xa5, 0xa5};
        Decode(input, sizeof(input), output, 1);
        assert(output[0] == 0 && output[1] == 0xa5);
    }
#if !defined(PVS_BASELINE)
    if (test == "all") {
        const byte mixed[] = {0x81, 0, 3, 0x02};
        byte output[] = {0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5};
        Decode(mixed, sizeof(mixed), output, 5);
        const byte expected[] = {0x81, 0, 0, 0, 2, 0xa5};
        assert(std::memcmp(output, expected, sizeof(output)) == 0);
        for (std::size_t bytes = 0; bytes < sizeof(mixed); ++bytes) {
            assert(!CM_DecompressPVS(mixed, bytes, output, 5));
        }
        const byte zeroRun[] = {0, 0};
        assert(!CM_DecompressPVS(zeroRun, sizeof(zeroRun), output, 1));
        assert(CM_DecompressPVS(nullptr, 0, nullptr, 0));
        assert(!CM_DecompressPVS(mixed, sizeof(mixed), nullptr, 1));
        assert(!CM_DecompressPVS(nullptr, 0, output, ModelVisibility::MaximumRowBytes + 1));
        Decode(nullptr, 0, output, 5);
        assert(output[0] == 255 && output[4] == 255 && output[5] == 0xa5);
        std::vector<byte> maximum(ModelVisibility::MaximumRowBytes + 2, 0xa5);
        std::vector<byte> compressed;
        for (std::size_t bytes = 0; bytes < ModelVisibility::MaximumRowBytes; bytes += 255) {
            compressed.push_back(0);
            compressed.push_back(255);
        }
        Decode(compressed.data(), compressed.size(), maximum.data() + 1, ModelVisibility::MaximumRowBytes);
        assert(maximum.front() == 0xa5 && maximum.back() == 0xa5);
        assert(std::all_of(maximum.begin() + 1, maximum.end() - 1, [](byte value) {
            return value == 0;
        }));
        RunConsumers();
        if (argc > 2) {
            RunCorpus(argv[2]);
        }
    }
#endif
    CM_FreePAS();
    assert(liveCacheAllocations == 0);
    for (void* allocation : hunkAllocations) {
        std::free(allocation);
    }
    std::puts("PASS visibility cases");
}
