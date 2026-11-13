#pragma once

#include "browser_controller.h"

namespace cse::browser {
// The paths supplied here are UTF-8. The trusted host path is relative to SDL_GetBasePath(), whose
// macOS bundle-directory meaning is packaging configuration, not an assumed MacOS/Resources layout.
bool ResolveEngineControllerPaths(const std::string& baseDirectory, const std::string& preferenceDirectory,
    const std::string& hostRelativePath, ControllerConfig& config, std::string& error);

bool GetEngineControllerConfiguration(ControllerConfig& config, std::string& error);

// Expose a controller only after its provider completes the versioned handshake. Explicit configurations
// also support isolated component fixtures; production configuration never supplies a fixture Keychain ID.
std::unique_ptr<IHTMLChromeController> CreateInitializedController(const ControllerConfig& config, std::string& error);
std::unique_ptr<IHTMLChromeController> CreateEngineController(std::string& error);
}
