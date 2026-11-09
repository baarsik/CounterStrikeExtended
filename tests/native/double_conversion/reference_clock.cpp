#include "reference_clock.h"

namespace {

volatile double controlledSeconds = 0.0;

}

void SetReferenceSeconds(double seconds) {
    controlledSeconds = seconds;
}

double CSystem::GetCurrentTime() {
    return controlledSeconds;
}

__declspec(noinline) double ReferenceScaleSeconds(double seconds) {
    return seconds * 1000.0;
}

__declspec(noinline) std::int32_t ReferenceRawDoubleCast(double value) {
    static_assert(sizeof(long) == sizeof(std::int32_t), "The actual Windows long profile is required");
    return static_cast<long>(value);
}
