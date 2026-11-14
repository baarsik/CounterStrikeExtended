#ifndef CSE_EXTSTATS_MODULECONFIG_H
#define CSE_EXTSTATS_MODULECONFIG_H
#define MODULE_NAME "CSE Extended Stats"
#define MODULE_VERSION "202611.1"
#define MODULE_AUTHOR "CounterStrikeExtended"
#define MODULE_URL "https://github.com/baarsik/CounterStrikeExtended"
#define MODULE_LOGTAG "CSESTATS"
#define MODULE_LIBRARY "cse_extstats"
#define MODULE_LIBCLASS ""
#define MODULE_DATE "2026-11-14"
#define USE_METAMOD
#define NO_ALLOC_OVERRIDES
#define FN_AMXX_ATTACH StatsAttach
#define FN_AMXX_CHECKGAME StatsCheckGame
#define FN_ServerActivate StatsServerActivate
#define FN_ServerActivate_Post StatsServerActivated
#define FN_ServerDeactivate StatsServerDeactivate
#define FN_StartFrame_Post StatsStartFrame
#define FN_ClientPutInServer_Post StatsClientEnter
#define FN_ClientDisconnect StatsClientLeave
#define FN_CvarValue2 StatsCvarReply
#define FN_MessageBegin StatsMessageBegin
#define FN_MessageEnd_Post StatsMessageEnd
#define FN_WriteByte StatsWriteByte
#define FN_WriteShort StatsWriteShort
#endif
