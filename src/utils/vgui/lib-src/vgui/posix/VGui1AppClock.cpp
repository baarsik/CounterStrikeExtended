#if !defined(__APPLE__)
#error This real clock backend is selected only for Apple; fixtures supply a fake clock.
#endif

#include "VGui1AppPlatform.h"
#include <mach/mach_time.h>

namespace cse_vgui1_app {
bool ReadColdClock(std::uint32_t& encodedMillis) noexcept {
    mach_timebase_info_data_t timebase{};
    if (mach_timebase_info(&timebase) != KERN_SUCCESS) {
        return false;
    }
    // SDK contract: continuous time advances during sleep. No video/UI subsystem,
    // per-App epoch or late singleton is needed by an engine static constructor.
    return EncodeClockTicks(mach_continuous_time(), timebase.numer, timebase.denom, encodedMillis);
}
}
