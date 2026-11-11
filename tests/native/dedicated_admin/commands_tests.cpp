#include "dedicated/native/AdminCommands.h"
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
}
