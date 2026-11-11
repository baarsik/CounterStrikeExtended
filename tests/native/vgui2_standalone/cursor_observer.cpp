#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {
std::map<SDL_Cursor*, SDL_SystemCursor> cursors;
SDL_Cursor* simulatedCurrent = nullptr;
int creates = 0;
int sets = 0;
int destroys = 0;
int failStage = 0;
constexpr int FailCreation = 1;
constexpr int FailSelection = 2;
constexpr int CreateCalls = 0;
constexpr int SelectCalls = 1;
constexpr int DestroyCalls = 2;

bool Simulated() {
    const char* mode = std::getenv("CSE_CURSOR_SIMULATED");
    if (mode && std::strcmp(mode, "1") == 0) {
        const char* driver = SDL_GetCurrentVideoDriver();
        if (!driver || std::strcmp(driver, "dummy") != 0) {
            std::abort();
        }
        return true;
    }
    return false;
}

bool Fail(int stage) {
    if (failStage != stage) {
        return false;
    }
    failStage = 0;
    SDL_SetError("controlled cursor boundary failure");
    return true;
}

SDL_Cursor* Create(SDL_SystemCursor shape) {
    ++creates;
    if (Fail(FailCreation)) {
        return nullptr;
    }
    // Dummy has no cursor backend. This mode tests surface ownership only.
    SDL_Cursor* cursor = Simulated() ? reinterpret_cast<SDL_Cursor*>(new int(0)) : SDL_CreateSystemCursor(shape);
    if (cursor) {
        cursors.emplace(cursor, shape);
    }
    return cursor;
}

bool Set(SDL_Cursor* cursor) {
    ++sets;
    if (Fail(FailSelection)) {
        return false;
    }
    if (Simulated()) {
        if (cursor && cursors.count(cursor) == 0) {
            std::abort();
        }
        simulatedCurrent = cursor;
        return true;
    }
    return SDL_SetCursor(cursor);
}

SDL_Cursor* Get() {
    return Simulated() ? simulatedCurrent : SDL_GetCursor();
}

void Destroy(SDL_Cursor* cursor) {
    if (!cursor) {
        return;
    }
    if (cursors.erase(cursor) != 1 || SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        std::abort();
    }
    ++destroys;
    if (Simulated()) {
        if (simulatedCurrent == cursor) {
            simulatedCurrent = nullptr;
        }
        delete reinterpret_cast<int*>(cursor);
    } else {
        SDL_DestroyCursor(cursor);
    }
}

void Quit(SDL_InitFlags flags) {
    if ((flags & SDL_INIT_VIDEO) && !cursors.empty()) {
        std::fputs("FAIL: cursor survives video retirement\n", stderr);
        std::abort();
    }
    SDL_QuitSubSystem(flags);
}
} // namespace

extern "C" __attribute__((visibility("default"))) int CseCursorCount(int kind) {
    switch (kind) {
    case CreateCalls:
        return creates;
    case SelectCalls:
        return sets;
    case DestroyCalls:
        return destroys;
    default:
        return static_cast<int>(cursors.size());
    }
}

extern "C" __attribute__((visibility("default"))) int CseCursorShape() {
    const auto found = cursors.find(Get());
    return found == cursors.end() ? -1 : static_cast<int>(found->second);
}

extern "C" __attribute__((visibility("default"))) void CseCursorFail(int stage) {
    failStage = stage;
}

#define INTERPOSE(replacement, original) \
    __attribute__((used, section("__DATA,__interpose"))) static const struct { \
        const void* replace; \
        const void* originalAddress; \
    } interpose_##original = {reinterpret_cast<const void*>(&replacement), reinterpret_cast<const void*>(&original)}
INTERPOSE(Create, SDL_CreateSystemCursor);
INTERPOSE(Set, SDL_SetCursor);
INTERPOSE(Get, SDL_GetCursor);
INTERPOSE(Destroy, SDL_DestroyCursor);
INTERPOSE(Quit, SDL_QuitSubSystem);
