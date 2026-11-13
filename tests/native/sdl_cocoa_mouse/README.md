# Cocoa mouse provider boundary

The native64 Mac engine adapter is pure C++ and calls three fixed-width V1 C exports from the owned
SDL3.4.20 provider. AppKit/ARC stays in that already Objective-C provider. The same-image mode bridge must
admit the call before any AppKit access. Only Ready succeeds; relative focus requires actual Relative mode,
absolute position requires actual Absolute mode, and failed position reads preserve both outputs.
The native window is a borrowed genuine current SDL Cocoa property, not an arbitrary pointer. Serialize
SDL/window lifetime and invalidate the engine binding before destroying the window. No Runtime10 change,
public SDL header/dynapi modification, upstream patch, pointer layout or private-RVA assumption is involved.

The relocation preserves the frozen sampler's 21 service/helper bodies byte for byte. Its typed fake-service
controls live in `../native_mouse_window`. They cover transforms, focus, bad geometry, atomic outputs and
guard ordering, while this fixture executes real provider loading, guards and owned hidden-window binding.
`adapter_tests.cpp` checks every status (including unknown nonzero statuses), forwarding, high caller
addresses and canaries. A truthy-status adapter mutant must compile/link successfully and fail17.

The real boundary gate requires an ordinary production provider plus a separately named fixture variant
with only the two already documented local-init/quit seams. In the A→B case, a public call explicitly
initializes genuine foreign dynapi dispatch outside the observed queries, then the fixture seam establishes
local A video readiness. Queries reject the foreign dispatch before touching poisoned AppKit pointers.
Preinit separately proves no dispatch initialization. Main-thread, worker, post-quit and wrong-mode guards
are exercised through actual compiled C exports. Both streams are merged before marker checks; upstream
duplicate Objective-C class diagnostics from loading two SDL images remain outside the query interval.
The foreign case uses vendor dummy video, not real Cocoa window objects or live cursor reads.

`cocoa_module_host` holds SDL resident while it loads a C++ consumer containing the actual window owner and
adapter. Each of three epochs creates an owned hidden SDL window, binds it, invalidates the binding, destroys
the window, verifies its ID is gone, and quits VIDEO before closing the consumer. Acceptance requires its C++
destructor, dyld remove notification, absent image list, unmapped former header and balanced RTLD_NOLOAD miss.
The old Objective-C consumer must fail the same gate17. Image headers, sections, dependencies and exports are
archived separately; neither consumer uses RTLD_NODELETE or an alternative Mach-O file type.

The prerequisite hidden-window probe uses the frozen sampler and records only owned admission/geometry
and synthetic point conversions before Show. Normal unchanged SDL VIDEO initialization is allowed, including
its internal cursor baseline; the probe makes no explicit live cursor read or input capture and does not log
that baseline. The lifecycle gate likewise performs no explicit position query or window Show.

Build the production and separately named variant with `../sdl_mouse_bridge/provider`, toggling
`CSE_BRIDGE_FIXTURE_LOCAL_SEAMS=OFF`/`ON` in distinct build directories. Then run:

```sh
python3 tests/native/sdl_cocoa_mouse/run_boundary.py \
  --repo "$PWD" --sdl-include /absolute/pinned/SDL3-3.4.20/include \
  --provider /absolute/production/libSDL3.0.dylib \
  --variant /absolute/fixture/libSDL3_cse_fixture.0.dylib \
  --frozen-sdk /absolute/frozen/src --config Release \
  --build /absolute/owned/build --evidence /absolute/new/evidence
```

Repeat with Debug. The provider source has explicit private Objective-C++14, ARC, NoEH/unwind, precise FP,
disabled contraction, PCH skip and hidden internals. The engine follows its retained C++17 owning policy.
The tested Apple Silicon OS/image flags and unload outcomes are evidence for this leaf component. Published
dyld residency policy alone does not identify the installed dyld revision. Full hw/sw image unload, physical
AppKit/Win32 sampling, relative camera semantics and actual game/window behavior require separate acceptance.
The Win64 backend and legacy Win32 provider selection remain unchanged.
