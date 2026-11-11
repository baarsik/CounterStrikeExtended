# Native typed events

These CPU-only tests exercise the actual event implementation, without SDL, an
output device, provider, Windows HANDLE services, or a loaded A3D module.

```sh
cmake -S tests/native/a3d_notifications -B build/a3d-native-20261111/notifications-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/a3d-native-20261111/notifications-release --parallel 1
ctest --test-dir build/a3d-native-20261111/notifications-release --parallel 1 --output-on-failure
```

Use `notifications-debug` with `-DCMAKE_BUILD_TYPE=Debug`; use
`notifications-sanitizers` with Debug and `-DA3D_SANITIZERS=ON` for ASan/UBSan.
All builds, including dependency builds, remain serial.

- `notification_event_reset_wait`: sticky auto/manual permits, coalescing,
  finite monotonic timeout, real CV-blocked waiters, and completion surviving
  Reset/Close before the waiting thread can return.
- `notification_cancel_close`: sticky cancellation without consuming target
  permits, target-close precedence, real sleeping infinite waits, terminal close,
  and 128 admission-before-sleep races driven by seed `0x6d2b79f5`.
- `notification_event_identity`: exact IID/layout and factory validation,
  canonical IUnknown, full-width identity, rejection without foreign virtual
  calls, retained state generation and release, module lifetime observations,
  checked allocator and condition initialization/wait failure paths.

Fixture-only hooks park admitted waits outside the owner mutex. An independent
condition-variable handshake proves waits have actually released the owner mutex
inside the platform CV wait. Both have five-second watchdogs; CTest caps each
suite at 30 seconds. These hooks/fault controls are compiled out of production.
The allocator fault supplies a null allocation result to the real factory unwind
path; no exception-enabled container or production throwing allocation is used.

The event mutex is the linearization domain for state and the intrusive registry.
Signal commits eligible wait outcomes while locked, and Reset cannot retract
those outcomes. Cancellation registration and target retention happen in the
same critical section. Every wait uses a stack CV and retained concrete objects;
the registry itself is non-owning. A private state token owns exactly one retained
reference and may be transferred, but copying its bytes does not AddRef. Tokens
must be released exactly once. Release removes the final registry entry before
free, while the enclosing active-call lease still prevents admitted unload.

`notification_tests.cpp` is deliberately unregistered until real buffer
registrations/crossings exist in Task 5. Export descriptor/admitted module unload
is Task 7. The event counters are an honest private integration seam, not a claim
that the current module has already wired `DllCanUnloadNow`.
