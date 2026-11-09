#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

void PreparePresentationGuardProcess();
void InvokeUnusedPresentationBinding(const char* name);
