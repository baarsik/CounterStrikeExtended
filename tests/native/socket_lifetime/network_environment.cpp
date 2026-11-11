namespace {
#include "actual_network_state.h"
}
#include "actual_network_quiescence.h"


extern "C" __attribute__((visibility("default"))) void TestNetworkState(int worker, int configured, int descriptor) {
    net_thread_initialized = worker;
    net_configured = configured;
    ip_sockets[0] = descriptor;
}
