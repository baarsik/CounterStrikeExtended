# T2a legacy dispatch CPU fixture

This slice tests private renderer dispatch ownership for `gl_rmain`, `r_triangle`, and `glHud`.
The first frozen checkpoint intentionally includes the absent production `legacy_dispatch.h`.
A missing-header compile RED is API evidence only. It does not qualify the eight behavioral
cases. No fixture replacement or configurable fake baseline is permitted. The next checkpoint
must compile the actual minimum production dispatch and demonstrate a genuine missing-route
behavior failure with these assertions unchanged before the complete implementation goes GREEN.

The fixture defines the actual qgl linkage slots and exact-typed CPU driver spies. It links the
production CPU state/texture/frame code, and will link production dispatch once that file exists.
No OpenGL, SDL runtime, GPU, window, or device is linked or invoked. Expected event sequences are
spelled independently from production wrapper generation; pointer identity and numeric arguments
are checked in addition to order. `CaptureOnly` is private fixture use, not production admission.

Eight named cases: lifecycle, mixed-owner, matrices-queries, shadow-failure, capture-refusal,
refusal-gate, extension-aliases, hud-trace. `audit_calls.py` is a separate bounded source check:
three owners, qgl install/removal hooks, and four released multitexture aliases. It is explicitly
not the final whole-engine T2d inventory audit. Texture/list/array/FBO/shader/media work is deferred.

Root must first run actual Windows x64 Debug/Release RED and GREEN. Only after explicit Root release
may Mac ARM64 Debug/Release and meaningful sanitizer configurations run. Preserve existing policy:
C++14; MSVC /W4 /WX /EHsc /fp:precise, /MDd or /MD, /Od or /O2; Apple Clang -ffp-contract=off.
No global compiler policy changes. Do not execute these commands before the relevant lane release:

```text
cmake -S tests/native/render_dispatch -B build/render-dispatch-x64-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCSE_TARGET_ARCH=x64 -DCSE_RENDER_GL_ORACLE=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/render-dispatch-x64-debug --parallel 1
ctest --test-dir build/render-dispatch-x64-debug --verbose --output-on-failure --parallel 1
python tests/native/render_dispatch/audit_calls.py --root .
python tests/native/render_dispatch/audit_calls.py --self-test
```

Use a separate Release build directory. The artifact-only Root packet pins actual tools and
protected state, preserves every raw failure, and owns finite runner/policy checks; this README
does not authorize a VM or device action. The separately reviewed `real-gl-run-packet.md` defines
Root's required same-frame ReferenceGL/ShadowCapture comparison; CPU traces are not pixel evidence.

Proposed private dispatch seam consumed by the tests (not public engine ABI):
`InstallLegacyDispatch(DispatchMode, FrameRecorder*, std::string&)`, `RemoveLegacyDispatch()`,
`LegacyDispatchStatus()`, `RouteLegacyExtension(const char*, LegacyProcedure)`, and exact APIENTRY
`CseRender*` wrappers. `LegacyProcedure` is `void (APIENTRY*)()`; extension absence stays null.
Repeated installation preserves original driver ownership and must not capture a wrapper as its
own reference. Removal is idempotent and restores the most recently probed original/null alias.
`FrameRecorder::Refuse(Status)` must retain first failure, reject Ok, and recover only after Abort.

## T2b RED preparation (not executed)

The additive `t2b_slots.inc` and `t2b_cases.inc` retain the old fixture bodies and expectations.
Fourteen new cases cover exact typed ownership/forwarding, mixed texture calls, explicit sampler
maps, RGB/BGRA unpack ownership, source-shaped glyph/lightmap calls, draw/list-time array copying,
array rejection, raw-list facade capture, immediate list state, list rejection, generated-name
refusal, late texture aliases, bounded failure forwarding, and state/no-frame behavior.
The five-owner raw-call and alias gaps are checked separately by `t2b_audit.py` against the frozen
518-entry lexical provenance ledger. The ledger is not a fixture oracle or a runtime call count.
Its 29 shader entries and one diagnostic resolver remain named T2d deferrals.

Current production additions are exact-typed forwarding/refusal scaffolding only. All new capture
routes refuse explicitly; the existing TexParameterf capture remains deferred. Producer files and
texture alias handoffs remain unchanged so Root can observe genuine missing-route behavior.
Generated names remain explicitly unsupported in capture under locked R2/R3; ReferenceGL and
ShadowCapture retain the actual driver's full-width outputs/returns, including zero.
R6 limits capture to float2 UV/XY, interleaved four-float stride, GL_QUADS, first zero. Negative,
unsupported, null-positive, checked extent-overflow and quota rows have independent refusals.
The high-address case fails unless the actual source pointer is above UINT32_MAX on a 64-bit run.

Prepared checkpoints are under Source `build/production-render-dispatch-2b-red-preparation-v1/checkpoint/`:
`preimages/`, `api-absence/`, and `missing-route/`. The API-absence checkpoint is DATA evidence
and was not compiled; the missing-route checkpoint is intended to link for actual behavioral RED. Nothing in this
preparation constitutes an executed C++ test, successful build, GPU evidence, or lane release.
Root owns exact Windows x64 Debug/Release commands and receipts. Use distinct fresh directories:

```text
cmake -S tests/native/render_dispatch -B build/render-dispatch-t2b-red-x64-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCSE_TARGET_ARCH=x64 -DCSE_RENDER_GL_ORACLE=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/render-dispatch-t2b-red-x64-debug --target render_dispatch_tests --parallel 1
ctest --test-dir build/render-dispatch-t2b-red-x64-debug --verbose --output-on-failure --parallel 1
cmake -S tests/native/render_dispatch -B build/render-dispatch-t2b-red-x64-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=x64 -DCSE_RENDER_GL_ORACLE=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/render-dispatch-t2b-red-x64-release --target render_dispatch_tests --parallel 1
ctest --test-dir build/render-dispatch-t2b-red-x64-release --verbose --output-on-failure --parallel 1
python tests/native/render_dispatch/t2b_audit.py --root .
python tests/native/render_dispatch/t2b_audit.py --root . --self-test
```

The current audit is expected to fail for eleven raw calls and two aliases. At minimum, actual
behavioral RED must name `t2b-mixed-texture: raw bind did not capture full-width name on selected unit`,
`t2b-list-routing: raw NewList/EndList facade did not capture qgl geometry`, and
`t2b-unpack-owned: upload/raw-unpack routes cannot seal two owned draws`. Build failure or a
fixture-only failure is not a substitute. Direct facade cases do not compile the producer owners;
use the separate source audit for that coverage boundary. No Mac/native/GL/compiler action is
released to the writer, no production backend changes, and no generated project changes are needed.

## T2b GREEN preparation

The preceding section records the frozen RED scaffold. Root subsequently verified its actual Windows
x64 Debug/Release result: twelve exact missing-route failures and twelve ordinary passes per configuration.
Current Source implements the released texture/list/array capture and raw/alias routes; GREEN remains
uncompiled and unqualified until Root runs the fresh packet. Generated texture/list names still refuse
capture while forwarding actual driver results. No T3 initialization/session or T5 GPU parity is added.

Uploads use the approved allocation-free `TextureStore::UploadByteExtent` seam, sharing existing layout
arithmetic; invalid/over-limit results leave its output untouched. Arrays read configured per-draw/frame
limits before allocation or caller reads, retain only live descriptors, and copy UV/XY at draw/list compile.
Owned allocations catch bad_alloc/length_error before Shadow's unchanged driver call. The source-shaped
shared RGB palette is copied, with an independent retained-byte assertion. The old test bodies and named
RED expectations remain; only the approved Fog index style change and focused extent/palette checks are added.

Root's fresh Windows GREEN packet must require all 24 dispatch cases and all 21 unchanged state/texture/frame
cases in each configuration: 90 ordinary passes, no skips/errors/abnormal exits. The frame ownership case
still requires actual high addresses and allocation-failure case still requires its exact hook-hit output.
The standalone state fixture must explicitly set CSE_RENDER_GL_ORACLE=OFF. No Mac/compiler/native/GL release
is implied by source/data checks. The bounded source audit now expects zero errors; its injected raw-call
negative remains source DATA only.
