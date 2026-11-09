//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Basic random number generator
//
// $NoKeywords: $
//=============================================================================

#include "Random.h"

#include <time.h>
#if !defined(_WIN32)
#include <cstdint>
#include <cstring>
#endif

#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV ( 1 + ( IM - 1 ) / NTAB )

// fran1 -- return a random floating-point number on the interval [0,1)
//
#define AM ( 1.0 / IM )
#define EPS 1.2e-7
#define RNMX ( 1.0 - EPS )
#define MAX_RANDOM_RANGE 0x7FFFFFFFu

#if defined(_WIN32)
using RandomState = long;
#else
using RandomState = std::int32_t;

namespace {
std::int32_t RandomSignedWord(std::uint32_t bits) {
    std::int32_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}
#endif

static RandomState idum;

void SeedRandomNumberGenerator(long seed) {
    constexpr RandomState kSeedNormalizationLimit = 1000;
    constexpr RandomState kSmallSeedOffset = 22261048;
#if defined(_WIN32)
    idum = seed ? seed : -(long)time(NULL);
#else
    // A Windows long seed and clock negation contribute exactly their low 32 bits.
    const std::uint32_t argumentBits = static_cast<std::uint32_t>(seed);
    const std::uint32_t seedBits = argumentBits ? argumentBits
        : std::uint32_t{0} - static_cast<std::uint32_t>(time(NULL));
    idum = RandomSignedWord(seedBits);
#endif
    if (idum > kSeedNormalizationLimit) {
        idum = -idum;
    } else if (idum > -kSeedNormalizationLimit) {
        idum -= kSmallSeedOffset;
    }
}

int ran1() {
    constexpr int kExtraShuffleIterations = 7;
    static RandomState iy;
    static RandomState iv[NTAB];
    RandomState quotient;
    int index;
    if (idum <= 0 || !iy) {
#if defined(_WIN32)
        if (-idum < 1) {
            idum = 1;
        } else {
            idum = -idum;
        }
#else
        // INT32_MIN negates to the same Win32 word before the signed comparison.
        const RandomState magnitude = RandomSignedWord(std::uint32_t{0} - static_cast<std::uint32_t>(idum));
        idum = magnitude < 1 ? 1 : magnitude;
#endif
        for (index = NTAB + kExtraShuffleIterations; index >= 0; --index) {
            quotient = idum / IQ;
            idum = IA * (idum - quotient * IQ) - IR * quotient;
            if (idum < 0) {
                idum += IM;
            }
            if (index < NTAB) {
                iv[index] = idum;
            }
        }
        iy = iv[0];
    }
    quotient = idum / IQ;
    idum = IA * (idum - quotient * IQ) - IR * quotient;
    if (idum < 0) {
        idum += IM;
    }
    index = iy / NDIV;
    iy = iv[index];
    iv[index] = idum;
    return iy;
}

float fran1() {
    float value = (float)AM * ran1();
    if (value > RNMX) {
        return (float)RNMX;
    }
    return value;
}

float RandomFloat(float low, float high) {
    if (!idum) {
        SeedRandomNumberGenerator();
    }
    return fran1() * (high - low) + low;
}

int RandomLong(int low, int high) {
    if (!idum) {
        SeedRandomNumberGenerator();
    }
#if defined(_WIN32)
    unsigned int range = high - low + 1;
#else
    const std::uint32_t range = static_cast<std::uint32_t>(high) - static_cast<std::uint32_t>(low) + 1;
#endif
    if (!range || range - 1 > MAX_RANDOM_RANGE) {
        return low;
    }
    unsigned int maximum = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE + 1) % range);
    unsigned int value;
    do {
        value = ran1();
    } while (value > maximum);
#if defined(_WIN32)
    return low + value % range;
#else
    return RandomSignedWord(static_cast<std::uint32_t>(low) + value % range);
#endif
}
