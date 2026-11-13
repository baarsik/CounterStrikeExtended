# Half-Life HUD title UTF8 fixture

This macOS fixture extracts the complete current `CHudMessage` scan, colour,
positioning and `Draw` bodies. Only the HUD environment and external glyph draw
callback are controlled. It loads the retained source-built `hw.dylib`, `vgui2`
and filesystem modules, gives the existing VGUI scheme service the **engine's**
`VGUI_Surface026`, and loads an actual retained `TrackerScheme.res`. Glyph widths,
height and Cyrillic/Polish glyph coverage come from that engine-owned font manager.
It does not initialize another controls system or open a game window.

```sh
python3 tests/native/hl_message_utf8/run.py \
  --root . --evidence build/hl-message-green \
  --runtime build/runtime-macos-arm64-release/runtime/Release \
  --resources /path/to/retained/valve --sanitize --font-mode both
```

`both` tests the retained `UseScaledCreditsFont=1` setting and a fixture copy with
that setting changed to zero, selecting `Legacy_CreditsFont`. Every other scheme
setting remains unchanged. The current retained 640x480 scheme produces 17px and
22px fonts respectively; the clipping expectations explicitly check those two
fixtures. Pass another evidence directory to preserve an earlier run.

The ten scenarios cover Russian and Polish codepoints, all three effects,
highlight/normal glyph advance and clipping, effect2 reveal/expiry, final-line
width and right alignment, trailing LF, no final LF, an empty string, a 90-character
ASCII line, oversized multiline blocks, unchanged unsigned CP1252/malformed UTF8
fallback, and service availability changes between scans. Source and provider
hashes, compiler command, per-case logs and exit codes are written to the evidence
directory. ASan/UBSan instrument the extracted client scan and private helper;
the existing engine/VGUI modules are loaded as built.

For behavioral RED comparison, pass an explicitly pinned pre-fix `message.cpp`
with `--source`, a selected `--cases` list, and `--expect-failure`. The fixture
must compile before expected scenario failures count as RED.

This validates glyph calls and actual font metrics/coverage, not rendered pixels
or whole-game behavior. Windows owning-target/runtime validation remains separate.
The native retained scheme may log unavailable Marlett symbol-font warnings; these
are independent of the credits font and Russian/Polish coverage assertions.

The production helper intentionally handles valid BMP UTF8 only. Malformed input,
CP1252, supplementary-plane text and unavailable services retain unsigned byte
semantics. No grapheme shaping or supplementary Unicode support is claimed.
