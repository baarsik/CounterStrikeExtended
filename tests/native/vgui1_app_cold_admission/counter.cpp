#include "fixture.h"
#include <atomic>
#include <cstdlib>
namespace {
std::atomic<unsigned int> clocks{0};
std::atomic<unsigned int> providers{0};
}
extern "C" void FixtureColdClockHit() {
    ++clocks;
}
extern "C" unsigned int FixtureColdClockCalls() {
    return clocks.load();
}
extern "C" unsigned int FixtureProviderCalls() {
    return providers.load();
}
extern "C" void FixtureUnexpectedProviderCall() {
    ++providers;
    std::abort();
}
