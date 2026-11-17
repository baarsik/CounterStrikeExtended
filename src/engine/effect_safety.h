#pragma once

#include "client.h"
#include "../common/r_efx.h"
#include <cmath>

namespace cse {
namespace effects {

inline bool FiniteVector(const float* value) {
    return value && std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

inline bool FiniteEntity(const cl_entity_t* entity) {
    // Attachments are independent effect inputs: a bad attachment must not hide a valid model.
    return entity && FiniteVector(entity->origin) && FiniteVector(entity->angles) &&
        std::isfinite(entity->curstate.scale) && std::isfinite(entity->curstate.frame) &&
        std::isfinite(entity->curstate.framerate);
}

inline bool FiniteTemporary(const TEMPENTITY* entity) {
    return entity && FiniteEntity(&entity->entity) && std::isfinite(entity->die) &&
        FiniteVector(entity->entity.baseline.origin) && FiniteVector(entity->entity.baseline.angles) &&
        (!(entity->flags & FTENT_PLYRATTACHMENT) || FiniteVector(entity->tentOffset));
}

inline bool FiniteParticle(const particle_t* particle) {
    return FiniteVector(particle->org) && FiniteVector(particle->vel) && std::isfinite(particle->die) &&
        std::isfinite(particle->ramp);
}

inline void SanitizeRetiredParticle(particle_t* particle) {
    // Some legacy producers reuse velocity/ramp instead of initializing every field. Repair only
    // corrupt fields after retirement, never positions of live geometry or finite inherited values.
    const auto repair = [](float& value) {
        if (!std::isfinite(value)) {
            value = 0;
        }
    };
    for (auto& value : particle->org) {
        repair(value);
    }
    for (auto& value : particle->vel) {
        repair(value);
    }
    repair(particle->die);
    repair(particle->ramp);
}

template<typename Beam> bool FiniteBeamParameters(const Beam& beam) {
    return std::isfinite(beam.freq) && std::isfinite(beam.die) && std::isfinite(beam.width) &&
        std::isfinite(beam.amplitude) && std::isfinite(beam.speed) && std::isfinite(beam.brightness) &&
        std::isfinite(beam.r) && std::isfinite(beam.g) && std::isfinite(beam.b) &&
        std::isfinite(beam.frame) && std::isfinite(beam.frameRate) && beam.frameCount > 0;
}

inline int BeamSegments(float length, const float* delta, float amplitude, int maximum) {
    if (!FiniteVector(delta) || !std::isfinite(amplitude)) {
        return 0;
    }
    // Preserve normal Length() rounding. Its float squared sum can overflow for finite endpoints;
    // use double only in that case and saturate before converting to int. All beam drawing paths
    // already cap tessellation to NOISE_DIVISIONS * 8 or less.
    double safeLength = length;
    if (!std::isfinite(safeLength)) {
        safeLength = std::sqrt(static_cast<double>(delta[0]) * delta[0] +
            static_cast<double>(delta[1]) * delta[1] + static_cast<double>(delta[2]) * delta[2]);
    }
    constexpr float kLowNoiseThreshold = 0.5f;
    constexpr double kLowNoiseSegmentsPerUnit = 0.075;
    constexpr double kHighNoiseSegmentsPerUnit = 0.25;
    constexpr int kMinimumSegments = 3;
    const double segments = safeLength * (amplitude < kLowNoiseThreshold ?
        kLowNoiseSegmentsPerUnit : kHighNoiseSegmentsPerUnit) + kMinimumSegments;
    return segments >= maximum ? maximum : static_cast<int>(segments);
}

inline void RetireInvalidTemporaryEntities(TEMPENTITY*& active, TEMPENTITY*& free) {
    auto** link = &active;
    while (*link) {
        auto* entity = *link;
        if (FiniteTemporary(entity)) {
            link = &entity->next;
        } else {
            *link = entity->next;
            entity->next = free;
            free = entity;
        }
    }
}

template<typename Value> bool FiniteArgument(Value) {
    return true;
}

inline bool FiniteArgument(float value) {
    return std::isfinite(value);
}

inline bool FiniteArgument(float* value) {
    // Every float-pointer argument in efx_api_t denotes a three-component vector, never an output scalar.
    return FiniteVector(value);
}

inline bool FiniteArgument(cl_entity_t* value) {
    return FiniteEntity(value);
}

inline bool FiniteArgument(TEMPENTITY* value) {
    return FiniteTemporary(value);
}

// Preserve every public signature, calling convention and table slot. Rejected effects have no side
// effects and return the API's usual failure value (void or nullptr); finite inputs pass through unchanged.
template<typename Function, Function function> struct CheckedEffect;

template<typename Result, typename... Arguments, Result (*function)(Arguments...)>
struct CheckedEffect<Result (*)(Arguments...), function> {
    static Result Invoke(Arguments... arguments) {
        bool finite = true;
        const int checked[] = {0, (finite = finite && FiniteArgument(arguments), 0)...};
        (void)checked;
        if (!finite) {
            return Result();
        }
        return function(arguments...);
    }
};

}
}
