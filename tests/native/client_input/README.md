This fixture executes complete mouse functions extracted byte-for-byte from the HL and CS `inputw32.cpp` owners. It also
extracts their mouse/cvar state, including CS's double raw-cache timestamp. `actual_bodies.json` records source/body hashes
and source locations. The retained Valve notices and licensing remain applicable to those source bodies; extraction does
not change their provenance.

The fixture uses the actual cvar, command, button and globals declarations and pinned SDL3.4.20 headers. Engine, HUD, VGUI
visibility, cvar storage and SDL calls are controlled seams. Joystick initialization/movement are explicitly counted seams;
this fixture does not test joystick behavior, engine/client ABI, a real SDL provider, OS cursor policy, physical input, focus,
third-person camera, UI or game behavior. No SDL library is linked and no window/device/provider is opened.

The executable tests the native non-Windows branch. On Windows it includes the SDK headers first, then undefines `_WIN32`
only inside this isolated fixture so MSVC can execute that same native branch. This is not Windows platform emulation.
Separate actual owning-TU compiles and before/after Windows preprocessing controls qualify the Windows-selected source.

Coverage includes CS Init `>0` versus timed `!=0`, strict `>1.0` with adjacent float times, null/rewound globals, live HL raw
state, fractional remainders, raw transitions, integer accumulation, filter history, activation/clear/shutdown, camera/UI/
intermission suppression, pitch/strafe/noclip/drift, five-button ordering, HL sensitivity clamp and CS acceleration/zoom.
High-address guarded caller buffers are required on 64-bit runs. Exact arithmetic checks use chosen representable values;
there is no epsilon-based claim of cross-hardware transcendental equivalence.

Configure and run from the repository root, pointing to the pinned SDK include directory:

```sh
cmake -S tests/native/client_input -B build/client-input-macos-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_SDL_INCLUDE=/absolute/path/to/SDL3-3.4.20/include -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/client-input-macos-release --parallel 2
ctest --test-dir build/client-input-macos-release --output-on-failure
```

Use explicit Debug or `-DCSE_CLIENT_INPUT_SANITIZERS=ON` in separate build directories. The standalone fixture explicitly
selects Release `-O2`/`/O2`, Debug `-O0 -g`/`/Od /Zi`, precise FP, and the owners' distinct EH policies (HL no EH, CS EH).
MSVC conversion-warning suppression is scoped around the unchanged extracted expressions, preserving their original
integer/double-to-float rounding points. Empty retained parameters and unrelated unused extracted state are excluded from
fixture-only warning errors. The actual owning production commands retain their original warning policies.

`run_negative_variants.py ROOT OUTPUT SDL_INCLUDE` mutates private source copies only. It requires runtime rejection of
CS nonzero Init, early refresh at exactly one second, positive-only refresh, lost HL filter history, lost raw-transition
remainder reset, and omitted CS zoom. It never edits production files or the original reference inputs.

The implementation packet and actual receipts are under the owned artifact prefix `native-client-input-chunk-a-v1`.
The first POINT/raw compile failures, declaration-only CS Init failure, Init-only CS refresh failure, and Windows-only SPI
storage warning failures are retained. The Windows compiler fixture's first conversion-warning failure is also retained.
Native camera/engine ABI work is a separate unimplemented Chunk B.
