#if !defined(CSE_APP_COLD_FIXTURE)
#error Test-only aborting provider may never be selected by a production target
#endif
#include "VGui1FontPlatform.h"
#include "VGui1AppPlatform.h"
#include "fixture.h"
#include <cstdlib>
namespace cse_vgui1 {
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    FixtureUnexpectedProviderCall();
    std::abort();
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) {
    std::abort();
}
}
namespace cse_vgui1_app {
bool ReadColdClock(std::uint32_t& value) noexcept {
    FixtureColdClockHit();
    constexpr std::uint32_t FixtureMillis = 1234;
    value = FixtureMillis;
    return true;
}
}
