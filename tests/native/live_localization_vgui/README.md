# Live VGUI localization provider

`runtime_tests.cpp` is the root-owned actual-widget integration driver. It checks
labels, literal wide strings, text-entry preservation and repeated language
refresh with the real control implementations.

`provider_tests.cpp` isolates the actual library provider from widget refresh. Run
it with a previously built admitted control fixture packet:

```sh
python3 tests/native/live_localization_vgui/run_provider.py \
  --packet build/live-localization/vgui-red \
  --evidence build/live-localization/provider-actual-final \
  --vgui build/runtime-macos-arm64-release/runtime/Release/vgui2.dylib \
  --filesystem build/runtime-macos-arm64-release/runtime/Release/filesystem_stdio.dylib \
  --platform src/deploy/platform --sanitizers
```

The helper recompiles only this driver with its retained owning compile command
and relinks the admitted fixture objects. `--sanitizers` instruments the driver;
it does not rebuild or instrument the supplied real provider libraries. Logs and
exact compile/link/run commands are retained in the evidence directory.

Provider assertions cover nonmutating prepare, discard, failed malformed target
rollback, successful-only logical registration, late registration, English
fallback, hidden old-language-only tokens, stable IDs and borrowed raw pointers,
interned values after forty switches, explicit programmatic overrides versus
server-browser emergency fallback, editor changes, invalid metadata indices,
UTF16 save/reload, formatting, stable HFont identity and Russian/English font
metric changes, and the engine font-reset callback.

`catalogue_tests.cpp` exercises the production bounded UTF16/parser and stable
storage directly under ASan/UBSan. It rejects malformed structure, invalid UTF16,
odd byte counts and trailing garbage; accepts empty values and safely handles
values beyond the previous stack token limit. Pass disk catalogue filenames to
also validate real staged inputs:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I src/vgui2/src tests/native/live_localization_vgui/catalogue_tests.cpp \
  -o build/live-localization/catalogue-tests
```

These fixtures do not prove a complete game's active-match presentation, physical
GPU rendering, native Windows behavior, or final app packaging.

## Label layout revision regression

`label_layout_tests.cpp` checks the actual Label/Button layout after a short token
becomes longer while its 236px control stays unchanged. It deliberately reads
translated text before Think so the TextImage revision has already been consumed.
The label must independently update drawWidth and localized hotkey, preserve a
literal label, button selection/visibility, and dispatch no action until a later
user click. `draw-widths.tsv` records old/current/needed widths.

```sh
python3 tests/native/server_browser_ui/build_vgui_component.py --repo . \
  --packet build/live-localization/label-layout-shared-green --profile release \
  --recipe-cache build/runtime-macos-arm64-release \
  --sdl-library build/runtime-macos-arm64-release/runtime/Release/libSDL3.0.dylib \
  --driver label_layout --label-variant shared --sanitizers
```

Repeat with a new packet and `--label-variant cs` to compile the actual private CS
Label.cpp with its cs_client owning flags; remaining control/provider dependencies
are real shared implementations, rather than mocked layout code. Invoke each
packet's `vgui_tests <vgui2.dylib> <filesystem_stdio.dylib> <platform> <output>`.
The full client-specific native game acceptance remains separate.

`check_control_abi.py` and `check_control_abi.py --cs` compare complete HEAD header
closures, including HEAD Label.h, to prevent current-header contamination through
relative Button/EditablePanel includes. They probe Label size/alignment and its
full inherited virtual table as well as Frame/ComboBox/RichText; only existing
OnThink targets may differ. `overrides.json` records those target changes.

## Degenerate scrollbar geometry

```sh
python3 tests/native/live_localization_vgui/run_scrollbar.py \
  --evidence build/live-localization/scrollbar-geometry-green-final
```

This extracts the actual shared and CS private forward/inverse scrollbar mapping
methods unchanged, controlling only panel dimensions/repaint. Each copy runs
5,040 range/window/value/size cases under ASan/UBSan and float-cast-overflow with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Coverage includes equal/empty/
inverted ranges, zero/negative/oversized windows, zero/tiny/negative tracks,
integer extremes and both orientations. Exact ordinary thumb endpoints and
nonzero-origin value mapping are checked, as is retaining a valid logical scroll
position when a tiny track cannot represent movement. `results.json` records
source hashes, compilation commands and exits.

A full actual-widget reproduction is retained in
`build/live-localization/widgets-extended-final/scrollbar-strict-red.log`; it stops
at the former NaN→int conversion through RichText layout/SetValue rather than
accepting a printed PASS followed by teardown sanitizer errors.

The ABI checker compares both owning header views with pinned pre-localization
commit `538661e`, so the comparison remains meaningful after the implementation
is committed. `label_layout_tests.cpp` checks independent label revision tracking,
clipping width and hotkeys after an earlier getter refresh. `run_scrollbar.py --evidence <directory>` extracts both actual slider mapping owners and checks
ordinary and degenerate geometry with strict sanitizer failure handling. The
extended real-widget driver also covers deferred teardown; run it with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
