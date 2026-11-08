#ifndef CSE_NUMERIC_ENGINE_BOUNDARY_H
#define CSE_NUMERIC_ENGINE_BOUNDARY_H
#include "../../../GoldSrc-SDK/common/legacy_float_to_int.h"
#include "../../../GoldSrc-SDK/common/cvardef.h"
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#if !defined(_WIN32)
#define _snprintf snprintf
#endif
using FileHandle_t = void*;
struct NumericHookTable {
    void (*Cvar_SetValue)(char**, float*);
};
extern NumericHookTable g_engdstAddrs;
void Cvar_Set(const char* name, const char* value);
void Q_strcat(char* target, const char* suffix);
void Con_Printf(const char* format, ...);
int FS_FPrintf(FileHandle_t file, const char* format, ...);
void MSG_WriteBits(unsigned int data, int numBits);
[[noreturn]] void Sys_Error(const char*);
void MSG_WriteBitAngle(float angle, int numBits);
void Cvar_SetValue(char* name, float value);
void Cmd_CvarListPrintCvar(cvar_t* variable, FileHandle_t file);
#endif
