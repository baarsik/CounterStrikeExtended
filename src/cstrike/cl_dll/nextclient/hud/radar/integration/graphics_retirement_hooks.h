#pragma once

#include <cstdint>

namespace radar::integration {
// Object names may be reused by a replacement context, even when SDL reuses its
// context/window addresses. A missing service therefore permits abandonment only.
constexpr bool CanReleaseGraphics(std::uint64_t owned_generation, std::uint64_t current_generation,
                                  bool identity_matches) {
    return identity_matches && owned_generation != 0 && owned_generation == current_generation;
}
} // namespace radar::integration
