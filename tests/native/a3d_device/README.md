This fixture separates three boundaries: the typed native device contract, actual
system DirectSound initialization, and the complete owned A3D class factory.
Passing one boundary does not qualify a native A3D provider or game features.

`extract_sources.py` compiles the exact current `CA3d::Init`, native adapter,
`SetupBackend` and `ShutdownBackend` bodies. Generated source line directives and
body hashes identify the inputs. `fixture_support.h` adapts receiver declarations,
records compatibility/resource-mode requests, and supplies an empty actual source
list with its real Next/Clear bodies. It is not the complete CA3d class ABI. macOS uses a minimal COM declaration
adapter; Windows uses real SDK COM/DirectSound declarations. Interface doubles
are explicitly test objects, and unselected renderer methods abort.

Native tests use actual heap GUID storage above DWORD range, complete nonzero
GUID contents, nullable/default selection, callee-scope copies, initialization
failure, missing interface/caps backend, successful and failed reinitialization,
QI ownership and existing feature/compatibility requests. Provider copies survive
release of caller GUID storage. The native V1 IID is distinct and pinned in the
production private header. The legacy external IID, `InitializeEx(DWORD, ...)`
vtable, Win32 initialization route, caps layout, factory and renderer stubs remain
unchanged. No owned stub advertises the new native IID. Win32 repeat tests perform
one valid initialization; only the new native route qualifies repeated calls.

On Windows, `real-default` creates an actual uninitialized system DirectSound COM
object and runs the actual setup/init/shutdown bodies with a default device. It
creates no sound buffers and never plays output. This manually supplied object
path is separate from the original factory: the factory activates external OEM
CLSID 442d12a1 first, then legacy CLSID_A3d and A3D.DLL version checks. It does not
create system DirectSound. A missing real audio device is an observed probe
failure, not a successful provider result.

The Windows full module is built from a query of its owning CMake target,
including its five generated geometry sections and effective per-source
settings. The factory probe loads that actual module, obtains DllGetClassObject,
calls CreateInstance, and separately records actual OEM/legacy COM activation
HRESULTs. The missing-provider test requires measured REGDB_E_CLASSNOTREG results
and the corresponding factory failure. It does not register a DLL, install a
driver, replace COM activation, or substitute a successful provider.

The full-module fixture preserves the original Win32 export definition. For
native builds it projects only the two C export aliases to native C names,
retaining ordinals 2/3 and PRIVATE. This is fixture-only export handling; root
production export integration is a separate requirement. No
production definition file is edited by this fixture.

`CSE_A3D_DEVICE_BUILD_FULL_MODULE=OFF` explicitly limits a Windows run to the
contract and real-DirectSound leaf probes. It does not qualify the complete
module/factory. This split retains the observed ARM64 full-module compile failure
at a2d.cpp's x86 floating-point-control header pending its separate bounded port.

From the repository root on macOS:

```sh
cmake -S tests/native/a3d_device -B build/a3d-device-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=arm64 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build/a3d-device-release
ctest --test-dir build/a3d-device-release --output-on-failure
```

Use another Debug directory; add `CSE_A3D_DEVICE_SANITIZERS=ON` for ASan/UBSan.
For Windows use the matching x86, x64 or native ARM64 MSVC environment, omit OSX
arguments and select `CSE_TARGET_ARCH`. Windows also builds/runs the real-provider
and full-module probes. `CSE_A3D_DEVICE_SOURCE` selects a preserved complete A3D
source directory for original/current comparison. The interface declaration used
by test doubles remains current; original production bodies are not rewritten.
`CSE_A3D_DEVICE_REFERENCE=ON` enables Clang's MS pointer-cast extension only to
execute the preserved truncating reference body. Strict current builds have no
cast or warning suppression.

There is no new SDL/engine/world-geometry hook, renderer, native provider,
auth/device stub promoted to parity, buffer playback or game execution here.
The unchanged owned sound backend's Initialize/InitializeEx remain E_NOTIMPL,
and ResMan still depends on external OEM activation. Source/component results,
manual DirectSound initialization and factory/provider availability must be
reported separately. Existing broader source/renderer lifetime and failure
behavior remains outside this bounded initialization change.
