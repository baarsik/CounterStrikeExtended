#pragma once

// Advertised in client userinfo and acknowledged by the server in physics
// info. Both sides must use the same movement rules for prediction.
#define CSE_MOVEMENT_TIMING_KEY "_cse_pmove"
#define CSE_MOVEMENT_TIMING_VERSION "1"

// An engine-owned prediction callback value, separate from the server's physics info.
#define CSE_REGAMEDLL_MOVEMENT_KEY "_cse_regamedll_pmove"
#define CSE_REGAMEDLL_MOVEMENT_VERSION "1.0"
