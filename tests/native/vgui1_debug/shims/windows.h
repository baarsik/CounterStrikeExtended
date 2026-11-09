#ifndef CSE_VGUI1_DEBUG_WINDOWS_TRANSPORT_SHIM_H
#define CSE_VGUI1_DEBUG_WINDOWS_TRANSPORT_SHIM_H
// Exact retained Windows Debug bodies, using the same native formatter in this control.
// This only replaces OutputDebugStringA transport; it is not a Windows CRT provider.
void OutputDebugStringA(const char* text);
#endif
