// The bootstrap entry point follows CEF's BSD-licensed cefsimple example.
#include <windows.h>

#include "probe.h"
#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_version_info.h"

CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE instance, LPWSTR, int, void* sandbox,
    cef_version_info_t*) {
    if (!sandbox) {
        return 2;
    }
    CefMainArgs arguments(instance);
    const int childResult = CefExecuteProcess(arguments, nullptr, sandbox);
    if (childResult >= 0) {
        return childResult;
    }
    CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine();
    commandLine->InitFromString(::GetCommandLineW());
    return RunCefProbe(arguments, commandLine, sandbox);
}
