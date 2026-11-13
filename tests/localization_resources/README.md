# Bundled translation checks

The portable catalogue/generator/CMake suite uses `python3 -m unittest discover -s tests/localization_resources -v`.
The CMake project optionally checks the C++ disk decoder and actual VGUI loader; see [native tests](../native/README.md).

## Actual retained-control fit and rendered samples

Build the production macOS runtime first, then create the component from its owning commands:

```sh
python3 tests/native/server_browser_ui/build_vgui_component.py --repo . \
  --packet build/translation-fit-component --profile release --driver translation_fit \
  --recipe-cache build/runtime-macos-arm64-release \
  --sdl-library build/runtime-macos-arm64-release/runtime/Release/libSDL3.0.dylib
python3 tests/localization_resources/run_fit.py \
  --runtime build/runtime-macos-arm64-release/runtime/Release \
  --platform /absolute/official/platform --game /absolute/official/Half-Life \
  --packet build/translation-fit-component --output build/translation-fit-results
```

Use fresh packet/output directories. The compilation database must contain the current owning GameUI recipes;
`ninja -t compdb` in that build directory supplies all owning commands when a provider-only CMake export is incomplete.
The fixture loads the actual filesystem/VGUI providers, schemes, all ten-language resources, real retained controls
and the production server-browser component. It injects the existing engine geometry methods at 640x480, forwarding
font creation, glyph coverage, ABC widths, wrapping and raster drawing to the actual providers. This prevents a
768px host desktop from silently enlarging proportional CS budgets.

`fit_controls.json` pins617 distinct stock/control constraints and original source hashes. Owned admin controls reload
the actual maintained layouts. Measurements include real image/text insets; image-only headers are excluded. The
actual minimum640x384browser adds full tab-strip edge checks and its expanded filters. An intentionally oversized
button must be rejected. The active page and visibility of its expanded controls are asserted. The
existing unbounded game/map/filter preview may intentionally elide; its draw
width is checked and reported separately. Every translated fixed control must
fit completely. The report records every width/height and complete text; SDL captures actual BMP samples,
and the runner converts them to PNG on macOS. Glyph checks cover each loaded alphabet through actual face/fallback
selection in both Tracker and Client scheme font roles. This proves these inputs, not every dynamic server/player
string or a full game session.

## Windows font budgets

[The Windows x86 GDI fixture](windows_fit/README.md) independently checks all617 stock budgets against actual
Tahoma/Verdana metrics at 480p, including glyph indices, accelerator handling, wrapping and negative controls.
It supplements the native control/raster fixture; it does not execute complete Windows game widgets.
