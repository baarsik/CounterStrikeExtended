This fixture qualifies the bounded native POSIX transport repairs in `net_ws.cpp`.
It extracts exact production method bodies at configure time; it does not link or
execute the engine, its static Steam initializer, the cooperative network worker,
netchan, fakelag or a game. The whole production translation unit is separately
compiled using the actual generated Release/Debug command row and headers.

The seven Mac tests cover:

- Open success, IPv4/port/options, all existing fatal open stages, exact owned
  close counts and saved errno, descriptor zero duplication, duplication failure,
  native `FD_SETSIZE` edges, unsupported TOS, and the inherited nonfatal multicast
  loop warning.
- Native bitmap bounds/canaries, duplicate descriptors and `maxfd + 1`.
- Finite/infinite/empty waits, positive counts, error propagation, and unchanged
  stagger ordering/timeval arithmetic at the default `sys_ticrate` of 100.
- Checked native count and address lengths, negative/oversize requests, error and
  zero receive results, actual native socklen storage, and checked copy-back.
- Exact threshold/client/server split packet bytes and order; split failure and
  inherited short-send counting; FakeIP dispatch precedence through a controlled
  binding only (no provider qualification).
- Real owned local pipe readiness, finite timeout, error and one-close lifetime.

The typed socket API controls never open a real socket. Their signatures use the
platform's actual `ssize_t`/`size_t`/`socklen_t` or Winsock `int` types. The synthetic
`fixture.invalid` host record keeps parser branches reachable without DNS; any
other hostname aborts. Windows builds run both retained WSOCK32 and Winsock2
policies, preserve high SOCKET bits and characterize the existing Windows open
failure ownership behavior. The fixture uses the production `_CRT_SECURE_NO_WARNINGS`
definition for the retained CRT calls and retains the production parser's narrow
4996 pragma; it adds no production warning suppression.

```sh
cmake -S tests/native/network_platform -B build/network-platform-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/network-platform-release
ctest --test-dir build/network-platform-release --output-on-failure -V
```

Use Debug for the Debug gate, and `-DCSE_NETWORK_SANITIZERS=ON` with Debug for
ASan/UBSan. Apple's current sanitizer runtime does not support `detect_leaks=1`;
that rejected invocation is retained separately. The accepted sanitizer run uses
`ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`, with nonrecovering
UBSan instrumentation. It makes no LeakSanitizer claim.

`compile_tu.py --database <actual compile_commands.json> --output <object>` replays
the owning engine command, changing only the output and dependency receipt flags.
`--sanitizers` instruments that compile-only object too. It must not be linked and
executed as a standalone transport test because its production forced include has
provider initialization responsibilities.

`verify_windows_selection.py` checks selected source tokens against an exact
pre-fix file or the immutable V11 Windows source. This is source-selection evidence,
separate from the fresh MSVC actual-method fixture invocations. `windows_matrix.py`
uses a fresh physical source workspace, validates its frozen hash manifest, keeps
compiler environments in RAM, builds with `/W4 /WX /EHsc`, runs both policy suites
for x86/x64/ARM64 Release/Debug and records actual PE machine IDs and binaries.

The Mac `network_loopback_probe` is deliberately absent from CTest. `--describe`
opens no socket. `--loopback` requires separate network-fixture authorization.
It uses the actual production open/send/readiness/receive bodies, two independently
owned nonblocking IPv4 UDP sockets bound to `127.0.0.1:ephemeral`, and no DNS,
worker, FakeIP provider or game. Binding/sending wrappers reject other addresses.
It verifies source family/address/owned sender port, checked receive lengths,
known bytes/order, empty nonblocking receive and one close per sole owned reference.

The proposed run sends exactly three datagrams: bytes `00 7f 80 ff`, then a
1401-byte payload `payload[i] = i % 251` split into 1400 and 19 bytes. Both split
records use fixed nine-byte little-endian headers: ID -2, sequence 2, packet IDs
0x02/0x12. Receive capacity is 1401 to detect an oversize/truncated datagram; counts
above 1400 are rejected. Each of three receives has at most 50 waits of 20,000 us;
a monotonic five-second total deadline is checked around the whole operation.
Any failed assertion aborts this isolated process, whose owned descriptors are
then reclaimed by the OS. The normal path explicitly closes both once. The review
packet includes each concrete binary's hashes/identity/describe output and expected
packet files. No Mac UDP execution is included in the initial sealed packet.

The retained POSIX readiness backend is `select`: descriptor indices must be
below native `FD_SETSIZE` (measured 1024 on this Mac). A high open descriptor fails
explicitly with a diagnostic and closes only its owned reference. Success with
higher descriptors is not qualified. These checks do not qualify the full native
engine, Linux runtime, worker/provider behavior, external transport or gameplay.
Production Windows selected tokens are unchanged; its existing non-bind open
failure leaks are characterized, not repaired in this POSIX task.
