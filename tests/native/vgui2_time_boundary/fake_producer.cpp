#include "actual_time_type.h"

// The fake accepts only representable millisecond values supplied by the caller.
// It probes the actual interface return type, not either production clock backend.
extern "C" ActualTimeMillis Vgui2FakeTimeProducer(double validMilliseconds) {
    return static_cast<ActualTimeMillis>(validMilliseconds);
}
