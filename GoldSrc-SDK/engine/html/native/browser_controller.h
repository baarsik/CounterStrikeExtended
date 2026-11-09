#pragma once

#include "../ichromehtmlwrapper.h"
#include <memory>
#include <string>

namespace cse::browser {
struct ControllerDiagnostics {
    bool cleanShutdown = false;
    int exitCode = 0;
};

struct ControllerConfig {
    std::string executable;
    std::string profileDirectory;
    std::shared_ptr<ControllerDiagnostics> diagnostics = {};
    // Explicitly isolated developer fixtures use a separate encrypted Keychain item.
    std::string fixtureKeychainId = {};
};

// The controller owns serializers and the provider process. RemoveBrowser detaches callbacks synchronously.
std::unique_ptr<IHTMLChromeController> CreateController(const ControllerConfig& config);

// Only for instances returned by CreateController. Notify and detach live widget callbacks once,
// retaining serializers and the provider while the owner drains deferred widget deletion. Call Shutdown
// after that drain; widgets must have called RemoveBrowser before the controller itself is destroyed.
void BeginControllerShutdown(IHTMLChromeController& controller);
}
