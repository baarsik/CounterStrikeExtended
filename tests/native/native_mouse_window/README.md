# Native mouse sampler component

This fixture executes the actual private window/transition implementation, real current UI post-selection
blocks and lifecycle hook blocks through injected services. It links no SDL provider and creates no OS
window or game. Windows declarations/default bindings come from the real Windows SDK; AppKit declarations
/default thunks compile against the real SDK. AppKit executions use NSObject tokens plus typed injected
services; they never construct NSApplication, NSWindow or NSView.

The native64 sampler uses the required SDL3.4.20+CSE scalar mode bridge. Absolute reads use checked native
window position; relative reads require the owned SDL mouse-focus window and retain non-consuming cached
SDL position/recenter semantics. A single outer logical extent feeds platform validation and unchanged
float/truncation then double-aspect/truncation. Failed reads commit no outputs. Bind caches borrowed
SDL/native identity at hidden creation, checks explicit false mode plus actual Absolute, and invalidates
before teardown. The small transaction keeps per-UI pending intent, suppresses reentry and prevents a stale
outer handoff from committing a newer request's raw cache. No source/history authority is inferred from
the public relative flag.

The archived original helper produces a behavioral RED17 for fresh OS360,264 versus cached320,240.
Both original actual caller tails separately produce RED17 for stale raw-cache commit after failed enable.
Current caller controls exercise retry, no-change ownership and newer requests during pump/drain.
Startup failure is tested in a child process with exact noreturn signature/expected exit21; no setjmp or
exception-policy relaxation is used. Lifecycle blocks preserve font-retirement-before-invalidation.
Standalone fixtures use C++14 and explicit O2 Release/O0 Debug, precise FP/no contraction and NoEH/unwind;
actual owning command receipts separately record inherited C++17 engine policy and source-private provider ARC.
The current Cocoa service body is compiled from the owned SDL provider source, with its real generated SDK
configuration. The engine adapter is pure C++; the tests still inject the AppKit reads and mode bridge.

```sh
python3 tests/native/native_mouse_window/run_component.py \
  --repo "$PWD" --sdl-include /absolute/pinned/SDL3-3.4.20/include \
  --sdl-config /absolute/provider/upstream/include-config-release/build_config \
  --build /absolute/owned/build --evidence /absolute/new/receipt --config Release
```

Debug supports `--sanitizers`. `run_mutants.py` requires every mutant to configure/build0 then fail17;
a compiler error or crash is not a behavioral RED. The maintained coordinate fixture retains all previous
numeric/failure assertions and adds controlled private-sample readiness/failure controls. Its injected
sample is separate from the actual binding/platform routing controls here.

Physical Windows/AppKit sampling and real camera/game behavior remain unqualified. The original sealed
component's engine Objective-C translation unit remains preserved as a negative residency diagnostic.
The current implementation moves those exact service bodies behind three C exports in SDL; see
`../sdl_cocoa_mouse/README.md` for the distinct real hidden-window/provider/unload gate. That gate qualifies
its small C++ consumer, not full engine unload. Runtime10 and legacy Win32 selected behavior remain unchanged.
