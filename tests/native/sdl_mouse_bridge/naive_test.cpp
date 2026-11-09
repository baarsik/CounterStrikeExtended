#include "fixture_api.h"
#include <cstdio>

int main() {
    constexpr int assertionFailure = 17;
    constexpr uint32_t foreignDispatch = 5;
    FixtureReset();
    FixtureSetRelative(0);
    FixtureSetForeign(1);
    const uint32_t result = FixtureNaiveQuery();
    if (result != foreignDispatch) {
        std::fprintf(stderr, "naive local query=%u; required ForeignDispatch=%u\n", result, foreignDispatch);
        return assertionFailure;
    }
    return 0;
}
