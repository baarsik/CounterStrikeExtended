#pragma once
#include <cstdlib>
extern "C" void CseObjectResetFree(void* pointer);
#define free CseObjectResetFree
