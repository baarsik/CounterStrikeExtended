#pragma once

client_state_t cl;
constexpr int LightStyleValueCount = 256; // Runtime lookup table covers every byte-valued style ID.
int d_lightstylevalue[LightStyleValueCount];
mplane_t* lightplane;
vec3_t lightspot;

#include "light_trace.inc"
