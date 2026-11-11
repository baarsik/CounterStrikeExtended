#include "dedicated/native/DedicatedCommandLine.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "actual_startup.inc"

int main() {
    constexpr std::size_t MaximumTokenBytes = 514; // Actual Cmd_TokenizeString token limit, excluding terminator.
    const std::vector<std::string> testValues = {
        "CSE Zażółć gęślą", "", "C++ +/- literal - +", "+ literal", "- literal", "-42", "+42", "+/-", "-", "+",
        "x;hostname injected", "a:b,(c)//d", std::string(MaximumTokenBytes, 'x')
    };
    for (const std::string& value : testValues) {
        cse::dedicated::DedicatedCommandLine command;
        std::string error;
        const char* arguments[] = {"hlds", "-console", "+hostname", value.c_str(), "-game", "valve"};
        assert(command.Parse(6, arguments, error));
        std::string encoded = command.Encoded();
        Sys_InitArgv(encoded.data());
        Cmd_TokenizeString("stuffcmds");
        values.clear();
        Cmd_StuffCmds_f();
        Cbuf_ExecuteCommandsFromBuffer(&cmd_text, TRUE, -1);
        if (values.size() != 1 || values.find("hostname") == values.end() || values["hostname"] != value) {
            std::fprintf(stderr, "startup hostname mismatch: expected [%s], actual [%s]\n",
                value.c_str(), values["hostname"].c_str());
            return 1;
        }
    }
    cse::dedicated::DedicatedCommandLine command;
    std::string error;
    std::string oversizedToken(MaximumTokenBytes + 1, 'x');
    const char* oversized[] = {"hlds", "+hostname", oversizedToken.c_str()};
    assert(!command.Parse(3, oversized, error));
    const std::string direct = "+hostname " + oversizedToken;
    assert(!cse::native_arguments::Validate(direct.c_str()));
    std::string first(335, 'a');
    std::string second(335, 'b');
    std::string third(336, 'c');
    const char* maximumLine[] = {"a", "+hostname", first.c_str(), second.c_str(), third.c_str()};
    assert(command.Parse(5, maximumLine, error));
    third.push_back('c');
    maximumLine[4] = third.c_str();
    assert(!command.Parse(5, maximumLine, error));
    const char* invalidCommand[] = {"hlds", "+hostname;quit", "x"};
    assert(!command.Parse(3, invalidCommand, error));
    std::puts("PASS actual startup reconstruction -> command buffer -> tokenizer -> Cvar command value");
}
