# Counter-Strike timer colon fixture

The extractor compiles the complete production round/career `Draw`, `Reset` and
`VidInit` bodies, their actual class declarations, `CHudMessage::VidInit`, the
actual `_font` declaration and HUD metric assignment, `ScaleColors`,
`CHud::DrawHudString`, and the engine credits-font selection and character/string
callbacks. It records a SHA-256 manifest for the owning sources.

Named external seams supply the scheme, glyph ABC widths, surface draw recording,
number/stopwatch sprites, viewport and career eligibility. Font IDs and glyph
widths are controlled fixture values, not runtime measurements. The unchanged
production generic string path demonstrates the original mismatch; the fixed
path must draw using the same handle that supplied layout metrics. No production
timer decision is duplicated in the fixture.

```sh
cmake -S tests/native/hud_timer -B build/hud-timer-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/hud-timer-debug --parallel 2
ctest --test-dir build/hud-timer-debug --output-on-failure
```

Repeat in a separate Release directory. Add `-DCSE_HUD_TIMER_SANITIZERS=ON` for
Clang/GCC ASan+UBSan. The fixture also builds with MSVC; the actual client owning
compile and link remain separate gates.

The eleven cases cover round/career colon font identity and baseline at 480p,
1080p and 1440p; absolute x plus callback-width accounting and closest-right;
hide/spectator/bomb and career eligibility exits; expired countdown and panic
colors/cadence; career completion/fade; font reinitialization and unchanged generic
credits drawing. Both suit and no-suit vertical anchors are checked.

This is behavioral source evidence. It does not establish actual font raster
appearance, live loaded scheme selection, or 1440p visual acceptance.

On MSVC, the generated `scale_colors.inc` preserves the exact legacy body and
the owning `cl_util.h` warning 4244/4305 pragmas, scoped with push/pop around only
that body. New fixture code retains `/W4 /WX`. A twelfth, Windows-only compiler
control compiles that include successfully, then requires C4244 when narrowing
in a new function after the include. Its commands/output are retained under the
build's `msvc-diagnostic-boundary/`; no compiler diagnostic policy is waived for
the new function or the rest of the fixture.
