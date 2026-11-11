#include "SocketImageOwner.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <initializer_list>
#include <unistd.h>

static void Require(bool condition, const char* text) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::abort();
    }
}
#include <sys/wait.h>

int main(int argc, char** argv) {
    if (argc == 5 && std::strcmp(argv[1], "--child") == 0) {
        const int descriptor = std::atoi(argv[2]);
        const int ready = std::atoi(argv[3]);
        const int finish = std::atoi(argv[4]);
        const char absent = ::fcntl(descriptor, F_GETFD) == -1 && errno == EBADF ? '1' : '0';
        if (::write(ready, &absent, 1) != 1) {
            return 2;
        }
        char signal = 0;
        return ::read(finish, &signal, 1) == 1 ? 0 : 3;
    }
    Require(argc == 3, "fork or exec mode and actual provider path");
    Require(cse::socket_lifetime::InitializeProcess(), "explicit process infrastructure before provider load");
    void* image = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    Require(image != nullptr, "actual producer image load");
    auto* owner = new cse::socket_lifetime::SocketImageOwner;
    Require(owner->Open(image, argv[2]), "real anchored host and actual producer bind");
    const auto open = reinterpret_cast<int (*)(int)>(dlsym(image, "TestOpen"));
    const auto close = reinterpret_cast<void (*)(int)>(dlsym(image, "TestClose"));
    Require(open && close, "actual NET_IPSocket/close accessors");
    const int descriptor = open(-1);
    Require(descriptor > 0, "actual NET_IPSocket opens owned ephemeral loopback");
    sockaddr_in endpoint{};
    socklen_t bytes = sizeof(endpoint);
    Require(getsockname(descriptor, reinterpret_cast<sockaddr*>(&endpoint), &bytes) == 0, "own endpoint");
    int ready[2]{};
    int finish[2]{};
    Require(pipe(ready) == 0 && pipe(finish) == 0, "owned synchronization pipes");
    char descriptorText[32]{};
    char readyText[32]{};
    char finishText[32]{};
    std::snprintf(descriptorText, sizeof(descriptorText), "%d", descriptor);
    std::snprintf(readyText, sizeof(readyText), "%d", ready[1]);
    std::snprintf(finishText, sizeof(finishText), "%d", finish[0]);
    const pid_t child = fork();
    Require(child >= 0, "controlled normal fork");
    if (child == 0) {
        if (std::strcmp(argv[1], "exec") == 0) {
            execl(argv[0], argv[0], "--child", descriptorText, readyText, finishText, nullptr);
            return 4;
        }
        const char absent = ::fcntl(descriptor, F_GETFD) == -1 && errno == EBADF ? '1' : '0';
        if (::write(ready[1], &absent, 1) != 1) {
            return 5;
        }
        char signal = 0;
        return ::read(finish[0], &signal, 1) == 1 ? 0 : 6;
    }
    char absent = 0;
    Require(::read(ready[0], &absent, 1) == 1, "child remains alive with unrelated pipe usable");
    close(descriptor);
    const int replacement = open(ntohs(endpoint.sin_port));
    const bool rebound = replacement > 0;
    if (rebound) {
        close(replacement);
    }
    const char signal = 'x';
    Require(::write(finish[1], &signal, 1) == 1, "normal owned child completion");
    int status = 0;
    Require(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "normal child exit and wait");
    for (int descriptorToClose : {ready[0], ready[1], finish[0], finish[1]}) {
        Require(::close(descriptorToClose) == 0, "close only own synchronization descriptors");
    }
    Require(owner->Retire(), "actual producer terminal empty and host anchor release");
    delete owner;
    Require(dlclose(image) == 0, "normal provider close");
    std::printf("mode=%s child_socket_absent=%d parent_same_endpoint_rebound=%d child_normal_exit=1\n",
                argv[1], absent == '1', rebound);
    return absent == '1' && rebound ? 0 : 1;
}
