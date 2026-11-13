#include "dedicated/native/DedicatedCommandLine.h"
#include "common/icommandline.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    using cse::dedicated::DedicatedCommandLine;
    Require(argc == 2, "case required");
    DedicatedCommandLine command;
    std::string error;
    if (std::string(argv[1]) == "literal") {
        const char* arguments[] = {"/runtime@local/hlds", "-console", "+rcon_password", "p@ssword",
            "+hostname", "hello @server", "-literal", "@leading"};
        const int argumentCount = static_cast<int>(sizeof(arguments) / sizeof(arguments[0]));
        Require(command.Parse(argumentCount, arguments, error), error.c_str());
        const std::string expected =
            "/runtime@local/hlds -console +rcon_password p@ssword +hostname \"hello @server\" -literal @leading";
        Require(command.Encoded() == expected, "unexpected literal command encoding");
        Require(CreateNativeLiteralCommandLine(command.Encoded().c_str()), "literal initialization failed");
        Require(cmdline->GetCmdLine() == expected, "literal initialization expanded an at sign");
        char* value = nullptr;
        Require(cmdline->CheckParm("+rcon_password", &value) && value && std::string(value) == "p@ssword",
            "common parser changed the password");
        Require(cmdline->CheckParm("+hostname", &value) && value && std::string(value) == "hello @server",
            "common parser changed a quoted at sign");
        Require(cmdline->CheckParm("-literal", &value) && value && std::string(value) == "@leading",
            "common parser expanded a leading at sign");
        Require(CreateNativeLiteralCommandLine(cmdline->GetCmdLine()), "literal self-copy failed");
        Require(!CreateNativeLiteralCommandLine(nullptr), "null literal command must fail");
        constexpr std::size_t CommonCommandCapacity = 4096;
        const std::string oversized(CommonCommandCapacity, 'a');
        Require(!CreateNativeLiteralCommandLine(oversized.c_str()), "oversized literal command must fail");
        Require(cmdline->GetCmdLine() == expected, "failed literal update changed the active command");
        return 0;
    }
    if (std::string(argv[1]) == "flag") {
        const char* arguments[] = {"hlds", "-steam", "-console"};
        Require(command.Parse(3, arguments, error), error.c_str());
        Require(command.Console(), "terminal -console must select console even with -steam");
        const char* unsupported[] = {"hlds", "-HFILE", "4294967297"};
        Require(!command.Parse(3, unsupported, error), "Windows inherited handles must fail");
        return 0;
    }
    std::string exact(1023, 'a');
    const char* arguments[] = {exact.c_str()};
    Require(command.Parse(1, arguments, error), "1023 bytes must fit provider command");
    Require(command.Encoded().size() == 1023, "wrong encoded boundary");
    exact.push_back('a');
    arguments[0] = exact.c_str();
    Require(!command.Parse(1, arguments, error), "1024 bytes must fail before provider Init");
    const char* unicode[] = {"hlds", "+hostname", "Zażółć gęślą", "-console"};
    Require(command.Parse(4, unicode, error), error.c_str());
    Require(command.Encoded() == "hlds +hostname \"Zażółć gęślą\" -console", "UTF-8/space encoding");
    cmdline->CreateCmdLine(command.Encoded().c_str());
    char* value = nullptr;
    Require(cmdline->CheckParm("+hostname", &value) && value && std::string(value) == "Zażółć gęślą",
        "actual common parser did not preserve value");
    const char* invalid[] = {"hlds", "bad\"quote"};
    Require(!command.Parse(2, invalid, error), "unrepresentable quote must fail");
    return 0;
}
