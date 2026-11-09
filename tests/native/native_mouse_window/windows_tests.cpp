#include "native_mouse_windows.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>

namespace {
int checks = 0;
void Check(bool value, const char *text, int line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "windows-sample-check:%d:%s\n", line, text);
        std::exit(17);
    }
}
#define CHECK(value) Check((value), #value, __LINE__)
HWND const window = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(0x1234567890ULL));
HWND const child = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(0x2345678910ULL));
HWND const foreign = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(0x3456789010ULL));
DPI_AWARENESS_CONTEXT const dpi = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
struct Scene {
    DWORD error = 91;
    DWORD pid = 23;
    DWORD windowPid = 23;
    DWORD thread = 29;
    DWORD windowThread = 29;
    bool valid = true;
    bool cursorSucceeds = true;
    bool convertSucceeds = true;
    bool rectSucceeds = true;
    bool visible = true;
    bool iconic = false;
    bool styleFails = false;
    LONG_PTR style = 0;
    RECT rect{0, 0, 640, 480};
    POINT screen{860, 564};
    LONG offsetX = 500;
    LONG offsetY = 300;
    HWND foreground = window;
    HWND focus = window;
    HWND root = window;
    DPI_AWARENESS_CONTEXT threadDpi = dpi;
    DPI_AWARENESS_CONTEXT windowDpi = dpi;
    bool changeDpiDuringRead = false;
    int cursorReads = 0;
    int conversions = 0;
    int extentReads = 0;
};
Scene scene;
DWORD WINAPI LastError() {
    return scene.error;
}
void WINAPI SetError(DWORD value) {
    scene.error = value;
}
DWORD WINAPI Process() {
    return scene.pid;
}
DWORD WINAPI Thread() {
    return scene.thread;
}
BOOL WINAPI IsValid(HWND value) {
    CHECK(value == window);
    return scene.valid;
}
DWORD WINAPI WindowThread(HWND value, LPDWORD pid) {
    CHECK(value == window);
    *pid = scene.windowPid;
    return scene.windowThread;
}
DPI_AWARENESS_CONTEXT WINAPI ThreadDpi() {
    return scene.threadDpi;
}
DPI_AWARENESS_CONTEXT WINAPI WindowDpi(HWND value) {
    CHECK(value == window);
    return scene.windowDpi;
}
BOOL WINAPI DpiEqual(DPI_AWARENESS_CONTEXT a, DPI_AWARENESS_CONTEXT b) {
    return a == b;
}
UINT WINAPI WindowDpiValue(HWND value) {
    CHECK(value == window);
    return 192;
}
LONG_PTR WINAPI Style(HWND value, int index) {
    CHECK(value == window && index == GWL_EXSTYLE);
    if (scene.styleFails) {
        scene.error = ERROR_INVALID_WINDOW_HANDLE;
    }
    return scene.style;
}
HWND WINAPI Ancestor(HWND value, UINT flags) {
    CHECK((value == window || value == child || value == foreign) && flags == GA_ROOT);
    return scene.root;
}
HWND WINAPI Foreground() {
    return scene.foreground;
}
HWND WINAPI Focus() {
    return scene.focus;
}
BOOL WINAPI Visible(HWND value) {
    CHECK(value == window);
    return scene.visible;
}
BOOL WINAPI Iconic(HWND value) {
    CHECK(value == window);
    return scene.iconic;
}
BOOL WINAPI Rect(HWND value, LPRECT out) {
    CHECK(value == window);
    ++scene.extentReads;
    *out = scene.rect;
    return scene.rectSucceeds;
}
BOOL WINAPI Cursor(LPPOINT point) {
    ++scene.cursorReads;
    *point = scene.screen;
    scene.error = 777;
    if (scene.changeDpiDuringRead) {
        scene.threadDpi = DPI_AWARENESS_CONTEXT_UNAWARE;
    }
    return scene.cursorSucceeds;
}
BOOL WINAPI Convert(HWND value, LPPOINT point) {
    CHECK(value == window);
    ++scene.conversions;
    point->x -= scene.offsetX;
    point->y -= scene.offsetY;
    return scene.convertSucceeds;
}
cse::NativeMouseWindowsServices Services() {
    return {LastError, SetError, Process,        Thread, IsValid,  WindowThread, ThreadDpi,
            WindowDpi, DpiEqual, WindowDpiValue, Style,  Ancestor, Foreground,   Focus,
            Visible,   Iconic,   Rect,           Cursor, Convert};
}
void Fail() {
    float x = 123.5f;
    float y = 456.5f;
    CHECK(!cse::ReadNativeMouseWindows(Services(), window, 640, 480, &x, &y));
    CHECK(x == 123.5f && y == 456.5f);
    CHECK(scene.error == 91);
}
} // namespace

int main() {
    if (sizeof(void *) == sizeof(std::uint64_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(window) > 0xffffffffULL);
    }
    const auto services = Services();
    CHECK(cse::OwnsNativeMouseWindows(services, window));
    CHECK(scene.cursorReads == 0 && scene.error == 91);
    float x = 0.0f;
    float y = 0.0f;
    CHECK(cse::ReadNativeMouseWindows(services, window, 640, 480, &x, &y));
    CHECK(x == 360.0f && y == 264.0f && scene.cursorReads == 1 && scene.conversions == 1);
    CHECK(scene.error == 91 && scene.extentReads == 1);
    scene = Scene{};
    scene.screen = {-100, 900};
    scene.focus = child;
    CHECK(cse::ReadNativeMouseWindows(services, window, 640, 480, &x, &y));
    CHECK(x == -600.0f && y == 600.0f);
    CHECK(cse::FocusesNativeMouseWindows(services, window, 640, 480));
    CHECK(scene.cursorReads == 1); // Relative validation must not sample absolute OS position.
    scene = Scene{};
    scene.cursorSucceeds = false;
    Fail();
    CHECK(scene.conversions == 0);
    scene = Scene{};
    scene.convertSucceeds = false;
    Fail();
    scene = Scene{};
    scene.valid = false;
    Fail();
    scene = Scene{};
    scene.windowPid = 24;
    Fail();
    scene = Scene{};
    scene.windowThread = 30;
    Fail();
    scene = Scene{};
    scene.threadDpi = nullptr;
    Fail();
    scene = Scene{};
    scene.windowDpi = DPI_AWARENESS_CONTEXT_UNAWARE;
    Fail();
    scene = Scene{};
    scene.changeDpiDuringRead = true;
    Fail();
    scene = Scene{};
    scene.style = WS_EX_LAYOUTRTL;
    Fail();
    scene = Scene{};
    scene.styleFails = true;
    Fail();
    scene = Scene{};
    scene.foreground = foreign;
    Fail();
    scene = Scene{};
    scene.focus = nullptr;
    Fail();
    scene = Scene{};
    scene.root = foreign;
    Fail();
    scene = Scene{};
    scene.visible = false;
    Fail();
    scene = Scene{};
    scene.iconic = true;
    Fail();
    scene = Scene{};
    scene.rectSucceeds = false;
    Fail();
    scene = Scene{};
    scene.rect.right = 1280;
    Fail();
    scene = Scene{};
    scene.rect.left = -1;
    Fail();
    scene = Scene{};
    CHECK(!cse::ReadNativeMouseWindows(services, window, 0, 480, &x, &y));
    CHECK(!cse::ReadNativeMouseWindows(services, window, 640, 480, nullptr, &y));
    CHECK(!cse::ReadNativeMouseWindows(services, nullptr, 640, 480, &x, &y));
    CHECK(scene.cursorReads == 0);
    std::printf("windows-sample checks=%d\n", checks);
    return 0;
}
