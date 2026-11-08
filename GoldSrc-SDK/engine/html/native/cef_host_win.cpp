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
