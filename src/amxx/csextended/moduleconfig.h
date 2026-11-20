#ifndef CSEXTENDED_MODULECONFIG_H
#define CSEXTENDED_MODULECONFIG_H

#define MODULE_NAME "csextended"
#define MODULE_VERSION "202611.2"
#define MODULE_AUTHOR "baarsik"
#define MODULE_URL "https://github.com/baarsik/CounterStrikeExtended"
#define MODULE_LOGTAG "CSEXTENDED"
#define MODULE_LIBRARY "csextended"
#define MODULE_LIBCLASS ""
#define MODULE_DATE "2026-11-19"
#define USE_METAMOD
#define NO_ALLOC_OVERRIDES
#define FN_AMXX_ATTACH CseAttach
#define FN_AMXX_CHECKGAME CseCheckGame
#define FN_AMXX_PLUGINSLOADED FormsPluginsLoaded
#define FN_AMXX_PLUGINSUNLOADING FormsPluginsUnloading
#define FN_ServerActivate CseServerActivate
#define FN_ServerActivate_Post CseServerActivated
#define FN_ServerDeactivate CseServerDeactivate
#define FN_StartFrame_Post CseStartFrame
#define FN_ClientPutInServer_Post CseClientEnter
#define FN_ClientDisconnect CseClientLeave
#define FN_ClientConnect FormsClientConnect
#define FN_ClientCommand FormsClientCommand
#define FN_CvarValue2 StatsCvarReply
#define FN_MessageBegin StatsMessageBegin
#define FN_MessageEnd_Post StatsMessageEnd
#define FN_WriteByte StatsWriteByte
#define FN_WriteShort StatsWriteShort

#endif
