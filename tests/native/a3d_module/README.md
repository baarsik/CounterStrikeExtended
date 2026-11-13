# Actual native A3D module boundary

This caller links no A3D implementation or mock factory. `A3D_MODULE` names the
owning runtime graph's actual `a3dapi.dylib`. `A3D_FOREIGN_MODULE` names a distinct
owning image (Release versus Debug) for module-local event rejection. Configure
and build those graphs first, using the existing dependency configuration.

```sh
cmake -S tests/native/a3d_module -B build/a3d-module-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DA3D_MODULE=/absolute/production/Release/a3dapi.dylib \
  -DA3D_FOREIGN_MODULE=/absolute/production/Debug/a3dapi.dylib
cmake --build build/a3d-module-tests --parallel 1
ctest --test-dir build/a3d-module-tests --parallel 1 --output-on-failure
```

Every CTest asserts process-local `SDL_AUDIO_DRIVER=dummy`. No physical device,
OEM driver or game-consumer acceptance follows from these tests.

The caller compiles independent retained source/geometry/listener declarations
in `caller_interfaces.h`. It computes its own standard 288-byte `CseGetModuleAbi`
expectation before resolving or calling a factory, creating an event, querying a
view or invoking any virtual. The production descriptor hashes all consumed
slots, record fields and widths, exact IIDs, native C/compiler architecture,
role and lifetime revision. Mutation tests reject every descriptor field and
contract family before a counted factory invocation. Native geometry/source
SDK tails return failure; no unsupported trailing vtable call is permitted by
an older descriptor. The provider additionally asserts actual and public method
shapes agree for all source/geometry/listener slots.

Tests observe existing production owner/event counter symbols only after ABI
admission. These observations are fixture diagnostics, not an extra public API,
replacement lifetime implementation or scheduling hook. An infinite waiter is
confirmed by the real wait count. Each host keeps its dlopen lease through all
outstanding calls, joins all waiters, verifies zero counts and S_OK, and only
then calls dlclose. There is no force unload, fake factory or always-busy stub.

The suites cover rejected class/IID/aggregation and failure cleanup; canonical
IUnknown across supported views; source and real direct/stream playback;
byte/STOP notifications, exact event/notify interfaces and capabilities,
invalid/foreign events and legacy HANDLE-view rejection; held API, source,
geometry, material, list and buffer views; balanced locks; standalone events,
registrations and active waits across provider release; and repeated image
load/release/unload. `callable_operation_matrix` checks supported outputs and
explicit unsupported failures. `source_buffer_playback` verifies the real
loaded source and native buffer routes rather than a PCM-only substitute.

Public device enumeration is declared in `src/public/cse_a3d_devices.h`.
The independent `a3d_enumeration_tests` caller uses only its own `enumeration_caller.h`
record and vtable declarations across the loaded production image boundary. A separate
admission TU compares record/method facts against production and constructs the expected
16-entry profile. Old and new profiles deliberately reject one another. The tests resolve
only `CseGetModuleAbi` before descriptor admission, then require the factory export before
any factory/virtual operation.

The host calls `SDL_SetMainReady` on its actual main thread before a worker can call the
factory. An unregistered `SDL_IsMainThread` may report true on an arbitrary thread; it
cannot establish cold-host thread identity. Factory acquisition and terminal Release must
be on the host main/control thread, serialized with all host SDL initialization/quit.
Concurrent readers each hold a reference; a main-thread anchor outlives and joins them.
The library lease covers every call and final Release. An enumerator alone refuses unload.

GetInfo accepts version 1 and exactly 32 record bytes, with bounded clearing on failure.
A size query/short name buffer returns S_FALSE, no token or partial label, and the complete
UTF-8 byte count including NUL. Names and records are caller-owned copies. Keep the snapshot
alive until InitializeDevice returns; the provider then owns its own SDL audio reference.
Copied names survive snapshot destruction, but copied tokens do not retain audio or image
lifetime. Tokens are ephemeral and do not promise universal cross-image collision rejection.
Hotplug can invalidate a snapshot token before initialization; create a new snapshot to retry.

All registered runtime cases require `SDL_AUDIO_DRIVER=dummy`. `empty_snapshot` observes the
real backend count without assuming a minimum. Explicit `enumeration_fault_*` subprocesses
load the test-only Darwin interposer to label synthetic empty/name/error/allocation branches.
They never redirect selected IDs or replace the factory/provider. Allocation failure is
restricted to module calls during the armed factory acquisition interval. Failure cases
verify allocation/audio balance and production owner counts, followed by actual image unload.
These tests provide no real-device or audibility evidence.

Set `A3D_FOREIGN_MODULE` to a byte-identical copy of the new dylib in a distinct directory,
using the same canonical staged SDL provider, and `A3D_BASELINE_MODULE` to the preserved old
module fixture. Foreign tests require genuinely distinct loaded images and observed unequal
tokens; they do not silently skip a collision-sensitive premise. Build/test jobs are serial.
