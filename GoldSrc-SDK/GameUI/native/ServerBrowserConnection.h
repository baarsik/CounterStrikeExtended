#pragma once

#include "ServerBrowserModel.h"
#include <array>

namespace cse::server_browser {
// The engine userinfo value limit is MAX_INFO_VALUE - 1 (engine/info.h).
constexpr size_t ServerPasswordCapacity = 128;
enum class ConnectResult { Started, InvalidServer, OtherGame, InvalidPassword };
class IConnectionEngine {
public:
    virtual ~IConnectionEngine() = default;
    virtual const char *Password() = 0;
    virtual void SetPassword(const char *value) = 0;
    virtual void Connect(const char *numericCommand) = 0;
};
class ServerBrowserConnection final {
public:
    ServerBrowserConnection(IConnectionEngine &engine, ServerBrowserModel &model, AppId_t app, std::string game);
    ~ServerBrowserConnection();
    ConnectResult Begin(const Row &row, std::string_view password);
    bool CanConnect(const Row &row) const;
    void Accepted(const char *game, uint32_t rawIP, uint16_t connectionPort, uint32_t unixTime);
    void Disconnected();
    std::optional<FavoriteRecord> CurrentServer() const;
    uint64_t AcceptanceSerial() const {
        return acceptanceSerial_;
    }

private:
    void Retire();
    IConnectionEngine &engine_;
    ServerBrowserModel &model_;
    AppId_t app_;
    std::string game_;
    std::optional<Address> pending_;
    std::optional<FavoriteRecord> current_;
    uint64_t acceptanceSerial_ = 0;
    std::array<char, ServerPasswordCapacity> password_{};
    std::string previous_;
};
}
