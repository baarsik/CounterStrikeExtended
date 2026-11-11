#include "DedicatedSession.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <unistd.h>
#include <vector>
int main(int argc, char** argv) {
    try {
        cse::dedicated::DedicatedSession session;
        std::string error;
        if (!session.Open(argc, argv, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return EXIT_FAILURE;
        }
        const int result = session.Run();
        if (result == EXIT_SUCCESS && session.RestartRequested()) {
            // Exec only after the complete admitted cohort and terminal owner close.
            std::vector<char*> arguments;
            for (const auto& argument : session.CommandLine().Arguments()) {
                arguments.push_back(const_cast<char*>(argument.c_str()));
            }
            arguments.push_back(nullptr);
            execv(session.Executable().c_str(), arguments.data());
            std::perror("Dedicated restart exec");
            return EXIT_FAILURE;
        }
        return result;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Dedicated startup failed: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
