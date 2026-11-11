#include "ServerBrowserModel.h"
#include "steam_provider.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

// Headless actual-model probe. No SDL initialization, preferences, favorites/history writes, or connection.
// Controller supplies the qualified provider image and owns execution against live Steam.
int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: provider_probe provider-image app-id seconds\n");
        return 2;
    }
    const auto app = static_cast<AppId_t>(std::strtoul(argv[2], nullptr, 10));
    const int seconds = std::atoi(argv[3]);
    constexpr int MaximumProbeSeconds = 60;
    if ((app != 10 && app != 70) || seconds <= 0 || seconds > MaximumProbeSeconds) {
        return 2;
    }
    cse_steam::Provider provider;
    if (!provider.Open(argv[1]) || !provider.Functions().initialize()) {
        std::fprintf(stderr, "provider initialization failed: %s\n", provider.LastError());
        return 3;
    }
    const auto& api = provider.Functions();
    auto* servers = static_cast<ISteamMatchmakingServers*>(api.userInterface(api.getUser(),
        STEAMMATCHMAKINGSERVERS_INTERFACE_VERSION));
    int result = 0;
    {
        cse::server_browser::ServerBrowserModel model({servers, nullptr});
        cse::server_browser::Filters filters;
        filters.app = app;
        if (!model.Refresh(cse::server_browser::ListKind::Internet, filters)) {
            std::fprintf(stderr, "request failed: %s\n", model.GetStatus().error.c_str());
            result = 4;
        } else {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
            size_t updates = 0;
            while (std::chrono::steady_clock::now() < deadline) {
                api.runCallbacks();
                model.Pump();
                updates += model.TakeUpdates().size();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const auto status = model.GetStatus();
            std::printf("app=%u rows=%zu updates=%zu failed=%zu pending=%zu phase=%d\n", app,
                status.rows, updates, status.failed, status.pending, static_cast<int>(status.phase));
        }
        if (!model.BeginClose() || !model.IsQuiescent()) {
            std::fputs("model retirement refused\n", stderr);
            std::abort();
        }
        std::puts("model_cancel_release_quiescent=1");
        // A small post-release pump window exercises the actual provider's retirement guarantee.
        for (int iteration = 0; iteration < 10; ++iteration) {
            api.runCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    api.shutdown();
    std::puts("steam_shutdown=1");
    return result;
}
