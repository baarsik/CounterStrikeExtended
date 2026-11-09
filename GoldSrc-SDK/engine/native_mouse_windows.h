#pragma once

#include "native_mouse_window.h"
#include <windows.h>

namespace cse {
// Exact SDK function types; injected tables exercise this body without desktop/input calls.
struct NativeMouseWindowsServices {
    decltype(&::GetLastError) lastError;
    decltype(&::SetLastError) setError;
    decltype(&::GetCurrentProcessId) process;
    decltype(&::GetCurrentThreadId) thread;
    decltype(&::IsWindow) valid;
    decltype(&::GetWindowThreadProcessId) windowThread;
    decltype(&::GetThreadDpiAwarenessContext) threadDpi;
    decltype(&::GetWindowDpiAwarenessContext) windowDpi;
    decltype(&::AreDpiAwarenessContextsEqual) dpiEqual;
    decltype(&::GetDpiForWindow) dpiValue;
    decltype(&::GetWindowLongPtrW) style;
    decltype(&::GetAncestor) ancestor;
    decltype(&::GetForegroundWindow) foreground;
    decltype(&::GetFocus) focus;
    decltype(&::IsWindowVisible) visible;
    decltype(&::IsIconic) iconic;
    decltype(&::GetClientRect) clientRect;
    decltype(&::GetCursorPos) cursor;
    decltype(&::ScreenToClient) screenToClient;
};

bool OwnsNativeMouseWindows(const NativeMouseWindowsServices &services, HWND window);
bool FocusesNativeMouseWindows(const NativeMouseWindowsServices &services, HWND window, int width, int height);
bool ReadNativeMouseWindows(const NativeMouseWindowsServices &services, HWND window, int width, int height, float *x,
                            float *y);
} // namespace cse
