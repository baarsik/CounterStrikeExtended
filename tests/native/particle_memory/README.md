# Native particle allocation and pool fixture

The actual module is selected from
`GoldSrc-SDK/projects/vs2026/particleman.vcxproj`. `manifest_sources.py` invokes
the repository's retained MSBuild evaluator and applies its six selected sources,
include directories, per-file definitions, and forced includes. The shared module
and component executable use the same production object files. The fixture does
not copy an allocation body or effects implementation.

The boundary executable includes the actual public particle header and disables
exceptions. A recording manager verifies exact dispatch up to INT_MAX, rejection
above INT_MAX and above 4 GiB without dispatch, native-size construction in real
small allocated storage, and pool-null failure skipping construction. The existing
POSIX nothrow overload follows the same checks. The virtual int-size API and vtable
remain unchanged; the regular class allocation function is now noexcept so its
existing null failure contract has defined new-expression behavior.

The native core-base allocator also declares its existing null-result contract
with `noexcept`; mismatched sizes, maximum-size requests, actual pool capacity
and exhaustion are checked separately. Its original Windows size-mismatch throw
is retained.

The full production component checks the original 1,875-slot capacity, distinct
pool-owned storage, exhaustion and its diagnostic, reset/reuse, render-disabled
failure, ordinary timed death and recycling, and 64 shutdown/restart cycles.
Reset retains its original Windows behavior: it returns active storage to the
free list without Die or C++ particle-destructor calls. Ordinary timed death
separately invokes virtual Die. Tests perform no GL drawing or game execution.

The pool's Win32 record has four pointers including its vptr: 16 bytes. Its
original 30,000-byte metadata budget therefore yields 1,875 slots. Native64 records
are 32 bytes; the metadata budget scales to retain the same slot count. Actual
particle block storage follows the registered native class sizes. The layout probe
uses the real public and Counter-Strike class declarations; it prints all selected
class sizes, not a packed or copied test structure.

The private spiral phase helper retains the original low signed 32 address bits.
Its tests cover both signed endpoints and native addresses with equal low 32
bits. The existing floating-point expressions and address-derived phase rule
remain intact. This does not establish visual or cross-address pixel parity.

The actual-module driver loads CreateInterface, constructs the real manager,
passes isolated engine callbacks, allocates pool storage, deletes the real
concrete manager through its virtual destructor, and unloads the image. The public
interface has no release method; this fixture knows the pinned concrete factory
type. Separate cases unload after reset, unload with active raw storage, unload
without ever constructing a pool, and perform 64 real image reloads. A component
case also leaves active storage for process-exit cleanup.

The POSIX guard is registered at module initialization after the pool vector and
lists in the same translation unit. Its target is the existing singleton pointer,
bound by Instance without lazy guard construction. Teardown executes the same
Reset then Shutdown order used by Windows DllMain, before vector/list destruction.
Shutdown clears the pointer so restart and a later guard are safe. Windows retains
its existing DllMain cleanup responsibility.

## Commands

From the repository root on Apple Silicon:

```sh
cmake -S tests/native/particle_memory -B build/particle-memory-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCSE_PARTICLE_MEMORY_LEAK_CHECKS=ON
cmake --build build/particle-memory-release --parallel
ctest --test-dir build/particle-memory-release --output-on-failure
```

Repeat in Debug with a separate directory. Eight optional Apple leaks scenarios
require a completed fixture and zero leaked bytes; full logs go to
`CSE_PARTICLE_MEMORY_RECEIPT_DIR` (default: the build's receipts directory).

For ASan/UBSan, use another Debug directory with
`-DCSE_PARTICLE_MEMORY_SANITIZERS=ON`. Production objects, the shared module, and
drivers are instrumented. The Apple runtime's true image-reload ODR registration
failure for `g_cFrustum` is retained in the original receipt; this mode omits only
the real image-reload case. API, active/unused unload, process exit, and 64 pool
restart cases remain instrumented. There is no sanitizer suppression.

Use another directory with `-DCSE_PARTICLE_MEMORY_SANITIZERS=ON
-DCSE_PARTICLE_MEMORY_SANITIZE_MODULE=OFF` for 64 actual image reloads with an
instrumented driver and explicitly uninstrumented production objects/module.
That mode does not qualify production accesses under sanitizers. Do not combine
the sanitizer options with the Apple leaks option.

For Windows, use the same configure/build/test commands from the corresponding
x86, x64, or ARM64 MSVC environment, omit Apple's two CMAKE_OSX arguments and the
leaks option, and choose a distinct directory. Windows uses /W4 /WX and matching
Debug/Release DLL CRT settings. The boundary fixture disables exception handling;
the actual module retains its manifest's existing exception policy.

## Limits

The fixture's own code treats warnings as errors. Legacy SDK headers are external
includes for standalone test drivers. The actual production module uses ordinary
manifest-owned includes with visible inherited warnings on Clang, and native
integer narrowing is an error; no production warning suppression was added.
The separate pre-existing CCoreTriangleEffect allocation/check helpers are unchanged.

RequestNewMemBlock still requires appropriately registered particle classes and
retains its existing behavior of ignoring its int argument. This task checks the
native-size boundary and defined registered-class use; it does not add support
for unregistered oversized classes, over-aligned classes, allocator concurrency,
or true multi-GiB allocations. Minimum macOS14 load metadata does not qualify
execution on macOS14. Windows execution and full game acceptance are separate.
