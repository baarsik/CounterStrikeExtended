#pragma once
#include "../public/native_socket_host.h"

namespace cse::socket_lifetime {
bool Admitted() noexcept;
Status Retire() noexcept;
int CreateUdp() noexcept;
// On rejection the descriptor remains owned. Consumed errors remain terminal faults.
CloseResult CloseUdp(int descriptor) noexcept;
} // namespace cse::socket_lifetime
