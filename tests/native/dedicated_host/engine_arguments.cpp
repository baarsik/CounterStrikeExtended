#include "dedicated/native/DedicatedCommandLine.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "actual_arguments.inc"

static void RoundTrip(const std::vector<std::string>& values) {
    std::vector<const char*> pointers;
    for (const auto& value : values) {
        pointers.push_back(value.c_str());
    }
    cse::dedicated::DedicatedCommandLine command;
    std::string error;
    assert(command.Parse(static_cast<int>(pointers.size()), pointers.data(), error));
    std::string encoded = command.Encoded();
    Sys_InitArgv(encoded.data());
    assert(host_parms.argc == static_cast<int>(values.size()) + 1);
    assert(std::string(host_parms.argv[0]).empty());
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] != host_parms.argv[index + 1]) {
            std::fprintf(stderr, "argument %zu did not round-trip\n", index);
        }
        assert(values[index] == host_parms.argv[index + 1]);
    }
}

int main() {
    RoundTrip({"hlds", "-console", "+hostname", "Zażółć gęślą", "-game", "my mod", "", " path "});
    RoundTrip({std::string(1023, 'x')});
    std::vector<std::string> maximum(MAX_NUM_ARGVS - 1, "a");
    maximum.back() = "last UTF-8 ż value";
    RoundTrip(maximum);
    std::vector<const char*> excess(MAX_NUM_ARGVS, "a");
    cse::dedicated::DedicatedCommandLine command;
    std::string error;
    assert(!command.Parse(static_cast<int>(excess.size()), excess.data(), error));
    for (const char* invalid : {"bad\"quote", "bad\tvalue", "bad\x01value", "bad\x7fvalue"}) {
        const char* values[] = {"hlds", invalid};
        assert(!command.Parse(2, values, error));
    }
    std::puts("PASS actual encoder -> Sys_InitArgv -> COM_InitArgv UTF-8/quotes/count/storage boundaries");
}
