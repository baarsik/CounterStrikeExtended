// Cocoa event scoping and dynamic loading follow CEF's BSD-licensed cefsimple example.
#import <Cocoa/Cocoa.h>

#include "probe.h"
#include "include/cef_application_mac.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_library_loader.h"

@interface CseProbeApplication : NSApplication <CefAppProtocol> {
    BOOL handlingSendEvent_;
}
@end

@implementation CseProbeApplication
- (BOOL)isHandlingSendEvent {
    return handlingSendEvent_;
}
- (void)setHandlingSendEvent:(BOOL)value {
    handlingSendEvent_ = value;
}
- (void)sendEvent:(NSEvent*)event {
    CefScopedSendingEvent eventScope;
    [super sendEvent:event];
}
- (void)terminate:(id)sender {
    CloseCefProbe();
}
@end

int main(int argc, char* argv[]) {
    CefScopedLibraryLoader loader;
    if (!loader.LoadInMain()) {
        return 1;
    }
    @autoreleasepool {
        [CseProbeApplication sharedApplication];
        if (![NSApp isKindOfClass:[CseProbeApplication class]]) {
            return 1;
        }
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine();
        commandLine->InitFromArgv(argc, argv);
        return RunCefProbe(CefMainArgs(argc, argv), commandLine, nullptr);
    }
}
