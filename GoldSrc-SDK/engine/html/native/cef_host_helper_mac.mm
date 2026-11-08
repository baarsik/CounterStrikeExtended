// Adapted from CEF's BSD-licensed cefsimple/process_helper_mac.cc; see tests/native/cef_host/NOTICE.
#include "cef_host.h"
#include "include/cef_sandbox_mac.h"
#include "include/wrapper/cef_library_loader.h"

int main(int argc, char* argv[]) {
    CefScopedSandboxContext sandbox;
    if (!sandbox.Initialize(argc, argv)) {
        return 1;
    }
    CefScopedLibraryLoader loader;
    if (!loader.LoadInHelper()) {
        return 1;
    }
    return CefExecuteProcess(CefMainArgs(argc, argv), CreateCefHostApp(), nullptr);
}
