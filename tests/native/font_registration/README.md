# macOS custom-font registration lifecycle

This fixture builds the production registration service once as a shared dylib
and links two independently unloadable consumer dylibs to it. No game, window,
full UI, CEF or provider is launched.
It uses the same `cmake/MacFontService.cmake` definition helper as the production
targets, including hidden C++ visibility and the `@rpath` install name. The
shared library is emitted into this fixture's `runtime/` subdirectory.

```sh
cmake -S tests/native/font_registration -B build/font-registration-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_NATIVE_THIRD_PARTY_PREFIX="$PWD/build/native-third-party/macos14/cse-arm64-macos"
cmake --build build/font-registration-release
ctest --test-dir build/font-registration-release --output-on-failure
```

Use a separate `Debug` directory for the debug dependency configuration. Add
`-DCSE_FONT_SANITIZERS=ON` in another Debug directory for ASan/UBSan.

The ten cases check actual CoreText registration plus the frozen FreeType
discovery boundary before and after final release, repeated registration,
multiple owners, symbolic/hard-link aliases, external pre-existing registration,
independent files with duplicate names, invalid inputs/data, failed release and
retry, collection ownership, UTF-8 resolved paths longer than 260 bytes, VFS
buffer growth/failure, both extracted production `AddCustomFontFile` bodies,
64 restarts with reused owner tokens, two real consumer dylib unload/reload
lifetimes, and process-exit cleanup. The complete Surface translation units and
their UI shutdown methods are not executed by this fixture.
The exit case also requires a successful real service release from a client
destructor after lease-book cleanup, catching service destruction that occurs
too early even when font absence and leak checks pass.

`MacFontRegistration.cpp` must be compiled exactly once into a shared runtime
service, linked by every consumer. Its two exported C functions take an opaque
owner address and a resolved UTF-8 path, with no cross-module C++ allocation
ownership. The service uses canonical file identity, tracks unique owner sets,
and pairs registrations it created with a process-scope unregistration after
the final owner. An existing registration at the same file URL is borrowed;
releasing it leaves the external registrar responsible for its lifetime.
CoreText's `AlreadyRegistered` result is not treated as another owned reference.

The client helper has translation-unit-local linkage so a module's unload
guard releases only that module's owners. Explicit Surface shutdown is
idempotent and removes those leases early. The registration service must remain
loaded until the consumers' destructors finish, as enforced by the shared
library dependency. Surface calls retain their existing main-thread UI contract;
the central registry serializes its C API operations.
The service constructs its registry in a hidden module initializer so its
destructor is registered before dependent client teardown callbacks.

The full resolved path is retained independently of `CUtlSymbol`. Files must
stay in place while registered, as required by Apple's
[registration API](https://developer.apple.com/documentation/coretext/ctfontmanagerregisterfontsforurl(_:_:_:)).
If a registered path disappears or changes identity, release fails visibly and
retains the owner for retry after the original path is restored. It does not
unregister an unrelated replacement file. External code that directly removes
or replaces service-owned catalog entries is outside this ownership contract;
all owned project consumers must use this one shared service.

The Unicode TTF and mixed TTC are generated only in the build directory from
the installed system Arial font and the existing local name-metadata fixture
generator. They retain local font data, are never installed persistently, and
must not be distributed. No font binary or system-font change is checked in.

These are native font lifecycle fixtures, not Windows regression or full
localization/UI acceptance. macOS 14 deployment metadata does not prove
execution on a macOS 14 machine.
