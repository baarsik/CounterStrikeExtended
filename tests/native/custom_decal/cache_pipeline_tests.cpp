// Reuse the actual CPU builder imports and typed renderer bindings; its standalone
// oracle entrypoint is compiled but is not the pipeline entrypoint.
#define main StandaloneBuilderOracleMain
#include "pipeline_tests.cpp"
#undef main
#include "engine_apiproxy.h"
#include "client.h"
#include "zone.h"
#include "mem.h"
#if defined(_WIN64) || defined(__LP64__)
#include "custom_decal_loader.h"
#endif
#include <climits>
#include <map>

namespace {
constexpr std::size_t GuardBytes = 16;
struct Allocation {
    byte* base;
    std::size_t bytes;
    bool cache;
};
std::map<void*, Allocation> live;
std::map<cache_user_t*, void*> cacheUsers;
std::vector<byte> inputBytes;
unsigned allocationCalls = 0;
unsigned failAllocation = 0;
unsigned cacheBuildAllocations = 0;
unsigned cacheFrees = 0;
unsigned heapFrees = 0;
unsigned fileHooks = 0;
unsigned decalRemovals = 0;
int removedTexture = 0;
bool hpakFails = false;
bool sourceShort = false;
bool sourceOpen = false;
std::size_t sourcePosition = 0;
bool corruptCacheCopy = false;
void* lastCache = nullptr;

void Guards(const Allocation& allocation) {
    for (std::size_t index = 0; index < GuardBytes; ++index) {
        Check(allocation.base[index] == 0xa5 && allocation.base[GuardBytes + allocation.bytes + index] == 0xa5,
              "caller/cache allocation canaries");
    }
}

void* Allocate(std::size_t size, bool cache) {
    ++allocationCalls;
    if (allocationCalls == failAllocation) {
        return nullptr;
    }
    byte* base = static_cast<byte*>(std::malloc(size + GuardBytes * 2));
    Check(base != nullptr, "owned fixture memory");
    std::memset(base, 0xa5, size + GuardBytes * 2);
    void* output = base + GuardBytes;
    if (sizeof(void*) > 4) {
        Check(reinterpret_cast<std::uintptr_t>(output) > UINT32_MAX, "native high-address ownership");
    }
    Check(live.emplace(output, Allocation{base, size, cache}).second, "unique allocation");
    return output;
}

void Release(void* memory, bool cache) {
    if (!memory) {
        return;
    }
    const auto found = live.find(memory);
    Check(found != live.end() && found->second.cache == cache, "matching owner and one release");
    Guards(found->second);
    if (!cache) {
        for (const auto& user : cacheUsers) {
            const auto address = reinterpret_cast<std::uintptr_t>(user.first);
            const auto begin = reinterpret_cast<std::uintptr_t>(memory);
            Check(address < begin || address - begin >= found->second.bytes,
                  "cache users released before freeing metadata array");
        }
    }
    std::free(found->second.base);
    live.erase(found);
}

void ResetPipeline(const std::vector<byte>& bytes) {
    Check(live.empty() && cacheUsers.empty() && !sourceOpen, "previous operation fully released");
    inputBytes = bytes;
    allocationCalls = 0;
    failAllocation = 0;
    cacheBuildAllocations = 0;
    cacheFrees = 0;
    heapFrees = 0;
    fileHooks = 0;
    decalRemovals = 0;
    removedTexture = 0;
    hpakFails = false;
    sourceShort = false;
    sourcePosition = 0;
    corruptCacheCopy = false;
    lastCache = nullptr;
    uploads = 0;
    unloads = 0;
    capturedBitmap.clear();
    capturedPalette.clear();
    for (unsigned index = 0; index < 256; ++index) {
        texgammatable[index] = static_cast<byte>((index * 13 + 7) & 255);
    }
}

resource_t MakeResource(std::size_t logicalBytes) {
    resource_t resource{};
    std::strcpy(resource.szFileName, "tempdecal.wad");
    resource.type = t_decal;
    resource.ucFlags = RES_CUSTOM;
    resource.nDownloadSize = static_cast<int>(logicalBytes);
    return resource;
}

void CopyFault(void* output, int size) {
    if (corruptCacheCopy && lastCache && output == static_cast<byte*>(lastCache) + MIP_EXTRASIZE && size > 40) {
        static_cast<byte*>(output)[16] = 0;
        static_cast<byte*>(output)[17] = 0;
        static_cast<byte*>(output)[18] = 0;
        static_cast<byte*>(output)[19] = 0;
        corruptCacheCopy = false;
    }
}

void FileHook(char**, int* mode, int** length) {
    Check(*mode == 5 && *length == nullptr, "actual customization hook arguments");
    ++fileHooks;
}
} // namespace

decltype(cls) cls{};
decltype(g_engdstAddrs) g_engdstAddrs{};
cache_user_t* loadcache = nullptr;
byte* loadbuf = nullptr;
int loadsize = 0;

void* Mem_Malloc(std::size_t size) {
    return Allocate(size, false);
}
void* Mem_ZeroMalloc(std::size_t size) {
    void* output = Mem_Malloc(size);
    if (output) {
        std::memset(output, 0, size);
    }
    return output;
}
void Mem_Free(void* memory) {
    if (memory) {
        ++heapFrees;
    }
    Release(memory, false);
}
char* Mem_Strdup(const char* text) {
    const std::size_t bytes = std::strlen(text) + 1;
    char* output = static_cast<char*>(Mem_Malloc(bytes));
    if (output) {
        std::memcpy(output, text, bytes);
    }
    return output;
}
void Q_memset(void* output, int value, int bytes) {
    Check(bytes >= 0, "nonnegative initialized extent");
    std::memset(output, value, static_cast<std::size_t>(bytes));
}
int Q_strlen(const char* text) {
    return static_cast<int>(std::strlen(text));
}
int Q_atoi(const char* text) {
    return std::atoi(text);
}
int Q_strcmp(const char* left, const char* right) {
    return std::strcmp(left, right);
}
void Q_strcpy(char* output, const char* input) {
    std::strcpy(output, input);
}
void* Cache_Check(cache_user_t* user) {
    const auto found = cacheUsers.find(user);
    Check(user->data ? found != cacheUsers.end() && found->second == user->data : found == cacheUsers.end(),
          "coherent cache lifetime");
    if (user->data) {
        Guards(live.at(user->data));
    }
    return user->data;
}
void* Cache_Alloc(cache_user_t* user, int bytes, const char*) {
    Check(user && !user->data && bytes > 0, "cache allocation contract");
    void* output = Allocate(static_cast<std::size_t>(bytes), true);
    if (output) {
        user->data = output;
        Check(cacheUsers.emplace(user, output).second, "one cache owner");
        lastCache = output;
        ++cacheBuildAllocations;
    }
    return output;
}
void Cache_Free(cache_user_t* user) {
    Check(user && user->data && cacheUsers.erase(user) == 1, "cache release owns published data");
    Release(user->data, true);
    user->data = nullptr;
    ++cacheFrees;
}
void R_DecalRemoveAll(int texture) {
    ++decalRemovals;
    removedTexture = texture;
}
qboolean HPAK_GetDataPointer(const char* name, resource_t*, byte** output, int* size) {
    Check(std::strcmp(name, "custom.hpk") == 0 && output, "actual HPAK customization call");
#if defined(_WIN64) || defined(__LP64__)
    Check(size != nullptr, "native caller requires actual HPAK length");
#endif
    *output = nullptr;
    if (size) {
        *size = 0;
    }
    if (hpakFails) {
        return FALSE;
    }
    if (!inputBytes.empty()) {
        *output = static_cast<byte*>(Mem_Malloc(inputBytes.size()));
        if (!*output) {
            return FALSE;
        }
        std::memcpy(*output, inputBytes.data(), inputBytes.size());
    }
    if (size) {
        *size = static_cast<int>(inputBytes.size());
    }
    return TRUE;
}
FileHandle_t FS_Open(const char* name, const char* mode) {
    Check(!sourceOpen && std::strcmp(name, "tempdecal.wad") == 0 && std::strcmp(mode, "rb") == 0,
          "owned source path");
    sourceOpen = true;
    sourcePosition = 0;
    return &inputBytes;
}
unsigned int FS_Size(FileHandle_t file) {
    Check(sourceOpen && file == &inputBytes, "source size handle");
    return static_cast<unsigned int>(inputBytes.size());
}
int FS_Read(void* output, int bytes, int count, FileHandle_t file) {
    Check(sourceOpen && file == &inputBytes && bytes >= 0 && count == 1 && sourcePosition == 0,
          "source complete byte read");
    const int copied = bytes - (sourceShort && bytes ? 1 : 0);
    std::memcpy(output, inputBytes.data(), static_cast<std::size_t>(copied));
    sourcePosition += static_cast<std::size_t>(copied);
    return copied;
}
void FS_Close(FileHandle_t file) {
    Check(sourceOpen && file == &inputBytes, "source close handle");
    sourceOpen = false;
}
void* Z_Malloc(int) {
    std::abort();
}
void* Hunk_AllocName(int, const char*) {
    std::abort();
}
void* Hunk_TempAlloc(int) {
    std::abort();
}

#include "names.inc"
#include "loader.inc"
#include "custom_functions.inc"

int main(int argc, char** argv) {
    Check(argc == 2 || argc == 3, "independently authored WAD required");
    const std::vector<byte> raw = Load(argv[1]);
    g_engdstAddrs.COM_LoadFile = FileHook;
    afterFixtureCopy = CopyFault;
    cls.state = ca_active;
    ResetPipeline(raw);
    resource_t resource = MakeResource(raw.size());
    customization_t head{};
    customization_t* created = nullptr;
    int lumps = -27;
    if (argc == 3 && std::strcmp(argv[2], "--large-flags") == 0) {
        Check(raw.size() > 20480, "authored large class");
        for (int flags : {0, FCUST_WIPEDATA, FCUST_IGNOREINIT, FCUST_WIPEDATA | FCUST_IGNOREINIT}) {
            ResetPipeline(raw);
            resource = MakeResource(raw.size());
            lumps = -27;
            const bool ignored = (flags & FCUST_IGNOREINIT) != 0;
            const bool result = COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK | flags, &created, &lumps);
            Check(result == ignored && cacheBuildAllocations == 1 && cacheFrees == 1,
                  "large file still validates; init-only size rule follows ignore flag");
            if (result) {
                Check(created->pBuffer && !created->pInfo && !created->bTranslated && lumps == -27,
                      "large ignored initialization preserves raw and output semantics");
                COM_ClearCustomizationList(&head, FALSE);
            }
            Check(live.empty() && cacheUsers.empty() && !head.pNext, "large flag case released");
        }
        return 0;
    }
    if (argc == 3) {
        Check(std::strcmp(argv[2], "--reject") == 0 && sizeof(void*) > 4, "native-only malformed input mode");
        Check(!COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK, &created, &lumps)
            && !created && !head.pNext && live.empty() && cacheUsers.empty() && cacheBuildAllocations == 0,
            "header-alias malformed class rejected before builder allocation");
        return 0;
    }
    Check(COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK, &created, &lumps), "actual create succeeds");
    Check(head.pNext == created && created && created->bTranslated && lumps == 1 && created->nUserData2 == 1,
          "published valid customization fields");
    const unsigned completeCreateAllocations = allocationCalls;
    auto* wad = static_cast<cachewad_t*>(created->pInfo);
    Check(wad && wad->cacheCount == 1 && wad->cacheMax == 16 && !wad->cache[0].cache.data,
          "validation cache freed; persistent metadata starts uncached");
    void* texture = Draw_CustomCacheGet(wad, created->pBuffer, resource.nDownloadSize, 0);
    Check(texture && Cache_Check(&wad->cache[0].cache) == texture, "actual lazy reload builds cache");
    const unsigned firstBuilds = cacheBuildAllocations;
    Check(Draw_CustomCacheGet(wad, nullptr, 0, 0) == texture && cacheBuildAllocations == firstBuilds,
          "cache hit does not reparse or consume source");
    Cache_Free(&wad->cache[0].cache);
    Check(Draw_CustomCacheGet(wad, created->pBuffer, resource.nDownloadSize, 0), "eviction reload succeeds");
#if defined(_WIN64) || defined(__LP64__)
    Cache_Free(&wad->cache[0].cache);
    static_cast<byte*>(created->pBuffer)[0] = 0;
    Check(!Draw_CustomCacheGet(wad, created->pBuffer, resource.nDownloadSize, 0) && !wad->cache[0].cache.data,
          "malformed reloaded source is not cached");
    static_cast<byte*>(created->pBuffer)[0] = 'W';
    corruptCacheCopy = true;
    Check(!Draw_CustomCacheGet(wad, created->pBuffer, resource.nDownloadSize, 0) && !wad->cache[0].cache.data
        && !corruptCacheCopy, "failed copied-cache validation frees published allocation");
    Check(Draw_CustomCacheGet(wad, created->pBuffer, resource.nDownloadSize, 0), "retry after failure builds fresh data");
    Check(!Draw_CustomCacheGet(wad, created->pBuffer, resource.nDownloadSize, -1), "negative cache index rejected");
#endif
    COM_ClearCustomizationList(&head, TRUE);
    Check(!head.pNext && live.empty() && cacheUsers.empty() && decalRemovals == 1 && removedTexture == ~5,
          "clear closes cache users and metadata once");
    COM_ClearCustomizationList(&head, TRUE);
    Check(decalRemovals == 1, "empty repeat clear");

    for (int flags : {0, FCUST_WIPEDATA, FCUST_IGNOREINIT, FCUST_WIPEDATA | FCUST_IGNOREINIT}) {
        ResetPipeline(raw);
        resource = MakeResource(raw.size());
        created = nullptr;
        lumps = -27;
        const bool allowedSize = raw.size() >= 1024 && raw.size() <= 20480;
        const bool ignored = (flags & FCUST_IGNOREINIT) != 0;
        if (!allowedSize && !ignored) {
            Check(!COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK | flags, &created, &lumps)
                && !created && !head.pNext && live.empty(), "legacy init size gate retained");
            continue;
        }
        Check(COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK | flags, &created, &lumps), "flag create");
        const bool wiped = (flags & FCUST_WIPEDATA) != 0;
        Check(created->pBuffer && (created->pInfo != nullptr) == (!ignored && !wiped), "raw retained; info flags honored");
        Check(!!created->bTranslated == !ignored && lumps == (ignored ? -27 : 1), "translation and output flag semantics");
        Check(cacheBuildAllocations == 1 && cacheFrees == 1, "validation always builds and frees its temporary cache");
        COM_ClearCustomizationList(&head, FALSE);
        Check(live.empty() && cacheUsers.empty(), "flag ownership released");
    }
    ResetPipeline(raw);
    resource = MakeResource(raw.size());
    Check(COM_CreateCustomization(&head, &resource, 5, 0, &created, nullptr) && fileHooks == 1,
          "actual strict disk loader hook once");
    COM_ClearCustomizationList(&head, FALSE);
#if defined(_WIN64) || defined(__LP64__)
    for (unsigned failure = 1; failure <= completeCreateAllocations; ++failure) {
        ResetPipeline(raw);
        failAllocation = failure;
        resource = MakeResource(raw.size());
        customization_t existing{};
        head.pNext = &existing;
        created = reinterpret_cast<customization_t*>(static_cast<std::uintptr_t>(1));
        lumps = -27;
        Check(!COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK, &created, &lumps), "allocation rollback");
        Check(head.pNext == &existing && !created && lumps == -27 && live.empty() && cacheUsers.empty(),
              "failed create never changes existing list or outputs/leaks ownership");
        head.pNext = nullptr;
    }
    ResetPipeline(raw);
    resource = MakeResource(raw.size() + 1);
    Check(!COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK, &created, nullptr)
        && live.empty() && cacheBuildAllocations == 0, "claimed length cannot exceed HPAK allocation");
    ResetPipeline(raw);
    sourceShort = true;
    resource = MakeResource(raw.size());
    Check(!COM_CreateCustomization(&head, &resource, 5, 0, &created, nullptr) && live.empty() && fileHooks == 1,
          "short heap file never reaches parser");
    ResetPipeline(raw);
    inputBytes.insert(inputBytes.end(), 16, 0xa5);
    resource = MakeResource(raw.size());
    Check(COM_CreateCustomization(&head, &resource, 5, FCUST_FROMHPAK, &created, nullptr),
          "initialized trailing bytes retain valid logical-prefix semantics");
    COM_ClearCustomizationList(&head, FALSE);
#endif
    Check(live.empty() && cacheUsers.empty() && !sourceOpen, "all pipeline owners released");
    std::printf("actual custom pipeline: metadata allocations=%u; cache/flags/reload/rollback pass\n",
                 completeCreateAllocations);
    return 0;
}
