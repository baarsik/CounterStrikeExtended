#include "thread_api.h"
#include <mach/port.h>
#include <pthread.h>

namespace {
bool forceFailure = true;
mach_port_t controlledId = MACH_PORT_NULL;
}

extern "C" mach_port_t CseFixtureNativeThreadId(pthread_t thread) {
    return forceFailure ? controlledId : pthread_mach_thread_np(thread);
}

int main() {
    Require(Plat_GetCurrentThreadID() == 0 && Plat_PrimaryThreadID == 0, "initial failure-control state");
    Require(Plat_RegisterThread(nullptr) == 0 && Plat_GetCurrentThreadID() == 0, "null native ID rejected");
    controlledId = MACH_PORT_DEAD;
    Require(Plat_RegisterPrimaryThread() == 0 && Plat_PrimaryThreadID == 0, "dead native ID rejected");
    forceFailure = false;
    const auto valid = Plat_RegisterPrimaryThread();
    Require(valid != 0 && Plat_PrimaryThreadID == valid && Plat_GetCurrentThreadID() == valid,
        "registration recovers through actual native API");
    forceFailure = true;
    controlledId = MACH_PORT_NULL;
    Require(Plat_RegisterPrimaryThread() == 0 && Plat_GetCurrentThreadID() == 0,
        "failed registration leaves caller unregistered");
    Require(Plat_PrimaryThreadID == valid, "failed registration does not replace established primary");
    std::puts("controlled_native_id_failure_scenario_passed=1");
    return 0;
}
