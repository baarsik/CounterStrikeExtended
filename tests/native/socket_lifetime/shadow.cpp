#include "native_socket_host.h"
extern "C" __attribute__((visibility("default"))) const cse::socket_lifetime::HostTable CseNativeSocketHostV1 = {
    sizeof(cse::socket_lifetime::HostTable), cse::socket_lifetime::Version, cse::socket_lifetime::HostContract,
    nullptr, nullptr, nullptr, nullptr};
