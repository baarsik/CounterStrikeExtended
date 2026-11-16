#pragma once

#include "hud/radar/map/layers/RadarMapLayers.h"

enum class RadarMapMode;

// Keep the top-image provider authoritative: a pending layered analysis cannot
// make a pending top image drawable, while an existing cached image remains usable.
template <class Backdrop>
RadarMapStatus RadarMapReadiness_PrepareLayered(Backdrop& backdrop, RadarMapMode mode, const RadarMapLayers* layers,
                                                RadarMapStatus layout_status, bool use_fallback, bool prepare_only) {
    backdrop.EnsureMap(mode, layers, true);
    if (layout_status != RadarMapStatus::Ready || use_fallback) {
        return backdrop.EnsureTopMap(mode, prepare_only, layers);
    }
    return RadarMapStatus::Ready;
}
