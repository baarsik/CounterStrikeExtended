# Native SDL mouse-mode bridge

The native64 source provider is SDL3.4.20+CSE mouse-query V1. The upstream archive remains
`f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`, with no vendor source,
version, feature-default or dynapi changes. One owned C translation unit adds one scalar C export.
Its building-provider visibility macro is local to that source. The shared header exposes no
SDL objects, layouts, callbacks, allocations or pointers. Win32's prebuilt/default provider is unchanged.

`CseSdl_QueryMouseModeV1(1)` returns0 NotReady,1 Absolute,2 Relative,3 WrongThread,
4 ContractMismatch or5 ForeignDispatch. Only1/2 are successful observations. Initialization/teardown
must be serialized with calls; the wrong-thread result does not make concurrent SDL_Quit valid.
The SDK main-thread check precedes device access. Availability includes VIDEO/device/not-quitting;
the video owner is checked before reading global mode. The default-handler getters compare typed
C function identities internally and never invoke a handler. Normal custom assertion handlers are
independent of this identity. This rejects the pinned official whole-table foreign dispatch;
it makes no malicious-provider security claim.

The controlled fixture imports nine complete pinned SDK bodies, including the actual main-thread,
availability, video-owner, default/current-handler and global-mode getters. Declarations are the
complete real private headers. A byte-identical vendor minimal configuration and typed fake services
are confined to this fixture; this is not the real provider's generated configuration/layout proof.
Counting wrappers redirect function names through macros without changing imported body text.
Both fake callback identities have distinct observable bodies so intra-image ICF cannot fold them.
No handler is invoked by a valid query. SDK state snapshots, pending accumulator values and forbidden
error/allocation/pump/warp/drain/mode/hint/log seams detect query-side effects. Version/thread guards
must not touch a deliberately inaccessible device. x86 runs the header/legacy-hypothesis gate;
the native64 bridge implementation is not selected there.

The first naive local-mode witness must compile and fail17 for a foreign dispatch. Nine deliberate
production-source variants also must compile then fail17. A compiler failure or crash is not a
behavioral RED. The old source/initial fixture failures remain separately archived.

```sh
python3 tests/native/sdl_mouse_bridge/run_controlled.py \
  --repo "$PWD" --sdl-source /absolute/pinned/SDL3-3.4.20 \
  --build /absolute/new/build --evidence /absolute/new/receipt --config Release
```

Debug and `--sanitizers` provide the controlled Mac ASan+UBSan gate. Fixture optimization is explicit
O2 Release/O0 Debug. The real provider inherits its existing target flags instead: current full-root
MSVCDefaults clears initial SDK Release optimization flags. Receipts preserve this no-O policy limit;
this component does not change it or claim optimized provider Release qualification.

`provider/CMakeLists.txt` uses the actual owned dependency boundary and its real generated
configuration/PCH. The production image's exports must equal upstream plus exactly the CSE entry.
The typed C import probe and C++ dynamic probe exercise the real scalar ABI. The genuine dynapi
control uses the existing upstream dummy video driver explicitly; no driver feature is changed and
no OS input/window/game behavior is claimed.

The fixture-only provider variant adds two clearly named local lifecycle seams and a distinct
output name. They call REAL initialization/teardown only in that fixture image. The host initializes
public A-to-B dispatch explicitly OUTSIDE the observed query, then initializes both local videos.
B independently returns Absolute; A must repeatedly return ForeignDispatch. A fresh-process preinit
query must leave valid B unloaded, while an explicit public call afterward loads it. Query-region
phase markers preserve chronological stdout/stderr. On Mac, loading two complete vendor images
emits duplicate Objective-C class warnings during that deliberate load; they are preserved, not
suppressed, and no Cocoa backend is exercised. This is C-state/dynapi evidence, not dual-Cocoa acceptance.

The runner archives separate production/fixture images and verifies production bytes stay unchanged.
Upstream dynapi intentionally retains its foreign loader reference until process exit; the host
releases its own references, shuts down both local video lifetimes, joins its worker and exits.
Windows builds require a measured per-profile budget plus4GiB reserve and record observed owned-byte
peak/free-space minima. No original runtime producer is modified.

```sh
python3 tests/native/sdl_mouse_bridge/run_provider.py \
  --repo "$PWD" --build /absolute/new/provider-build \
  --evidence /absolute/new/provider-receipt --baseline /absolute/pinned/upstream-library \
  --config Release
```

Engine sampling, UI retry, startup and Runtime10 consumers remain unwired in this component.
Provider ABI/source controls are separate from physical capture, cursor sampling and game acceptance.
