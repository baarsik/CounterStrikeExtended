#ifndef CSE_WEBM_ALLOCATION_INVESTIGATION_H
#define CSE_WEBM_ALLOCATION_INVESTIGATION_H

#include <cstddef>

struct AllocationReached {
};

struct AllocationObservation {
    std::size_t bytes;
    unsigned int calls;
    unsigned int innerDestructors;
    unsigned int outerDestructors;
    unsigned int scalarCompletions;
};

extern AllocationObservation allocationObservation;

short* ReferenceWindowsAllocation(unsigned int count);
short* ReferenceNativeAllocation(unsigned int count);
short* ExplicitInvalidLength(unsigned int count);
void CallThroughNoExceptions(unsigned int variant, unsigned int count);
short* CallThroughNoExceptionsPlain(unsigned int count);
short* CallThroughNoExceptionsScalar(unsigned int count);

#endif
