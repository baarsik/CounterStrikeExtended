#include "native_mouse_windows.h"
#include <cstdint>

namespace cse {
namespace {
bool Complete(const NativeMouseWindowsServices &s) {
    return s.lastError && s.setError && s.process && s.thread && s.valid && s.windowThread && s.threadDpi &&
           s.windowDpi && s.dpiEqual && s.dpiValue && s.style && s.ancestor && s.foreground && s.focus && s.visible &&
           s.iconic && s.clientRect && s.cursor && s.screenToClient;
}

class PreserveLastError {
    public:
    explicit PreserveLastError(const NativeMouseWindowsServices &services)
        : services(services), saved(services.lastError()) {
    }
    ~PreserveLastError() {
        services.setError(saved);
    }

    private:
    const NativeMouseWindowsServices &services;
    DWORD saved;
};

bool Owned(const NativeMouseWindowsServices &s, HWND window, DPI_AWARENESS_CONTEXT &context) {
    if (!window || !s.valid(window)) {
        return false;
    }
    DWORD process = 0;
    const DWORD thread = s.windowThread(window, &process);
    if (!thread || thread != s.thread() || !process || process != s.process()) {
        return false;
    }
    context = s.threadDpi();
    const DPI_AWARENESS_CONTEXT windowContext = s.windowDpi(window);
    if (!context || !windowContext || !s.dpiEqual(context, windowContext) || !s.dpiValue(window)) {
        return false;
    }
    s.setError(ERROR_SUCCESS);
    const LONG_PTR style = s.style(window, GWL_EXSTYLE);
    if ((!style && s.lastError() != ERROR_SUCCESS) || (style & WS_EX_LAYOUTRTL) != 0) {
        return false;
    }
    return s.ancestor(window, GA_ROOT) == window;
}

bool Focused(const NativeMouseWindowsServices &s, HWND window, int width, int height, DPI_AWARENESS_CONTEXT &context) {
    if (width <= 0 || height <= 0 || !Owned(s, window, context) || s.foreground() != window || !s.visible(window) ||
        s.iconic(window)) {
        return false;
    }
    const HWND focus = s.focus();
    if (!focus || s.ancestor(focus, GA_ROOT) != window) {
        return false;
    }
    RECT rect{};
    if (!s.clientRect(window, &rect) || rect.left != 0 || rect.top != 0) {
        return false;
    }
    // Compare in the caller's unchanged DPI context; never invent a DPI multiplier.
    return static_cast<std::int64_t>(rect.right) - rect.left == width &&
           static_cast<std::int64_t>(rect.bottom) - rect.top == height;
}

NativeMouseWindowsServices DefaultServices() {
    return {GetLastError,
            SetLastError,
            GetCurrentProcessId,
            GetCurrentThreadId,
            IsWindow,
            GetWindowThreadProcessId,
            GetThreadDpiAwarenessContext,
            GetWindowDpiAwarenessContext,
            AreDpiAwarenessContextsEqual,
            GetDpiForWindow,
            GetWindowLongPtrW,
            GetAncestor,
            GetForegroundWindow,
            GetFocus,
            IsWindowVisible,
            IsIconic,
            GetClientRect,
            GetCursorPos,
            ScreenToClient};
}

bool DefaultOwned(void *window) {
    return OwnsNativeMouseWindows(DefaultServices(), static_cast<HWND>(window));
}
bool DefaultFocused(void *window, int width, int height) {
    return FocusesNativeMouseWindows(DefaultServices(), static_cast<HWND>(window), width, height);
}
bool DefaultAbsolute(void *window, int width, int height, float *x, float *y) {
    return ReadNativeMouseWindows(DefaultServices(), static_cast<HWND>(window), width, height, x, y);
}
} // namespace

bool OwnsNativeMouseWindows(const NativeMouseWindowsServices &s, HWND window) {
    if (!Complete(s)) {
        return false;
    }
    PreserveLastError error(s);
    DPI_AWARENESS_CONTEXT context = nullptr;
    return Owned(s, window, context);
}

bool FocusesNativeMouseWindows(const NativeMouseWindowsServices &s, HWND window, int width, int height) {
    if (!Complete(s)) {
        return false;
    }
    PreserveLastError error(s);
    DPI_AWARENESS_CONTEXT context = nullptr;
    return Focused(s, window, width, height, context);
}

bool ReadNativeMouseWindows(const NativeMouseWindowsServices &s, HWND window, int width, int height, float *x,
                            float *y) {
    if (!x || !y || !Complete(s)) {
        return false;
    }
    PreserveLastError error(s);
    DPI_AWARENESS_CONTEXT context = nullptr;
    if (!Focused(s, window, width, height, context)) {
        return false;
    }
    POINT point{};
    if (!s.cursor(&point) || !s.screenToClient(window, &point) || !s.dpiEqual(context, s.threadDpi())) {
        return false;
    }
    const float resultX = static_cast<float>(point.x);
    const float resultY = static_cast<float>(point.y);
    *x = resultX;
    *y = resultY;
    return true;
}

NativeMousePlatform NativeMouseDefaultPlatform() {
    return {SDL_PROP_WINDOW_WIN32_HWND_POINTER, DefaultOwned, DefaultFocused, DefaultAbsolute};
}
} // namespace cse
