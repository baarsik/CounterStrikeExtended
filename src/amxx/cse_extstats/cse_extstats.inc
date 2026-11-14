#if defined _cse_extstats_included
    #endinput
#endif
#define _cse_extstats_included
#pragma reqlib cse_extstats
#if !defined AMXMODX_NOAUTOLOAD
    #pragma loadlib cse_extstats
#endif

// Float-based game storage is preserved. Nonpositive health invokes normal player death.
native cse_set_user_health(index, health);
// Changes the amount only; use the CStrike equipment API to grant/change kevlar or helmets.
native cse_set_user_armor(index, armor);
// Publish actual entity values after another API changed them, including in manual mode.
native cse_sync_user_stats(index);
// Reports CSE identity after detection; does not promise a particular extension version.
native bool:cse_is_supported(index);
