#pragma once

#include "server.h"

server_t sv;
int fatbytes;
byte fatpvs[MAX_MAP_LEAFS / ModelVisibility::BitsPerByte];
int fatpasbytes;
byte fatpas[MAX_MAP_LEAFS / ModelVisibility::BitsPerByte];

void Q_memset(void* output, int value, int bytes) {
    std::memset(output, value, bytes);
}

void SV_AddToFatPVS(vec_t*, mnode_t*) {}
void SV_AddToFatPAS(vec_t*, mnode_t*) {}

#include "fat_visibility.inc"
