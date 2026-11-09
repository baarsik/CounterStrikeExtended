#include "fixture_context.h"

int main() {
    ResetApi();
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == api.created, "normal open returns owned socket");
    Require(api.openCalls == 1 && api.bindCalls == 1 && api.closed.empty(), "success keeps ownership with caller");
#if !defined(_WIN32)
    for (Failure failure : {Failure::Ioctl, Failure::Broadcast, Failure::Reuse, Failure::Tos, Failure::Bind}) {
        ResetApi();
        api.failure = failure;
        api.reuse = true;
        api.tos = true;
        Require(NET_IPSocket(nullptr, PORT_ANY, false) == 0, "failed open returns closed sentinel");
        Require(api.closed == std::vector<SOCKET>{api.created}, "failed stage closes its sole owned socket");
        Require(errno == kInjectedError, "failure errno survives diagnostics and close");
    }
    ResetApi();
    api.failure = Failure::Tos;
    api.tos = true;
    api.injectedError = ENOPROTOOPT;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == api.created && api.closed.empty(),
        "original unsupported TOS option remains nonfatal");
    ResetApi();
    api.failure = Failure::Socket;
    api.injectedError = EAFNOSUPPORT;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == 0 && api.closed.empty() && api.diagnostics.empty()
        && errno == EAFNOSUPPORT, "unsupported address-family suppression and errno preserved");
    ResetApi();
    api.created = 0;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == api.duplicate, "zero descriptor preserved through duplication");
    Require(api.closed == std::vector<SOCKET>{0} && api.duplicateCalls == 1, "zero ownership paired exactly once");
    ResetApi();
    api.created = 0;
    api.failure = Failure::Duplicate;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == 0, "failed duplicate rejects open");
    Require(api.closed == std::vector<SOCKET>{0} && errno == kInjectedError, "failed duplicate closes owned zero");
    ResetApi();
    api.created = FD_SETSIZE;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == 0, "high descriptor rejected before bitmap use");
    Require(api.closed == std::vector<SOCKET>{api.created} && errno == EMFILE, "high descriptor reports backend capacity");
    ResetApi();
    api.created = FD_SETSIZE - 1;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == api.created && api.closed.empty(), "last representable descriptor kept");
    ResetApi();
    api.created = 0;
    api.duplicate = FD_SETSIZE;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == 0, "high duplicate rejected");
    Require(api.closed == std::vector<SOCKET>({0, FD_SETSIZE}) && errno == EMFILE,
        "high duplicate pairs both owned references");
#else
    // Characterize the unchanged legacy failure exits; no real resource is opened in this control.
    for (Failure failure : {Failure::Ioctl, Failure::Broadcast, Failure::Reuse, Failure::Bind}) {
        ResetApi();
        api.failure = failure;
        api.reuse = true;
        Require(NET_IPSocket(nullptr, PORT_ANY, false) == 0, "legacy Windows failed-open result");
        Require(api.closed.size() == (failure == Failure::Bind ? 1u : 0u), "legacy Windows close behavior unchanged");
    }
#endif
    ResetApi();
    api.failure = Failure::Socket;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == 0 && api.closed.empty(), "failed creation owns no handle");
    ResetApi();
    char numeric[] = "127.0.0.1";
    Require(NET_IPSocket(numeric, 27015, true) == api.created && api.bindCalls == 1,
        "numeric-interface multicast open still follows the same option path");
    Require(api.boundAddress.sin_family == AF_INET && api.boundAddress.sin_port == htons(27015)
        && api.boundAddress.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && api.loopOption == 0,
        "exact IPv4 address, network-order port, default multicast loop option");
    ResetApi();
    api.failure = Failure::Loop;
    Require(NET_IPSocket(nullptr, PORT_ANY, false) == api.created && api.closed.empty(),
        "inherited multicast-loop warning remains nonfatal");
    Require(!api.diagnostics.empty(), "nonfatal option diagnostic retained");
    std::printf("actual_open_control_passed=1 socket_bytes=%zu fd_set_bytes=%zu fd_capacity=%d\n",
        sizeof(SOCKET), sizeof(fd_set), FD_SETSIZE);
    return 0;
}
