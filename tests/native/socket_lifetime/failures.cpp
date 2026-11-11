#include "SocketImageOwner.h"
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
using namespace cse::socket_lifetime;

namespace {
enum class Fault { None, Socket, Cloexec, CloseConsumed, AnchorClose, Install, InstallFork, CreateRace, CloseRace };
std::atomic<Fault> fault{Fault::None};
std::atomic<int> lastCreated{-1};
std::atomic<int> nativeCloses{0};
std::atomic<bool> prepareEntered{false};
std::atomic<bool> syscallEntered{false};
int heldPipe[2]{};
int raceClosed = -1;
bool installChildRejected = false;

void Require(bool condition, const char* text) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::abort();
    }
}

void ObserverPrepare() {
    prepareEntered.store(true);
}

void WaitForFork() {
    syscallEntered.store(true);
    while (!prepareEntered.load()) {
    }
}
}

int CseTestSocket(int family, int type, int protocol) {
    if (fault.load() == Fault::Socket) {
        errno = ENFILE;
        return -1;
    }
    const int descriptor = ::socket(family, type, protocol);
    lastCreated.store(descriptor);
    if (fault.load() == Fault::CreateRace) {
        WaitForFork();
    }
    return descriptor;
}

int CseTestFcntl(int descriptor, int command, ...) {
    va_list arguments;
    va_start(arguments, command);
    const int value = va_arg(arguments, int);
    va_end(arguments);
    if (fault.load() == Fault::Cloexec && command == F_SETFD) {
        errno = EIO;
        return -1;
    }
    return ::fcntl(descriptor, command, value);
}

int CseTestClose(int descriptor) {
    ++nativeCloses;
    const int result = ::close(descriptor);
    if (fault.load() == Fault::CloseRace) {
        // Raw fd has been consumed while the production close/remove transaction still holds the fork gate.
        // Allocate an unrelated descriptor with the exact same number to expose stale child-close records.
        const int replacement = ::dup2(heldPipe[0], descriptor);
        if (replacement != descriptor) {
            std::abort();
        }
        raceClosed = descriptor;
        WaitForFork();
    }
    if (fault.load() == Fault::CloseConsumed && result == 0) {
        errno = EIO;
        return -1;
    }
    return result;
}

int CseTestDlclose(void* image) {
    if (fault.load() == Fault::AnchorClose) {
        return -1;
    }
    return ::dlclose(image);
}

int CseTestAtfork(void (*prepare)(), void (*parent)(), void (*child)()) {
    if (fault.load() == Fault::Install) {
        return EAGAIN;
    }
    if (fault.load() == Fault::InstallFork) {
        const pid_t forkChild = fork();
        Require(forkChild >= 0, "controlled fork during process installation");
        if (forkChild == 0) {
            fault.store(Fault::None);
            std::exit(InitializeProcess() ? 1 : 0);
        }
        int status = -1;
        Require(waitpid(forkChild, &status, 0) == forkChild, "installation child normal wait");
        installChildRejected = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    return ::pthread_atfork(prepare, parent, child);
}

int main(int argc, char** argv) {
    Require(argc == 4, "scenario, actual provider and wrong-anchor control");
    const bool race = std::strcmp(argv[1], "races") == 0;
    fault.store(Fault::Install);
    Require(!InitializeProcess(), "registration failure refuses process admission");
    fault.store(Fault::None);
    void* image = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    Require(image != nullptr, "actual provider");
    SocketImageOwner owner;
    Require(!owner.Open(image, argv[2]), "no late installer fallback in Open");
    fault.store(Fault::InstallFork);
    Require(InitializeProcess(), "retry explicit installer");
    Require(installChildRejected, "inherited pre-registration process cannot establish new entitlement");
    fault.store(Fault::None);
    Require(!owner.Open(image, "/missing/real/anchor"), "missing genuine NOLOAD anchor rejected");
    void* wrongImage = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
    Require(wrongImage != nullptr, "real mismatched image anchor control");
    SocketImageOwner mismatched;
    fault.store(Fault::AnchorClose);
    Require(!mismatched.Open(image, argv[3]) && mismatched.Active(),
            "mismatched anchor rollback failure remains represented without a producer binding");
    fault.store(Fault::None);
    Require(mismatched.Retire() && !mismatched.Active(), "mismatched unbound anchor close retry");
    Require(dlclose(wrongImage) == 0, "normal wrong-anchor control release");
    Require(owner.Open(image, argv[2]), "actual anchored producer binding");
    const auto open = reinterpret_cast<int (*)(int)>(dlsym(image, "TestOpen"));
    const auto close = reinterpret_cast<void (*)(int)>(dlsym(image, "TestClose"));
    Require(open && close, "actual net accessors");
    if (race) {
        Require(pipe(heldPipe) == 0, "own unrelated pipe");
        Require(pthread_atfork(ObserverPrepare, nullptr, nullptr) == 0, "test-only prepare ordering observer");
        for (Fault kind : {Fault::CreateRace, Fault::CloseRace}) {
            int descriptor = kind == Fault::CloseRace ? open(-1) : -1;
            prepareEntered.store(false);
            syscallEntered.store(false);
            fault.store(kind);
            int childStatus = -1;
            std::thread forkThread([&] {
                while (!syscallEntered.load()) {
                }
                const pid_t child = fork();
                Require(child >= 0, "controlled overlapping fork");
                if (child == 0) {
                    const int fd = kind == Fault::CreateRace ? lastCreated.load() : raceClosed;
                    const bool expected = kind == Fault::CreateRace
                        ? ::fcntl(fd, F_GETFD) == -1 && errno == EBADF : ::fcntl(fd, F_GETFD) >= 0;
                    const char signal = expected ? '1' : '0';
                    const bool sent = ::write(heldPipe[1], &signal, 1) == 1;
                    std::exit(expected && sent ? 0 : 1);
                }
                Require(waitpid(child, &childStatus, 0) == child, "normal owned child wait");
            });
            if (kind == Fault::CreateRace) {
                descriptor = open(-1);
                Require(descriptor > 0, "publication completed before fork snapshot");
            } else {
                close(descriptor);
            }
            forkThread.join();
            fault.store(Fault::None);
            Require(WIFEXITED(childStatus) && WEXITSTATUS(childStatus) == 0, "fork race child ownership result");
            char signal = 0;
            Require(::read(heldPipe[0], &signal, 1) == 1 && signal == '1', "unrelated pipe still usable");
            if (kind == Fault::CreateRace) {
                close(descriptor);
            } else {
                Require(::close(raceClosed) == 0, "close only own reused unrelated descriptor");
            }
        }
        Require(::close(heldPipe[0]) == 0 && ::close(heldPipe[1]) == 0, "own pipe cleanup");
    } else {
        for (Fault kind : {Fault::Socket, Fault::Cloexec}) {
            fault.store(kind);
            const int before = nativeCloses.load();
            Require(open(-1) == 0, "actual creation failure rejects output");
            Require(errno == (kind == Fault::Socket ? ENFILE : EIO), "real injected error survives diagnostics");
            Require(nativeCloses.load() == before + (kind == Fault::Socket ? 0 : 1), "rollback closes exactly owned fd");
            fault.store(Fault::None);
        }
        const int descriptor = open(-1);
        Require(descriptor > 0 && (::fcntl(descriptor, F_GETFD) & FD_CLOEXEC), "CLOEXEC is established");
        if (std::strcmp(argv[1], "consumed") == 0) {
            fault.store(Fault::CloseConsumed);
            close(descriptor);
            fault.store(Fault::None);
            const int before = nativeCloses.load();
            const int unrelated = ::open("/dev/null", O_RDONLY);
            Require(unrelated >= 0, "own reuse control");
            close(descriptor);
            Require(nativeCloses.load() == before && ::fcntl(unrelated, F_GETFD) >= 0,
                    "consumed close error never retries numeric descriptor");
            Require(!owner.Retire() && owner.Active(), "cleanup fault retains actual image and owner");
            Require(::close(unrelated) == 0, "own unrelated cleanup");
            // Failure is intentionally nonrecoverable: normal process exit retains the image until OS teardown.
            std::puts("consumed_fault_retained=1 no_numeric_retry=1");
            std::exit(0);
        }
        close(descriptor);
    }
    fault.store(Fault::AnchorClose);
    Require(!owner.Retire() && owner.Active(), "checked anchor close failure preserves retry owner");
    fault.store(Fault::None);
    Require(owner.Retire() && !owner.Active(), "anchor close retry succeeds");
    // A second real owner cannot bind an already retired producer; force its rollback close to fail, then retry.
    SocketImageOwner rejected;
    fault.store(Fault::AnchorClose);
    Require(!rejected.Open(image, argv[2]) && rejected.Active(), "failed-bind anchor rollback retained");
    fault.store(Fault::None);
    Require(rejected.Retire() && !rejected.Active(), "failed-bind anchor can be released without retiring unbound producer");
    Require(dlclose(image) == 0, "normal final provider release");
    Require(!dlopen(argv[2], RTLD_NOW | RTLD_NOLOAD), "physical leaf unmap");
    const pid_t child = fork();
    Require(child >= 0, "fork after physical producer absence");
    if (child == 0) {
        return 0;
    }
    int status = -1;
    Require(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "permanent handler remains safe after producer unmap");
    std::puts("actual_owner_failure_retry_and_post_unmap_fork=passed");
}
