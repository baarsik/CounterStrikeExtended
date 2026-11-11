#include "SystemWrapper.h"
#include "IDemoPlayer.h"
#include "HLTV/common/INetwork.h"
#include "HLTV/common/IServer.h"
#include <type_traits>
#include <cstdio>

int main() {
    int failures = 0;
#define REQUIRE_BASE(Type) \
    if (!std::is_base_of<ISystemModule, Type>::value) { \
        std::fprintf(stderr, #Type " lacks the canonical module base\n"); \
        ++failures; \
    }
#if INTPTR_MAX <= INT32_MAX
    static_assert(!std::is_base_of<ISystemModule, IDemoPlayer>::value, "legacy demo declaration changed");
    static_assert(!std::is_base_of<ISystemModule, IServer>::value, "legacy server declaration changed");
    static_assert(!std::is_base_of<ISystemModule, INetwork>::value, "legacy network declaration changed");
    return 0;
#else
    REQUIRE_BASE(IEngineWrapper)
    REQUIRE_BASE(IDemoPlayer)
    REQUIRE_BASE(INetwork)
    REQUIRE_BASE(IServer)
    return failures ? 1 : 0;
#endif
}
