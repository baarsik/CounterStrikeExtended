#include "model_fixture.inc"
#include "ServerBrowserConnection.h"
#include "check.h"
#include <cstring>

namespace {
constexpr AppId_t CurrentApp = 10;
constexpr Address SelectedAddress{0x7f000001U, 27015, 27016};
struct Engine final : IConnectionEngine {
    std::string password = "previous";
    std::vector<std::string> commands;
    const char *Password() override {
        return password.c_str();
    }
    void SetPassword(const char *value) override {
        password = value;
    }
    void Connect(const char *command) override {
        commands.emplace_back(command);
    }
};
Row Selected() {
    Row row;
    row.key = SelectedAddress;
    row.app = CurrentApp;
    row.gameDirectory = "cstrike";
    row.name = "evil\nquit; password hidden";
    row.responded = true;
    return row;
}
uint32_t RawLoopback() {
    const unsigned char bytes[]{127, 0, 0, 1};
    uint32_t raw = 0;
    std::memcpy(&raw, bytes, sizeof(raw));
    return raw;
}
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        Favorites favorites;
        ServerBrowserModel model({nullptr, &favorites});
        Engine engine;
        ServerBrowserConnection connection(engine, model, CurrentApp, "cstrike");
        Row row = Selected();
        if (name == "accepted-distinct-port") {
            Require(connection.Begin(row, "secret") == ConnectResult::Started, "valid connect rejected");
            Require(favorites.records.empty(), "connect attempt wrote history");
            Require(engine.commands == std::vector<std::string>{"connect 127.0.0.1:27015\n"},
                    "connect command contains remote display text, password, or query port");
            Require(engine.password == "secret", "direct password assignment missing");
            connection.Accepted("cstrike", RawLoopback(), SelectedAddress.connectionPort, 123456);
            Require(favorites.records.size() == 1, "accepted connection missing history");
            Require(favorites.records.front().address == SelectedAddress, "history lost distinct query port");
            Require(favorites.records.front().app == CurrentApp, "history borrowed mutable browse app");
            Require(engine.password == "previous", "accepted connection failed to restore owned cvar");
        } else if (name == "unrelated-acceptance") {
            connection.Accepted("cstrike", RawLoopback(), SelectedAddress.connectionPort, 123456);
            Require(favorites.records.empty(), "initial replay wrote history without pending request");
            Require(connection.Begin(row, "secret") == ConnectResult::Started, "valid connect rejected");
            connection.Accepted("cstrike", RawLoopback(), SelectedAddress.queryPort, 123456);
            Require(favorites.records.empty(), "unmatched connection wrote selected-server history");
            Require(engine.password == "previous", "unmatched terminal connection retained secret");
        } else if (name == "intervening-change") {
            Require(connection.Begin(row, "secret") == ConnectResult::Started, "valid connect rejected");
            engine.password = "user changed it";
            connection.Disconnected();
            Require(engine.password == "user changed it", "disconnect clobbered intervening cvar change");
        } else if (name == "supersession") {
            Require(connection.Begin(row, "first") == ConnectResult::Started, "first request rejected");
            Require(connection.Begin(row, "second") == ConnectResult::Started, "replacement rejected");
            connection.Disconnected();
            Require(engine.password == "previous", "supersession restored superseded secret");
            Require(favorites.records.empty(), "disconnect wrote history");
        } else if (name == "unicode-password") {
            Require(connection.Begin(row, u8"zażółć") == ConnectResult::Started, "valid UTF-8 password was rejected");
            Require(engine.password == u8"zażółć", "UTF-8 password changed during direct assignment");
            connection.Disconnected();
            const char malformed[]{static_cast<char>(0xc0), static_cast<char>(0xaf)};
            Require(connection.Begin(row, std::string_view(malformed, sizeof(malformed))) ==
                        ConnectResult::InvalidPassword,
                    "overlong UTF-8 secret accepted");
        } else if (name == "validation") {
            row.app = 70;
            row.gameDirectory = "valve";
            Require(connection.Begin(row, "secret") == ConnectResult::OtherGame, "unsupported game launch accepted");
            row = Selected();
            row.responded = false;
            Require(connection.Begin(row, "secret") == ConnectResult::InvalidServer, "unknown row connected");
            row = Selected();
            Require(connection.Begin(row, "x\nquit") == ConnectResult::InvalidPassword, "control secret accepted");
            Require(connection.Begin(row, std::string(128, 'a')) == ConnectResult::InvalidPassword,
                    "oversized userinfo secret accepted");
            Require(engine.commands.empty() && engine.password == "previous", "rejected action mutated engine");
        } else {
            throw std::runtime_error("unknown connection case");
        }
    });
}
