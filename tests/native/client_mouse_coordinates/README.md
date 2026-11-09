UNWIRED component: this tests the actual additive `cse::TryGetEngineMousePositionAndCenter` helper. It does not add a
client callback, descriptor, camera implementation, cursor policy, relative-motion consumer or engine API-table entry.
The existing getter/warp/helper source bytes remain unchanged.

The helper snapshots current logical and render extents/aspects, reads cached absolute mouse state once, and commits four
outputs after both axes validate. It preserves float multiply/divide followed by int truncation, checked integer
center subtraction, then double aspect adjustment followed by int truncation. It also checks the final signed
position-minus-center range needed by camera callers. Missing window/mode/outputs, failed or invalid extents, nonfinite
positions/aspects and unrepresentable arithmetic fail without mutating caller outputs. This is a main-thread query.

The fixture includes the actual header and pinned SDL3.4.20 declarations. Every executed SDL/video function is an owned
fake. It links no SDL library, opens no window/device, does not pump events, and must not drain relative motion or warp.
The fake mode pointer is only a non-null sentinel; no virtual method is called. Foundational vec3_t matches the retained
engine declaration needed to parse its surrounding headers. These seams are not provider, ABI, device, focus or game proof.

Coverage includes windowed/fullscreen dimensions, 2D independent axes, desktop offsets, non-unit DPI, letterbox/pillarbox
aspect factors, odd extents and stale cached screen dimensions. A 625-case finite corpus compares results exactly against
the existing `WindowToRenderCoordinate`. One witness is x=30000.125, render8192/window3: the retained float stage gives
81920344; prematurely doing the scale in double gives 81920341. Conversion-edge tests include valid fractions just outside
int's endpoints whose truncations fit, signed center-subtraction overflow, NaN/infinity, float overflow, subnormals, failed
reads, partial fullscreen-mode writes, null mode with no SDL reads, all null-output positions, and high-address canaries.

The approved policy bound is precise FP with contraction disabled; fast-math is not qualified. All current including
hw/swds TUs and R/D commands are audited in the artifact packet. Current MSVC defaults preserve source ordering and
special-value behavior, and disable contractions under precise mode in VS2022 and later. See Microsoft's
[/fp documentation](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170).
The component does not change FP flags or measure production thread FP control state.

Configure with the pinned header location, using separate Release, Debug and sanitizer directories:

```sh
cmake -S tests/native/client_mouse_coordinates -B build/client-mouse-coordinates-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_SDL_INCLUDE=/absolute/path/to/SDL3-3.4.20/include -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/client-mouse-coordinates-release --parallel 2
ctest --test-dir build/client-mouse-coordinates-release --output-on-failure
```

Use Debug and `-DCSE_MOUSE_COORDINATES_SANITIZERS=ON` for sanitizers. Fixture compile flags explicitly select -O2 or
-O0/-g and /O2 or /Od /Zi, precise FP, no EH and RTTI. No root build metadata is changed.

`run_negative_variants.py ROOT OUTPUT SDL_INCLUDE` mutates private header copies only. Eight variants must fail at runtime:
no-op, desktop center, double read, partial write, unchecked camera difference, relative drain, double scaling and an
overstrict lower conversion bound. The missing-helper RED, an initial fixture boundary-expectation error and mutator
matching/expected-location errors are retained in `client-mouse-coordinates-v1` artifacts alongside final receipts.
