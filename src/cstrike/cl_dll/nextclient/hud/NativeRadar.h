#pragma once

#include "interface.h"

// Native client lifecycle. Graphics retirement runs while the owning GL context is current.
void NativeRadar_Init();
void NativeRadar_VidInit();
void NativeRadar_Reset();
void NativeRadar_InitHud();
void NativeRadar_Draw(float time, bool hud_visible);
void NativeRadar_FlushHudText();
bool NativeRadar_IsClassicRequired();
void NativeRadar_ResetGraphics();
void NativeRadar_Shutdown();

void NativeRadar_SetView(const float* origin, const float* angles);

void NativeRadar_SetEngineFactory(CreateInterfaceFn factory);
