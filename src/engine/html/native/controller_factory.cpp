#include "controller_factory.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <filesystem>

#if !defined(CSE_BROWSER_HOST_RELATIVE_PATH)
#error Configure CSE_BROWSER_HOST_RELATIVE_PATH relative to SDL_GetBasePath for the staged native application
#endif
#if !defined(CSE_BROWSER_DEBUG_PROFILE)
#error Configure CSE_BROWSER_DEBUG_PROFILE to select the configuration-specific writable browser profile
#endif

namespace cse::browser {
namespace {
constexpr char PreferenceOrganization[] = "CounterStrikeExtended";
#if CSE_BROWSER_DEBUG_PROFILE
constexpr char PreferenceApplication[] = "CounterStrikeExtended Debug";
#else
constexpr char PreferenceApplication[] = "CounterStrikeExtended";
#endif
constexpr char BrowserProfileDirectory[] = "browser";
constexpr char ConfiguredHostRelativePath[] = CSE_BROWSER_HOST_RELATIVE_PATH;

struct SdlPathDeleter {
    void operator()(char* path) const {
        SDL_free(path);
    }
};
}

bool ResolveEngineControllerPaths(const std::string& baseDirectory, const std::string& preferenceDirectory,
    const std::string& hostRelativePath, ControllerConfig& config, std::string& error) {
    config = {};
    error.clear();
    const auto base = std::filesystem::u8path(baseDirectory);
    const auto preferences = std::filesystem::u8path(preferenceDirectory);
    const auto host = std::filesystem::u8path(hostRelativePath);
    if (base.empty() || preferences.empty() || host.empty() || !base.is_absolute() ||
        !preferences.is_absolute() || host.is_absolute() || host.has_root_name() || host.has_root_directory()) {
        error = "Native browser paths require absolute application/preferences roots and a configured relative host";
        return false;
    }
    config.executable = (base / host).lexically_normal().u8string();
    config.profileDirectory = (preferences / BrowserProfileDirectory).lexically_normal().u8string();
    return true;
}

bool GetEngineControllerConfiguration(ControllerConfig& config, std::string& error) {
    config = {};
    error.clear();
    // SDL3 owns and caches this const pointer. SDL_GetPrefPath instead returns an allocation to free.
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        error = "Could not resolve the native application directory: " + std::string(SDL_GetError());
        return false;
    }
    const std::unique_ptr<char, SdlPathDeleter> preferences(SDL_GetPrefPath(PreferenceOrganization,
        PreferenceApplication));
    if (!preferences) {
        error = "Could not resolve writable native preferences: " + std::string(SDL_GetError());
        return false;
    }
    return ResolveEngineControllerPaths(base, preferences.get(), ConfiguredHostRelativePath, config, error);
}

std::unique_ptr<IHTMLChromeController> CreateInitializedController(const ControllerConfig& config,
    std::string& error) {
    error.clear();
    if (!std::filesystem::u8path(config.executable).is_absolute() ||
        !std::filesystem::u8path(config.profileDirectory).is_absolute()) {
        error = "Native browser executable and writable profile must be absolute paths";
        return {};
    }
    SDL_PathInfo host{};
    if (!SDL_GetPathInfo(config.executable.c_str(), &host) || host.type != SDL_PATHTYPE_FILE) {
        error = "The staged native browser host is unavailable: " + config.executable;
        return {};
    }
    if (!SDL_CreateDirectory(config.profileDirectory.c_str())) {
        error = "Could not create the native browser profile: " + std::string(SDL_GetError());
        return {};
    }
    auto controller = CreateController(config);
    // The legacy htmlcache/htmlcookies filenames are not profile locations in the owned provider.
    if (!controller->Init(nullptr, nullptr)) {
        error = "The owned browser provider did not complete initialization: " + config.executable;
        return {};
    }
    return controller;
}

std::unique_ptr<IHTMLChromeController> CreateEngineController(std::string& error) {
    ControllerConfig config;
    if (!GetEngineControllerConfiguration(config, error)) {
        return {};
    }
    return CreateInitializedController(config, error);
}
}
