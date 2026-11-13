#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <vector>

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../src/common/const.h"
#include "../../../src/engine/progdefs.h"
#include "../../../src/engine/edict.h"
#include "../../../src/engine/eiface.h"

static_assert(std::is_same<decltype(enginefuncs_t::pfnPvAllocEntPrivateData),
    void *(*)(edict_t *, std::size_t)>::value, "Private allocation sizes must retain native width");

NEW_DLL_FUNCTIONS gNewDLLFunctions{};

namespace {
enum class Event { Notify, Free, Allocate };
std::vector<Event> events;
unsigned char storage[128]{};
edict_t *expectedEntity = nullptr;
void *expectedOldData = nullptr;
std::size_t requestedBytes = 0;
bool validOrdering = true;
bool failAllocation = false;

void NotifyFree(edict_t *entity) {
    validOrdering = validOrdering && entity == expectedEntity && entity->pvPrivateData == expectedOldData;
    events.push_back(Event::Notify);
}

void Mem_Free(void *memory) {
    validOrdering = validOrdering && memory == expectedOldData;
    events.push_back(Event::Free);
}

void *Mem_Calloc(int count, std::size_t bytes) {
    validOrdering = validOrdering && count == 1 && expectedEntity->pvPrivateData == nullptr;
    requestedBytes = bytes;
    events.push_back(Event::Allocate);
    return failAllocation ? nullptr : storage;
}
} // namespace

#include "../../../src/engine/entity_private_data_impl.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Private data check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (false)

int main() {
    edict_t entity{};
    expectedEntity = &entity;
    gNewDLLFunctions.pfnOnFreeEntPrivateData = NotifyFree;
    enginefuncs_t callbacks{};
    callbacks.pfnPvAllocEntPrivateData = PvAllocEntPrivateData;
    callbacks.pfnFreeEntPrivateData = FreeEntPrivateData;
    CHECK(callbacks.pfnPvAllocEntPrivateData(&entity, sizeof(storage)) == storage);
    CHECK(entity.pvPrivateData == storage && requestedBytes == sizeof(storage));
    CHECK(events == std::vector<Event>{Event::Allocate});
    expectedOldData = storage;
    events.clear();
    CHECK(callbacks.pfnPvAllocEntPrivateData(&entity, 0) == nullptr);
    CHECK(entity.pvPrivateData == nullptr);
    CHECK((events == std::vector<Event>{Event::Notify, Event::Free}));
    events.clear();
    callbacks.pfnFreeEntPrivateData(&entity);
    CHECK(events.empty());
    entity.pvPrivateData = storage;
    failAllocation = true;
    std::size_t largeRequest = UINT32_MAX;
#if SIZE_MAX > UINT32_MAX
    largeRequest = static_cast<std::size_t>((UINT64_C(1) << 32) + sizeof(storage));
#endif
    CHECK(callbacks.pfnPvAllocEntPrivateData(&entity, largeRequest) == nullptr);
    CHECK(requestedBytes == largeRequest && entity.pvPrivateData == nullptr);
    CHECK((events == std::vector<Event>{Event::Notify, Event::Free, Event::Allocate}));
    events.clear();
    gNewDLLFunctions.pfnOnFreeEntPrivateData = nullptr;
    entity.pvPrivateData = storage;
    callbacks.pfnFreeEntPrivateData(&entity);
    CHECK(events == std::vector<Event>{Event::Free});
    CHECK(validOrdering && entity.pvPrivateData == nullptr);
    std::puts("Private data native sizes, callbacks, replacement and allocation failure passed");
    return 0;
}
