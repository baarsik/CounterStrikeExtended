#pragma once

#include <cstddef>

void FreeEntPrivateData(edict_t *entity) {
    if (entity->pvPrivateData) {
        if (gNewDLLFunctions.pfnOnFreeEntPrivateData) {
            gNewDLLFunctions.pfnOnFreeEntPrivateData(entity);
        }
        Mem_Free(entity->pvPrivateData);
    }
    entity->pvPrivateData = nullptr;
}

void *PvAllocEntPrivateData(edict_t *entity, std::size_t bytes) {
    FreeEntPrivateData(entity);
    if (bytes == 0) {
        return nullptr;
    }
    entity->pvPrivateData = Mem_Calloc(1, bytes);
    return entity->pvPrivateData;
}
