# Standalone VGUI2 qualification

This fixture loads the actual owning `vgui2.dylib` and `filesystem_stdio.dylib` and links the actual hlds controls objects. Configure the retained owning build first; it must use the source-built pinned SDL dependency, its original per-file policies, and export compilation commands. Build `hlds` far enough to compile the controls owners. The two Task 3 sources may still prevent the final hlds link.

Stage retained platform resources into a separate owned directory, including `resource/TrackerScheme.res`, the platform images and WAV files, and the retained `Admin` resource directory. When the platform scheme assumes Windows' Marlett symbol font, supply the existing game's `valve/resource/marlett.ttf` as `resource/marlett.ttf` in that isolated stage. Preserve source/copy hashes and the font's embedded license notice. No game assets are distributed by this test directory.

```sh
cmake -S tests/native/vgui2_standalone -B build/vgui2-standalone-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCSE_OWNER_BUILD="$PWD/build/dedicated-graph-debug" \
  -DCSE_RESOURCE_ROOT="$PWD/build/vgui2-standalone-assets"
cmake --build build/vgui2-standalone-debug
ctest --test-dir build/vgui2-standalone-debug --output-on-failure
```

Repeat with `Release` and the owning Release build. The automated tests set SDL dummy video/audio and software rendering. They do not set keyboard-repeat hints; the production video owner must disable the macOS accent picker before SDL video initialization. Fractional wheel events are accumulated per native window and hovered panel serial handle; focus loss/leave discards unfinished fractions. SDL `FLIPPED` deltas are inverted to the retained positive-up convention before accumulation. A real TextEntry/ScrollBar test covers fractional sequences, distinct windows, focus reset and both directions. The renderer lifetime test instruments the real StandaloneWindow/Raster sources with ASan and UBSan for 100 window/renderer/texture generations and density-two clip pixels. The ownership interposer injects failures at actual SDL allocation boundaries and calls the real SDL functions on successful paths. The glyph reference uses retained FontManager glyph coverage and the pinned SDL compositor directly, bypassing the surface's glyph cache. The physical ABI negative test recompiles actual producer/control source recipes with deliberately reordered real interface declarations and a changed Color record, then exercises the production schema extraction.

For controller-owned physical Cocoa acceptance, unset `SDL_VIDEODRIVER`, `SDL_RENDER_DRIVER` and `SDL_AUDIODRIVER`, then run:

```sh
build/vgui2-standalone-debug/control_tests \
  "$PWD/build/dedicated-graph-debug/runtime/Debug" \
  "$PWD/build/vgui2-standalone-assets" cocoa
```

The two retained Frames contain Unicode-enabled TextEntry, buttons, checkbox and ComboBox/Menu controls. The fixture prints native window geometry, SDL event identities/coordinates/modifiers, VGUI focus, TextEntry callbacks and changed text. Cocoa mode also reads each SDL window's documented native pointer and logs `isVisible`/`parentWindow`; it rejects a hidden SDL window that remains natively visible for ten frames. These are fixture diagnostics, not a new public HWND capability. It never polls SDL independently of the production surface. Exit with **Quit both windows**, close both retained Frames, or use the native application Quit action. A `PASS cocoa` exit establishes graceful cleanup only; it is not proof that physical interaction, clipboard, density, resize or focus passed. Capture those observations separately. Freeze the executable, complete runtime and resource stage before handing it to the controller.

Callback retirement tests use actual retained Panel subclasses whose resize, scheme or think callbacks delete themselves/their popup ancestor or replace the native window while preserving the panel. The `callback_lifetime_sanitizers` test recompiles SurfaceSDL, VPanel and the Window/Raster owners with their actual compile recipes plus ASan/UBSan, relinks the actual VGUI2 object graph in a separate test runtime, and instruments the control fixture. The unchanged retained controls objects and pinned dependencies remain linked. It records all recipes and sanitizer outputs under `callback-sanitizers`; no production or frozen Cocoa runtime is overwritten. The mouse-state test abandons a press, moves keyboard focus into a real TextEntry, retires/reopens a popup, and rejects synthetic releases. The modifier-order test holds Shift across frames, clicks before a queued Shift-up, and checks text callbacks around a windowless modifier release.

`cursor_ownership` exercises the actual standalone surface through a cursor boundary observer.
The CTest entry explicitly uses `CSE_CURSOR_SIMULATED=1` with the dummy driver because SDL's
headless backend cannot create native cursors. This deterministic ownership test is not native
Cocoa qualification. It covers same-shape reuse, shape changes and aliases, hide/show/lock,
creation/selection failures, external cursor selection, focus/capture changes, and shutdown/reopen.

For actual Cocoa qualification the controller runs `control_tests <runtime> <resources>
cursor_ownership` with `DYLD_INSERT_LIBRARIES=<fixture-build>/libcursor_observer.dylib` and
`CSE_CURSOR_SIMULATED`, `SDL_VIDEODRIVER`, `SDL_AUDIODRIVER`, and `SDL_RENDER_DRIVER` unset.
The observer then forwards real SDL cursor operations (except the two deliberate failure cases),
requires the Cocoa driver, and prints `CURSOR_BACKEND ACTUAL Cocoa SDL`. The scenario exits
itself. Keep native log/exit evidence separately from dummy CTest output; neither cursor counts
nor synthetic focus transitions prove that every physical pointer/control flash is gone.
