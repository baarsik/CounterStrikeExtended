# Interface Options validation — 2026-11-13

Interface follows Video in the existing Options dialog. Its non-editable Language
list contains automatic Steam selection and all ten bundled languages by native
name. Apply/OK persist a CSE preference for the next restart; Cancel discards pending
changes. Explicit `-language` wins over the saved preference. All startup consumers
and the page use one strict native reader; malformed/unbundled stored values fall
back to automatic. The page retains pending selection and an error on failed writes.

## Observed verification

- `python3 -m unittest discover -s tests/localization_resources -p 'test_*.py'`:
  16 tests passed, including generated table order/Unicode, invalid metadata,
  catalogue parity and incremental offline CMake resource restoration.
- `python3 tests/native/interface_language/run.py --root . --evidence build/interface-options-language-verified`:
  actual ROOT/FALLBACK/surface and dedicated consumer agreement passed under ASan/UBSan.
- `python3 tests/native/interface_preferences/run.py --evidence build/interface-preferences-verified`:
  real macOS backend/reader passed using a disposable fixture domain; production
  Windows reader passed with controlled query-only APIs. Wrong type, missing
  terminators, embedded NULs, oversize values and CLI precedence are covered.
- The actual-controls `interface_options` driver passed for English, French,
  German, Italian, Spanish, Simplified Chinese, Traditional Chinese, Korean,
  Russian and Polish. Each runs CS, HL and single-player-only tab compositions
  with real 640×480 metrics installed before loading the Tracker scheme.
  It checks all tab/page bounds and translated text, non-editable selection,
  exact complete native dropdown labels/selection text, actual glyph coverage,
  keyboard selection, Apply/OK/Cancel/reset, unchanged writes, failed-save retry,
  automatic clearing, override notice and malformed-native-record repair.
  Final results and measurements: `build/interface-options-ui/results.json`
  and each language's `fit.tsv`; native raster samples are `options.bmp` and
  `languages.bmp`. Russian page, German action buttons and the complete multilingual
  dropdown were visually inspected.
- `cmake --build --preset runtime-macos-arm64-release --target gameui hw_engine swds hlds cs_client --parallel 2`:
  passed after the shared ComboBox Unicode correction. Subsequent GameUI build
  passed after adding its strict native reader and macOS backend linkage.
- Runtime resource verification passed all 478 CMake-selected dedicated paths
  and exact staged hashes. `git diff --check` passed.
- A fresh whole-change review and scoped correction review finished with no open
  material findings. The malformed page-record regression failed against the
  pre-fix production page and passed after using the shared strict reader.

## Evidence boundaries

The UI fixture compiles the production language page and all unchanged extracted
OptionsDialog methods with owning flags, actual shared controls and a source/body
receipt. Unrelated option pages, taskbar and mod metadata are controlled doubles.
Registry writes and strict-read simulation remain private to that fixture;
separate native tests validate actual record handling. No personal CSE or Steam
preference was changed. The actual installed Tracker scheme supplies assets/fonts;
translation catalogues come from the generated pack.

This validates the component and native macOS builds. No complete game session,
Windows SDK build or Windows VGUI raster/interaction run was performed for this
change. Existing broader translation verification remains recorded separately.
