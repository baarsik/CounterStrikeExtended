#include "allocation_investigation.h"

namespace {

struct InnerLifetime {
    ~InnerLifetime() {
        ++allocationObservation.innerDestructors;
    }
};

}

// This translation unit deliberately retains the production caller's no-EH policy.
void CallThroughNoExceptions(unsigned int variant, unsigned int count) {
    InnerLifetime lifetime;
    short* allocation = nullptr;
    if (variant == 0) {
        allocation = ReferenceWindowsAllocation(count);
    } else if (variant == 1) {
        allocation = ReferenceNativeAllocation(count);
    } else {
        allocation = ExplicitInvalidLength(count);
    }
    delete[] allocation;
}

short* CallThroughNoExceptionsPlain(unsigned int count) {
    return ReferenceWindowsAllocation(count);
}

short* CallThroughNoExceptionsScalar(unsigned int count) {
    const unsigned int localCount = count;
    short* allocation = ReferenceWindowsAllocation(localCount);
    allocationObservation.scalarCompletions = localCount;
    return allocation;
}
