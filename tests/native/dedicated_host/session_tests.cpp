#include "dedicated/native/DedicatedSession.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
int main(int argc, char** argv) {
    std::string error;
    if (argc == 3 && std::string(argv[1]) == "module") {
        cse::dedicated::DedicatedModuleSet modules;
        if (!modules.Open(argv[2], false, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 3;
        }
        return 0;
    }
    cse::dedicated::DedicatedSession session;
    if (argc > 1 && std::string(argv[1]) == "excess_arguments") {
        std::vector<const char*> excessive(cse::native_arguments::ArgumentCapacity, "a");
        if (session.Open(static_cast<int>(excessive.size()), excessive.data(), error)) {
            return 5;
        }
        return session.CurrentState() == cse::dedicated::DedicatedSession::State::Closed ? 0 : 6;
    }
    const char* arguments[] = {"hlds", "-console"};
    if (argc > 1 && std::string(argv[1]) == "retry") {
        setenv("CSE_TEST_FAIL_INIT", "1", 1);
        if (!session.Open(2, arguments, error) || session.Run() == 0) { return 2; }
        unsetenv("CSE_TEST_FAIL_INIT");
    }
    const auto previous = session.Generation();
    if (!session.Open(2, arguments, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 3;
    }
    if (session.Generation() <= previous) { return 4; }
    const int result = session.Run();
    session.Close();
    return result;
}
