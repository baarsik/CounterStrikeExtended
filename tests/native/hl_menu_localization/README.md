# HL command-menu title expansion regression

```sh
python3 tests/native/hl_menu_localization/run.py \
  --root . --evidence build/hl-menu-localization-green
```

This ASan/UBSan fixture compiles the actual `LocaliseTextString`, actual
`CreateCommandMenu` label/error scratch declarations and copy statements, and
actual `CommandButton::setText`, `RecalculateText` and `setBoundKey`. Title lookup
is the controlled external API; its complete Russian/Polish values come from
all 15 identifiers referenced by each language's actual HL spectator menu files.
It checks full equality through localization, the menu scratch, error label,
private button storage and final key-prefixed display. The receiver verifies
that display uses the existing length-aware literal Label overload, avoiding
its variadic formatter and fixed 4096-byte scratch.

The initial RED run produced an ASan stack overflow in the actual Russian
expansion into `cText[32]`. Polish failed full-button-text equality because of
the separate private 32-byte display/storage limit. The original binary and
source extraction remain in `build/hl-menu-localization-red`; replay against
the current shortened Russian labels still triggers the overflow.

Bounded cases cover short-destination canaries, every byte boundary of Cyrillic
and Polish strings, exact fit and NUL, zero/negative capacity, null input/output,
empty strings, in-place expansion, adjacent tokens, nonrecursive title bodies,
unknown/300-byte identifiers, invalid UTF8/CP1252 bytes and a complete four-byte
Unicode scalar. A 5000-byte private button label proves that replacing the old
32-byte cutoff did not introduce another private fixed-buffer cutoff.

Menu scratch/error buffers share a named 1024-byte capacity, matching the existing
buffered localization API. Actual retained menu labels are asserted untruncated;
other callers of the localization helper receive a bounded complete UTF8 prefix.
Non-UTF8 output retains byte semantics. `MAX_BUTTON_SIZE` and public engine/VGUI
object layouts and interface slots remain unchanged. The private client-only
CommandButton stores its original label dynamically.

## Actual font metrics

With the existing `tests/native/vgui1_utf8` fixture already built:

```sh
python3 tests/native/hl_menu_localization/font_fit.py \
  --root . --evidence build/hl-menu-localization-green \
  --provider-build build/vgui1-utf8
python3 tests/native/hl_menu_localization/bitmap_fit.py \
  --root . --evidence build/hl-menu-localization-green \
  --provider-build build/vgui1-utf8 \
  --bitmap '/path/to/Half-Life/valve/gfx/vgui/fonts/640_CommandMenu Text.tga'
```

The first compiles actual VGUI1 `Font::getTextSize` with the real native Arial
face at the retained 640 scheme's 14px/weight0 setting. The second links actual
Font/platform/font-service owners from the provider fixture and loads the actual
retained bitmap font (the default `bitmapfonts=1` path; observed cell height12),
with real native Arial Unicode fallback. All 30 current bound-key-prefixed menu
labels fit their nominal 160px options / 200px camera buttons with a conservative
4px border allowance. Native measurement exposed the original Russian mode1
label at212px; the current clear abbreviated label measures153px (bitmap146px)
and remains longer than32 UTF8 bytes, so the memory regression remains exercised.

The scripts write per-language fit TSV files. They validate actual font widths,
not a game screenshot or Windows GDI runtime. The owning `hl_client` target is a
separate compilation/link check.
