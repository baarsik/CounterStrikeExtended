#pragma once

#include "include/cef_app.h"
#include "include/cef_command_line.h"

// KeyDown/KeyUp carry Windows virtual keys on every platform. Modifier bit8 preserves keypad location;
// KeyChar carries Unicode scalars or UTF-16 units. The protocol never transports SDL keycodes or pointers.
CefRefPtr<CefApp> CreateCefHostApp();
int RunCefHost(const CefMainArgs& arguments, CefRefPtr<CefCommandLine> commandLine, void* sandbox);
void CloseCefHost();

#if defined(__APPLE__)
bool PrepareCefHostMacKey(CefKeyEvent& event);
#endif

void SetCefHostNativeTitle(CefWindowHandle window, const CefString& title);
bool IsCefHostNativeWindow(CefWindowHandle window);
