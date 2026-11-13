// The bootstrap entry point follows CEF's BSD-licensed cefsimple example.
#include <windows.h>

#include "cef_host.h"
#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_version_info.h"

CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE instance, LPWSTR, int, void* sandbox,
    cef_version_info_t*) {
    if (!sandbox) {
        return 2;
    }
    CefMainArgs arguments(instance);
    const int childResult = CefExecuteProcess(arguments, CreateCefHostApp(), sandbox);
    if (childResult >= 0) {
        return childResult;
    }
    CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine();
    commandLine->InitFromString(::GetCommandLineW());
    return RunCefHost(arguments, commandLine, sandbox);
}

void SetCefHostNativeTitle(CefWindowHandle window, const CefString& title) {
    SetWindowTextW(window, title.ToWString().c_str());
}

bool IsCefHostNativeWindow(CefWindowHandle window) {
    return window != nullptr && IsWindow(window) != FALSE;
}

namespace {
class WindowsParent final : public CefHostNativeParent {
public:
    WindowsParent() {
        constexpr int DialogParentWidth = 800;
        constexpr int DialogParentHeight = 600;
        // A hidden real top-level HWND supplies CEF's native dialog owner without a second browser view.
        // The built-in class does not post WM_QUIT when this owned parent is destroyed.
        window_ = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"CounterStrikeExtended Browser Dialogs",
            WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, DialogParentWidth, DialogParentHeight,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    }

    ~WindowsParent() override {
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
    }

    CefWindowHandle Handle() const override {
        return window_;
    }

private:
    HWND window_ = nullptr;
};
}

std::unique_ptr<CefHostNativeParent> CreateCefHostNativeParent() {
    return std::make_unique<WindowsParent>();
}
