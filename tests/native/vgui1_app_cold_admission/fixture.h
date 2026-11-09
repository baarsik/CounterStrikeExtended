#ifndef CSE_APP_COLD_FIXTURE_H
#define CSE_APP_COLD_FIXTURE_H
#include <cstdint>

// Fixture observation only; no substitute for a production App declaration.
struct ColdObservation {
    std::uint32_t appBytes;
    std::uint32_t fonts;
    std::uint32_t cursors;
    std::uint32_t nextTick;
    std::uint32_t minimumTick;
    std::uint32_t coldClockCalls;
};
extern "C" void FixtureColdClockHit();
extern "C" unsigned int FixtureColdClockCalls();
extern "C" unsigned int FixtureProviderCalls();
extern "C" void FixtureUnexpectedProviderCall();
#endif
