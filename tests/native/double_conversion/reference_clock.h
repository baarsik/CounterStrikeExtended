#ifndef CSE_DOUBLE_REFERENCE_CLOCK_H
#define CSE_DOUBLE_REFERENCE_CLOCK_H

#include <cstdint>

// Controlled binding for the exact extracted arithmetic method, not a replacement
// VGUI provider. Its public interface and actual clock backend are not instantiated.
class CSystem {
public:
    virtual ~CSystem() = default;
    virtual double GetCurrentTime();
    virtual long GetTimeMillis();
};

void SetReferenceSeconds(double seconds);
double ReferenceScaleSeconds(double seconds);
std::int32_t ReferenceRawDoubleCast(double value);

#endif
