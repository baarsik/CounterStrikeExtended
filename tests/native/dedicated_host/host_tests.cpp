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
