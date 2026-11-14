#ifndef CSE_EXTSTATS_MODULE_H
#define CSE_EXTSTATS_MODULE_H
#include "stats_state.h"
#include "amxxmodule.h"
#ifdef snprintf
#undef snprintf
#endif

namespace cse::extstats {
extern ServerState State;
extern enginefuncs_t* HookedEngine;
extern bool Publishing;
extern bool MessageOpen;
extern int HealthId;
extern int ArmorId;
extern int SpecHealthId;
extern int SpecHealth2Id;
extern int HltvId;
extern int ResetHudId;
extern int InitHudId;
extern AMX_NATIVE_INFO Natives[];
Player ReadEntityPlayer(int index);
void ResetLegacyMessage();
} // namespace cse::extstats
#endif
