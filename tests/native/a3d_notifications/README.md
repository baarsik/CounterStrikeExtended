# Native typed events and render notifications

These tests exercise the actual event, native buffer, retained A2D kernel and
notification owners. The waiter reentry suite also uses the actual SDL output
owner with process-local `SDL_AUDIO_DRIVER=dummy`; it never selects a real device.
The resource fixture in `../a3d_resources` also compiles the complete notification
suite against the actual stream submission owner. These fixtures do not qualify
the loaded-module export/unload path.

```sh
cmake -S tests/native/a3d_notifications -B build/a3d-native-20261111/notifications-task5-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DSDL3_DIR=/path/to/existing/SDL3/cmake/package
cmake --build build/a3d-native-20261111/notifications-task5-release --parallel 1
ctest --test-dir build/a3d-native-20261111/notifications-task5-release --parallel 1 --output-on-failure
```

Use `notifications-task5-debug` with `-DCMAKE_BUILD_TYPE=Debug`; use
`notifications-task5-sanitizers` with Debug and `-DA3D_SANITIZERS=ON` for ASan/UBSan.
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

- `notification_crossings`: actual render consumption for exact half-open
  endpoints, one/multiple laps, zero progress and seek; natural/explicit/final
  release/shutdown STOP; canonical typed view, exact caps, legacy QI/creation
  rejection, registration retirement and shutdown admission.
- `notification_replace_remove`: copied arrays and caller reference release;
  null/reserved/count/offset/foreign/closed/allocation failures preserve the old
  set; failure after each event acquisition balances retention; captured dispatch
  replacement/removal barriers, earlier committed permits, and close races.
- `notification_waiter_reentry`: real sleeping waiters immediately query the
  cursor, replace/remove registrations, Stop and Close. It covers manual actual
  Render and the actual SDL dummy producer followed by output Close/join.

The buffer snapshots immutable retained registration sets under its mix lock,
then leaves that lock before dispatch. A separate generation fence orders
replacement and internal signal commits; dispatch never acquires a buffer or
registration-owner lock. Fixture barriers park a captured batch before its fence
with a two-second watchdog. The new suites have a 15-second process watchdog.
No foreign event virtual is called, including for registration admission.

Native direct-kernel channel steps record an observational unsigned 64-bit
source-frame advance before modulo projection. This does not participate in
retained PCM arithmetic and adds no Windows structure fields. A separate logical
notification baseline is reset by seek and advances by real consumed bytes;
DWORD cursor snapshots alone are not used to infer full laps.

All six named suites are registered. The resource fixture now covers retained
`STREAM_SUBMITTED` updates, typed
provider/resource attachment, held source/buffer references, concurrency and
acquisition/shutdown cases. Task 5 remains subject to independent review; public
acoustic feature qualification is Task 6, and exported ABI/admitted module unload
remains Task 7. Private event counts do not establish `DllCanUnloadNow` integration.
