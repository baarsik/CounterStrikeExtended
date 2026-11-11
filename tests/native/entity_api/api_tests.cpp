#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "utlvector.h"

constexpr int FALSE = 0;
constexpr int TRUE = 1;
constexpr int INTERFACE_VERSION = 140;
struct DLL_FUNCTIONS { int marker; };
struct hash_item_t { void* pev; hash_item_t* next; hash_item_t* lastHash; int pevIndex; };
CUtlVector<hash_item_t> stringsHashTable;
DLL_FUNCTIONS gFunctionTable{73};
bool bound = false;
bool CseGameEngineAbiBound() { return bound; }
void EmptyEntityHashTable() {
    for (int index = 0; index < stringsHashTable.Count(); ++index) {
        stringsHashTable[index] = hash_item_t{};
    }
}
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}
#include "entity_api.inc"

int main() {
    DLL_FUNCTIONS callbacks{-1};
    int version = INTERFACE_VERSION;
    Require(!GetEntityAPI2(&callbacks, &version) && callbacks.marker == -1 && stringsHashTable.Count() == 0,
        "unbound API2 changes no callback or hash state");
    bound = true;
    --version;
    Require(!GetEntityAPI2(&callbacks, &version) && version == INTERFACE_VERSION && callbacks.marker == -1,
        "version mismatch returns supported version without partial initialization");
    Require(!GetEntityAPI2(&callbacks, nullptr), "missing version rejected");
    constexpr int EntityHashBucketCount = 2047;
    constexpr int RebindCount = 3;
    for (int cycle = 0; cycle < RebindCount; ++cycle) {
        Require(GetEntityAPI2(&callbacks, &version) && callbacks.marker == gFunctionTable.marker,
            "API2-first initializes actual callbacks");
        Require(stringsHashTable.Count() == EntityHashBucketCount, "API2 retains original fixed bucket count");
        stringsHashTable[0].pev = &callbacks;
        Require(GetEntityAPI(&callbacks, version), "legacy entry uses the same initializer");
        Require(stringsHashTable.Count() == EntityHashBucketCount && stringsHashTable[0].pev == nullptr,
            "API1/API2 rebind empties instead of appending buckets");
    }
}
