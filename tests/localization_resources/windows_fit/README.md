# Supplementary Windows x86 font fit check

This fixture measures actual installed Windows fonts through GDI. It complements the native VGUI control/render fixture; it does not run the full game or replace VGUI layout and screenshot acceptance.

From the repository root on macOS with the authorized Parallels VM running:

```sh
python3 tests/localization_resources/windows_fit/run_vm.py \
  --game-root "$HOME/Library/Application Support/Steam/steamapps/common/Half-Life" \
  --output build/windows-translation-fit
```

The runner emits current inputs, transfers a small ZIP over stdin, builds the utility with MSVC `vcvarsall x86`, and reads the report back. VM writes stay in `C:\CI\CSE-BundledTranslations-20261113\windows-fit-v1`. It changes neither VM configuration nor Steam settings. Use `--vm-directory` with another `windows-fit-*` name under that same authorized test area for a separate run. The executable has a compile-time x86 assertion.

For an existing Windows checkout, input emission and compilation can also be run separately:

```powershell
python tests/localization_resources/windows_fit/emit_inputs.py --game-root 'C:\path\to\Half-Life' --output input.tsv
# In an x86 MSVC command prompt:
cl /nologo /EHsc /std:c++17 /W4 /utf-8 tests/localization_resources/windows_fit/font_fit.cpp /Fe:font_fit.exe /link gdi32.lib
.\font_fit.exe input.tsv fit.tsv
```

The emitter reads every current Russian and Polish token family, all retained constraints in `fit_controls.json`, and current owning `.res` files under `src/deploy/` when present. It selects the installed CS `ClientScheme.res` and platform `TrackerScheme.res` definitions for a **640×480 game screen**. At that resolution, proportional geometry uses factor 1.0. Definitions with `yres` choose their 480p interval; Russian font-height adjustment mirrors `Scheme.cpp`. Unknown font aliases fall back to `Default`, matching the label's zero-font fallback. The emitted provenance file hashes schemes, catalogue sources, layouts and constraint metadata.

`font_fit.cpp` uses positive cell-height `CreateFontW`, the same charset/quality settings as the Win32 branch of `CLinuxFont`, `GetTextMetricsW`, and per-character `GetCharABCWidthsW`. `EnumFontFamiliesExW` chooses the first installed scheme candidate. The report records the actual selected face and line height. `GetGlyphIndicesW` verifies direct glyph coverage for each measured string and for the complete current catalogue alphabet across Default, DefaultSmall and DefaultVerySmall in both schemes. No fallback glyph is silently accepted as coverage.

Button content includes the source's six-pixel horizontal inset. Checkboxes add the installed empty-box icon dimensions; radios use their source-defined 20×13 indicator. Explicit newlines and accelerator handling (`&X`, `&&`) follow `TextImage::GetTextSize`. Automatic line-break positions mirror `TextImage::RecalculateNewLinePositions`, including its current break placement, rather than substituting `DrawText` word wrapping. The fixture measures full content, including text that an actual control might otherwise truncate with an ellipsis.

Each language also includes a deliberately oversized label. The executable requires both negative cases to overflow and returns nonzero for a normal control overflow, missing glyph, missing font or infrastructure error. Outputs are `fit.tsv`, `overflows.json`, `summary.json` and the exact input provenance.

Limits: this check covers static retained control constraints with Windows pixel metrics. It does not instantiate Windows VGUI controls, perform dynamic tab/list/browser layout, verify clipping/painting, choose game resolution, or exercise gameplay. Current native VGUI tests own those behaviors. In particular, passing an embedded 640×480 root to the macOS fixture does not force its proportional scale: `SurfaceSDL::GetScreenSize` reads primary-display dimensions. A desktop-height-dependent native sample must not be reported as a 480p screen test. Stock weapon or faction names may already exceed an unscaled stock control at 480p; this fixture reports those faithfully rather than classifying every overflow as a new translation regression.
