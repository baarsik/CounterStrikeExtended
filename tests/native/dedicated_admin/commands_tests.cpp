#include "dedicated/native/AdminCommands.h"
#include "dedicated/native/DedicatedCommandLine.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                       \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
int main() {
    std::string command;
    CHECK(cse::dedicated::BuildStartupCommands("zażółć server", "abc123", command));
    CHECK(command == "rcon_password \"abc123\"\nsetmaster enable\nhostname \"zażółć server\"\n");
    CHECK(!cse::dedicated::BuildStartupCommands("injected\"\nquit", "abc123", command));
    CHECK(command.empty());
    CHECK(!cse::dedicated::BuildStartupCommands(std::string(64, 'a').c_str(), "abc123", command));
    CHECK(!cse::dedicated::BuildStartupCommands("host", "x", command));
    cse::dedicated::DedicatedCommandLine startup;
    const char* original[] = {"hlds",     "-game",     "-old game",  "+map",
                              "+old map", "-insecure", "-unchanged", "value with spaces"};
    std::string error;
    CHECK(startup.Parse(8, original, error));
    cse::dedicated::StartupSettings settings{"cstrike", "de fixture", 12, 28015, true, true};
    CHECK(startup.ApplySetup(settings, error));
    CHECK(
        startup.Encoded() ==
        "hlds -unchanged \"value with spaces\" -game cstrike -maxplayers 12 +sv_lan 1 +map \"de fixture\" -port 28015");
    const std::string accepted = startup.Encoded();
    settings.map = "bad\"map";
    CHECK(!startup.ApplySetup(settings, error));
    CHECK(!error.empty() && startup.Encoded() == accepted);
}
