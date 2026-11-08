#pragma once

#include <cstddef>

struct sfx_s;

typedef struct voxword {
    int volume;
    int pitch;
    int start;
    int end;
    int cbtrim;
    int fKeepCached;
    int samplefrac;
    int timecompress;
    sfx_s *sfx;
} voxword_t;

// Sentence words live in memory and are rebuilt from text, never written as disk records.
constexpr std::size_t VoxIntegerFields = 8;
static_assert(offsetof(voxword_t, sfx) == VoxIntegerFields * sizeof(int));
static_assert(sizeof(voxword_t) == VoxIntegerFields * sizeof(int) + sizeof(sfx_s *));
