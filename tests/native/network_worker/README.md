# Native network worker fixtures

This suite compiles `engine/network_worker.cpp` and complete production bodies
extracted from `engine/net_ws.cpp`: UDP receive, queued packet append/consume,
start/stop, allocation/flush, configure and shutdown. It compiles the actual
`packetlag_t` and `net_messages_t` definitions with the production `netadr.h` and
checks each field offset for 32-bit and 64-bit pointer storage. These queues
contain native pointers; their struct bytes are never sent as protocol records.

The fixtures use real nonblocking loopback UDP sockets. They verify source
0/1/2 drain order, FIFO bytes and sender address, pool exhaustion and heap
ownership, packet-size boundaries, stop/flush during an in-flight packet,
idle wake/join with both sleep preferences, 64 restart cycles, stop during a continuously busy
drain, failed creation followed by restart, socket replacement and shutdown.
POSIX failure fixtures use real descriptor limits, including denial of the
second wake socket. Windows failure fixtures temporarily shut down their own
Winsock session. A continuously busy callback makes the cancellation check
independent of UDP scheduling; the byte/order cases use actual production UDP
receive and queue bodies.

Engine dependencies are controlled: allocation tracks every queue-owned block;
the lag callback passes through packets with no artificial lag; split-packet
assembly and the live Steam provider are not exercised. A controlled FakeIP-style
message queue verifies reception without UDP traffic, both with idle UDP sockets
and without UDP sockets, with and without `-netsleep`. Socket opening in the
configure fixture is a local UDP provider. No game, network server, microphone,
netchan, authentication, integrity policy or external plugin runs in this suite.
Passing these fixtures does not qualify the complete engine translation unit,
the complete native runtime or compatibility with an external server.

The packet mutex does not make every dependency of the inherited receive path
thread-safe. Artificial-lag/loss processing still reaches the engine clock,
cvars, shared RNG/hooks and diagnostics; Steam FakeIP has a separate provider
lifecycle. These paths need full engine qualification and a main-thread handoff
before claiming that the complete receive graph is race-free. The standalone
TSan result covers the controlled fixture dependencies described above.

From the matching MSVC native tools prompt (use `x86`, `x64`, or conditional
`arm64` for both the toolchain and `CSE_TARGET_ARCH`):

```powershell
cmake -S tests/native/network_worker -B build/network-worker-x64-release -G Ninja -DCSE_TARGET_ARCH=x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/network-worker-x64-release --parallel
ctest --test-dir build/network-worker-x64-release --output-on-failure -V
cmake -S tests/native/network_worker -B build/network-worker-x64-debug -G Ninja -DCSE_TARGET_ARCH=x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/network-worker-x64-debug --parallel
ctest --test-dir build/network-worker-x64-debug --output-on-failure -V
```

Windows builds run the same fixture linked separately with `ws2_32` and
`wsock32`, preserving both socket import policies. The worker uses Winsock 1.1
compatible calls and retains full-width `SOCKET` values in `fd_set`.

On macOS ARM64:

```sh
cmake -S tests/native/network_worker -B build/network-worker-macos-arm64-release -G Ninja -DCSE_TARGET_ARCH=arm64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/network-worker-macos-arm64-release --parallel
ctest --test-dir build/network-worker-macos-arm64-release --output-on-failure -V
```

Repeat in a separate Debug directory with `-DCMAKE_BUILD_TYPE=Debug`. For Clang
ASan/UBSan use a separate directory and `-DCSE_NETWORK_SANITIZER=address`; for
TSan use `-DCSE_NETWORK_SANITIZER=thread`. MSVC supports the `address` selection
where the selected toolchain provides AddressSanitizer.

For fixture sensitivity checks, `CSE_NETWORK_SOURCE` can select an archived
`net_ws.cpp` and `CSE_NETWORK_WORKER_SOURCE` a temporary mutated worker copy.
Extraction rejects ambiguous or missing whole functions and struct definitions.
The source files in the checkout do not need to be edited for these checks.
