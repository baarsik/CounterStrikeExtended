#include "native_socket_lifetime.h"
#include "fixture_context.h"

extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char*, int*) {
    return nullptr;
}

extern "C" __attribute__((visibility("default"))) int TestOpen(int port) {
    liveLoopback = true;
    char loopback[] = "127.0.0.1";
    return NET_IPSocket(loopback, port, false);
}

extern "C" __attribute__((visibility("default"))) void TestClose(int descriptor) {
    NET_PosixCloseOpenFailure(descriptor, 0);
}

qboolean net_thread_initialized;
SOCKET ip_sockets[NET_SOCKET_SOURCES];
int net_configured;
#if __has_include("actual_network_quiescence.h")
#include "actual_network_quiescence.h"
#endif
extern "C" __attribute__((visibility("default"))) void TestNetworkState(int worker, int configured, int descriptor) {
    net_thread_initialized = worker;
    net_configured = configured;
    ip_sockets[0] = descriptor;
}
