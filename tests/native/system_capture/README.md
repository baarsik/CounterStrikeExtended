# Cocoa interaction ownership query

Task 1 executes the complete owned `sdl3_native_cocoa_mouse.mm` body through typed
AppKit reads and the existing mode-bridge seam. It creates NSObject identity
tokens only, never NSApplication/NSWindow/NSView, audio devices or a game.
Both mode runs cover active/key foreign ownership, own/no/foreign recipients,
pass-through hit-test output, inactive/key/minimized states, geometry and every
read-service/bridge guard. Callback counters and unlinked input mutation APIs
bound the permitted operations; weak token checks cover pointer retention.
The old sampler/service structure and cursor-policy implementation remain intact.
Public C compilation verifies the additive result and all existing numeric values.

Configure with the qualified SDL cache, then build with at most two compilers:

```sh
cmake -S tests/native/system_capture -B /absolute/fresh/fixture -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_SDL_INCLUDE=/absolute/qualified/SDL3-3.4.20/include \
  -DCSE_SDL_CONFIG=/absolute/qualified/include-config-release/build_config
cmake --build /absolute/fresh/fixture --parallel 2
ctest --test-dir /absolute/fresh/fixture --output-on-failure
```

Repeat Debug with its actual generated config; `CSE_CAPTURE_SANITIZERS=ON` enables
ASan/UBSan. For the original-source RED, freeze all three Cocoa service files from
the pre-change revision into one directory and set `CSE_CAPTURE_PROVIDER_SOURCE`
to that complete `.mm`, plus `CSE_CAPTURE_ORIGINAL_BOUNDARY=ON`. Run the resulting
`interaction_tests` once without arguments and once with `--relative`; both must
compile/link successfully and exit17 with `result=1 expected=8`. This deliberately
demonstrates the original identity-only API's missing interaction distinction.
It does not assert that the new API existed or alter the original API's contract.
The RED selection is fixture-only; no production fallback/detector model exists.

Build immutable full SDL images from current qualified runtime caches:

```sh
python3 tests/native/system_capture/build_provider_packet.py \
  --repo "$PWD" --cache /absolute/runtime-macos-arm64-release \
  --packet /absolute/fresh/provider-release --config Release
```

The script reuses the exact retained compile/link recipes, replaces only the
owned Cocoa object, and full-links SDL. It hashes all unchanged input objects,
their dependency closure, all upstream files against the source archive, and
source/image/probe bytes. Each invocation compiles one process at a time. Repeat
with the qualified Debug cache and `--config Debug`. It changes no cache/app and
does not launch its output. Keep each probe beside its matching SDL image; its
rpath is `@executable_path` and SDL retains `@rpath/libSDL3.0.dylib` identity.

The built `provider_probe [--absolute|--relative]` initializes **VIDEO only** and
creates a standalone window for the controller's Cmd+Shift+4 signal check. Its
60-second lifetime can also end by closing the window. It logs elapsed nanoseconds,
actual mode and the new result at most once per 10ms, with no catch-up burst.
Relative mode is requested once at startup; this probe does not implement engine
capture suspension. It may therefore retain capture during selection. It never
initializes audio, opens recording devices, requests monitoring/capture permission,
installs an event tap, or modifies a game app. The query itself never pumps,
drains, changes presentation or warps; the standalone probe runs its own normal
SDL event loop. Runtime signal/camera acceptance remains with the controller.

Injected `hit=OwnedNumber` represents AppKit skipping a pass-through
`ignoresMouseEvents` window; it is a service-contract control, not a real WindowServer
transparency test. Actual overlay and transparent-panel behavior remain runtime
qualification. Existing native_mouse_window fixtures provide the old sampler,
UI owner, lifecycle, recenter and pinned Cocoa-delta regressions. The live
mac_cursor fixture can be built against each new packet and run by the controller.
