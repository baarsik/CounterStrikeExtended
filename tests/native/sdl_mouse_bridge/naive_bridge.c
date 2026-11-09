#define CSE_BRIDGE_FIXTURE_BUILD 1
#include "fixture_api.h"
#include "SDL_internal.h"
#include "events/SDL_mouse_c.h"

uint32_t FixtureNaiveQuery(void) {
    const uint32_t absolute = 1;
    const uint32_t relative = 2;
    return SDL_GetRelativeMouseMode() ? relative : absolute;
}
