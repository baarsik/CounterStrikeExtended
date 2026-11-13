#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using vec_t = float;
#include "vector.h"

namespace {
constexpr int NAV_JUMP = 2;
constexpr int WALK_THRU_BREAKABLES = 1;
constexpr int DAMAGE_YES = 1;
constexpr int ignore_monsters = 1;
constexpr int PathCapacity = 4;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

struct entvars_t {
    Vector origin = Vector(0, 0, 36);
    const char* classname = "worldspawn";
    float takedamage = 0;
};

struct edict_t {
    entvars_t variables;
};

struct TraceResult {
    float flFraction = 1;
    edict_t* pHit = nullptr;
    Vector vecEndPos = Vector(0, 0, 0);
};

bool blocked = false;
edict_t obstacle;

void UTIL_TraceLine(const Vector&, const Vector& end, int, edict_t*, TraceResult* trace) {
    trace->flFraction = blocked ? 0.0f : 1.0f;
    trace->pHit = blocked ? &obstacle : nullptr;
    trace->vecEndPos = end;
}

entvars_t* VARS(edict_t* entity) {
    return &entity->variables;
}

bool FClassnameIs(entvars_t* entity, const char* name) {
    return std::strcmp(entity->classname, name) == 0;
}

struct CNavArea {
    int GetAttributes() const {
        return 0;
    }
};

struct PathSegment {
    CNavArea* area = nullptr;
    int how = 0;
    Vector position = Vector(0, 0, 0);
    void* ladder = nullptr;
};

struct CCSBot {
    entvars_t variables;
    entvars_t* pev = &variables;
    int m_pathLength = PathCapacity;
    int m_pathIndex = 1;
    PathSegment m_path[PathCapacity];

    bool IsCrouching() const {
        return true;
    }

    bool IsWalkableTraceLineClear(const Vector&, const Vector&, int) const {
        return !blocked;
    }

    int FindOurPositionOnPath(Vector* closest, bool local) const;
    int FindPathPoint(float aheadRange, Vector* point, int* previousIndex);
};
}

#include "pathfind.inc"

int main() {
    CCSBot bot;
    CNavArea area;
    for (int index = 0; index < PathCapacity; ++index) {
        bot.m_path[index].area = &area;
        bot.m_path[index].position = Vector(100.0f * index, 0, 0);
    }

    bot.variables.origin = Vector(50, 0, 18);
    blocked = true;
    Require(bot.FindOurPositionOnPath(nullptr, true) == -1, "blocked path has no visible segment");
    Vector goal(250, 10, 0);
    int behind = 0;
    const int next = bot.FindPathPoint(300, &goal, &behind);
    Require(next == 1 && goal == Vector(250, 10, 0), "blocked path preserves its current target");
    Require(behind == 0, "blocked path must not expose the -1 search sentinel as an array index");

    blocked = false;
    Require(bot.FindPathPoint(300, &goal, &behind) == 1 && behind == 0,
            "the first visible segment preserves its existing fallback");
    Require(goal == Vector(250, 10, 0), "first-segment fallback leaves the current goal unchanged");

    bot.variables.origin = Vector(150, 0, 18);
    Require(bot.FindPathPoint(300, &goal, &behind) == 2 && behind == 1,
            "later visible segments retain their real previous index");
    Require(goal == Vector(200, 0, 0), "crouching follows the next visible path point");

    blocked = true;
    Require(bot.FindPathPoint(300, &goal, nullptr) == 1, "the previous index remains optional");
    std::puts("blocked and visible bot path-index cases passed");
}
