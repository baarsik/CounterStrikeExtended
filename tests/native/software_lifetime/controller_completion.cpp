#include "browser_controller.h"
#include "browser_commands.h"
#include "process_channel.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unistd.h>

using namespace cse::browser;
static bool failNextWait = false;
static int waitCalls = 0;
extern "C" bool CseControlledWait(SDL_Process* process, bool block, int* code) {
    ++waitCalls;
    if (failNextWait) {
        failNextWait = false;
        return false;
    }
    return SDL_WaitProcess(process, block, code);
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--cse-browser-host") == 0) {
        Writer hello;
        hello.U32(ProviderContractVersion);
        hello.U32(ProviderApiVersion);
        const auto header = EncodeHeader({Kind::Control, static_cast<std::uint32_t>(Control::Hello), 0, 0,
                                          static_cast<std::uint32_t>(hello.Data().size())});
        assert(std::fwrite(header.data(), 1, header.size(), stdout) == header.size());
        assert(std::fwrite(hello.Data().data(), 1, hello.Data().size(), stdout) == hello.Data().size());
        assert(std::fflush(stdout) == 0);
        char byte = 0;
        while (read(STDIN_FILENO, &byte, 1) > 0) {
        }
        return 0;
    }
    assert(argc == 2);
    auto diagnostics = std::make_shared<ControllerDiagnostics>();
    ControllerConfig config{std::filesystem::absolute(argv[0]).string(), argv[1], diagnostics, {}};
    auto controller = CreateController(config);
    assert(controller->Init(nullptr, nullptr));
    assert(!ControllerQuiescent(*controller));
    BeginControllerShutdown(*controller);
    failNextWait = true;
    controller->Shutdown();
    assert(waitCalls == 1 && !ControllerQuiescent(*controller) && !diagnostics->cleanShutdown);
    controller->Shutdown();
    assert(waitCalls == 2 && ControllerQuiescent(*controller) && !diagnostics->cleanShutdown);
    controller.reset();
    assert(waitCalls == 2);
    std::puts(
        "PASS actual controller/channel retain failed-reap process, retry real wait, preserve failure diagnostic");
}
